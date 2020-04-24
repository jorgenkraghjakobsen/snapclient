/* Play flac file by audio pipeline
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"


#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_sntp.h"
#include "opus.h"
#include "driver/i2s.h"
#include "rtprx.h"
#include "MerusAudio.h"
#include "dsp_processor.h"
#include "snapcast.h"

#include <sys/time.h>

xQueueHandle i2s_queue;

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

/* Constants that aren't configurable in menuconfig */
#define HOST "192.168.1.158"
#define PORT 1704
#define BUFF_LEN 4000

/* Logging tag */
static const char *TAG = "SNAPCAST";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
//static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */

static char buff[BUFF_LEN];
//static audio_element_handle_t snapcast_stream;
static char mac_address[18];


static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "drk-kontor",
            .password = "12341234"
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:kontor password:1234");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:kontor, password:1234...");
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    //ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    //ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    //vEventGroupDelete(s_wifi_event_group);
}

static void http_get_task(void *pvParameters)
{
    struct sockaddr_in servaddr;
    char *start;
    int sockfd;
    char base_message_serialized[BASE_MESSAGE_SIZE];
    char *hello_message_serialized;
    int result, size, id_counter;
    struct timeval now, tv1, tv2, tv3, last_time_sync;
    time_message_t time_message;
    double time_diff;

    last_time_sync.tv_sec = 0;
    last_time_sync.tv_usec = 0;
    id_counter = 0;

    OpusDecoder *decoder;

    //int size = opus_decoder_get_size(2);
    int oe = 0;
    decoder = opus_decoder_create(48000,2,&oe);
    //  int error = opus_decoder_init(decoder, 48000, 2);
  	//printf("Initialized Decoder: %d", oe);
	  int16_t *audio = (int16_t *)malloc(960*1*sizeof(int16_t));
    int16_t pcm_size = 120;
    uint16_t channels;

    dsp_i2s_task_init(48000);

    while(1) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(HOST);
        servaddr.sin_port = htons(PORT);

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(sockfd);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");

        codec_header_message_t codec_header_message;
        wire_chunk_message_t wire_chunk_message;
        server_settings_message_t server_settings_message;

        result = gettimeofday(&now, NULL);
        if (result) {
            ESP_LOGI(TAG, "Failed to gettimeofday\r\n");
            return;
        }

        bool received_header = false;
        base_message_t base_message = {
            SNAPCAST_MESSAGE_HELLO,
            0x0,
            0x0,
            { now.tv_sec, now.tv_usec },
            { 0x0, 0x0 },
            0x0,
        };

        hello_message_t hello_message = {
            mac_address,
            "ESP32-Caster",
            "0.0.2",
            "libsnapcast",
            "esp32",
            "xtensa",
            1,
            mac_address,
            2,
        };

        hello_message_serialized = hello_message_serialize(&hello_message, (size_t*) &(base_message.size));
        if (!hello_message_serialized) {
            ESP_LOGI(TAG, "Failed to serialize hello message\r\b");
            return;
        }

        result = base_message_serialize(
            &base_message,
            base_message_serialized,
            BASE_MESSAGE_SIZE
        );
        if (result) {
            ESP_LOGI(TAG, "Failed to serialize base message\r\n");
            return;
        }

        write(sockfd, base_message_serialized, BASE_MESSAGE_SIZE);
        write(sockfd, hello_message_serialized, base_message.size);
        free(hello_message_serialized);

        for (;;) {
            size = 0;
            while (size < BASE_MESSAGE_SIZE) {
                result = read(sockfd, &(buff[size]), BASE_MESSAGE_SIZE - size);
                if (result < 0) {
                    ESP_LOGI(TAG, "Failed to read from server: %d\r\n", result);
                    return;
                }

                size += result;
            }

            result = gettimeofday(&now, NULL);
            if (result) {
                ESP_LOGI(TAG, "Failed to gettimeofday\r\n");
                return;
            }

            result = base_message_deserialize(&base_message, buff, size);
            if (result) {
                ESP_LOGI(TAG, "Failed to read base message: %d\r\n", result);
                // TODO there should be a big circular buffer or something for this
                return;
            }

            base_message.received.sec = now.tv_sec;
            base_message.received.usec = now.tv_usec;

            start = buff;
            size = 0;
            while (size < base_message.size) {
                result = read(sockfd, &(buff[size]), base_message.size - size);
                if (result < 0) {
                    ESP_LOGI(TAG, "Failed to read from server: %d\r\n", result);
                    return;
                }

                size += result;
            }

            switch (base_message.type) {
                case SNAPCAST_MESSAGE_CODEC_HEADER:
                    result = codec_header_message_deserialize(&codec_header_message, start, size);
                    if (result) {
                        ESP_LOGI(TAG, "Failed to read codec header: %d\r\n", result);
                        return;
                    }

                    ESP_LOGI(TAG, "Received codec header message\r\n");

                    size = codec_header_message.size;
                    start = codec_header_message.payload;
                    ESP_LOGI(TAG, "Codec : %s , Size: %d \n",codec_header_message.codec,size);

                    uint32_t rate;
                    memcpy(&rate, start+4,sizeof(rate));
                    uint16_t bits;
                    memcpy(&bits, start+8,sizeof(bits));
                    //uint16_t channels;
                    memcpy(&channels, start+10,sizeof(channels));
                    ESP_LOGI(TAG, "Opus sampleformat: %d:%d:%d\n",rate,bits,channels);
                    int error = 0;
                    decoder = opus_decoder_create(rate,channels,&error);
                    if (error != 0)
                    { ESP_LOGI(TAG, "Failed to init opus coder"); }
                    ESP_LOGI(TAG, "Initialized opus Decoder: %d", error);

                    codec_header_message_free(&codec_header_message);
                    received_header = true;

                break;

                case SNAPCAST_MESSAGE_WIRE_CHUNK:
                    if (!received_header) {
                        continue;
                    }

                    result = wire_chunk_message_deserialize(&wire_chunk_message, start, size);
                    if (result) {
                        ESP_LOGI(TAG, "Failed to read wire chunk: %d\r\n", result);
                        return;
                    }

                    //ESP_LOGI(TAG, "Received wire message\r\n");
                    size = wire_chunk_message.size;
                    start = wire_chunk_message.payload;
                    //ESP_LOGI(TAG, "size : %d\n",size);

                    int frame_size = 0;
                    while ((frame_size = opus_decode(decoder, (unsigned char *)start, size, (opus_int16*)audio,
                                                     pcm_size/channels, 0)) == OPUS_BUFFER_TOO_SMALL)
                    {  pcm_size = pcm_size * 2;
                       ESP_LOGI(TAG, "OPUS encoding buffer too small, resizing to %d samples per channel", pcm_size/channels);
                    }
                    if (frame_size < 0 )
                    { ESP_LOGE(TAG, "Decode error : %d \n",frame_size);
                    } else
                    {
                      write_ringbuf(audio,size*4*sizeof(uint16_t));
                    }
                    wire_chunk_message_free(&wire_chunk_message);
                break;

                case SNAPCAST_MESSAGE_SERVER_SETTINGS:
                    // The first 4 bytes in the buffer are the size of the string.
                    // We don't need this, so we'll shift the entire buffer over 4 bytes
                    // and use the extra room to add a null character so cJSON can pares it.
                    memmove(start, start + 4, size - 4);
                    start[size - 3] = '\0';
                    result = server_settings_message_deserialize(&server_settings_message, start);
                    if (result) {
                        ESP_LOGI(TAG, "Failed to read server settings: %d\r\n", result);
                        return;
                    }

                    ESP_LOGI(TAG, "Setting volume: %d", server_settings_message.volume);
                    uint8_t cmd[4];
                    cmd[0] = 128-server_settings_message.volume  ;
                    cmd[1] = cmd[0];
                    ma_write(0x20,1,0x0040,cmd,1);
                break;

                case SNAPCAST_MESSAGE_TIME:
                    result = time_message_deserialize(&time_message, start, size);
                    if (result) {
                        ESP_LOGI(TAG, "Failed to deserialize time message\r\n");
                        return;
                    }

                    // tv == server to client latency (s2c)
                    // time_message.latency == client to server latency(c2s)
                    // TODO the fact that I have to do this simple conversion means
                    // I should probably use the timeval struct instead of my own
                    tv1.tv_sec = base_message.received.sec;
                    tv1.tv_usec = base_message.received.usec;
                    tv3.tv_sec = base_message.sent.sec;
                    tv3.tv_usec = base_message.sent.usec;
                    timersub(&tv1, &tv3, &tv2);
                    tv1.tv_sec = time_message.latency.sec;
                    tv1.tv_usec = time_message.latency.usec;

                    // tv1 == c2s: client to server
                    // tv2 == s2c: server to client
                    ESP_LOGI(TAG, "c2s: %ld %ld\r\n", tv1.tv_sec, tv1.tv_usec);
                    ESP_LOGI(TAG, "s2c: %ld %ld\r\n", tv2.tv_sec, tv2.tv_usec);
                    time_diff = (((double)(tv1.tv_sec - tv2.tv_sec) / 2) * 1000) + (((double)(tv1.tv_usec - tv2.tv_usec) / 2) / 1000);
                    ESP_LOGI(TAG, "Current latency: %fms\r\n", time_diff);
                break;
            }
            // If it's been a second or longer since our last time message was
            // sent, do so now
            result = gettimeofday(&now, NULL);
            if (result) {
                ESP_LOGI(TAG, "Failed to gettimeofday\r\n");
                return;
            }
            timersub(&now, &last_time_sync, &tv1);
            if (tv1.tv_sec >= 1) {
                last_time_sync.tv_sec = now.tv_sec;
                last_time_sync.tv_usec = now.tv_usec;

                base_message.type = SNAPCAST_MESSAGE_TIME;
                base_message.id = id_counter++;
                base_message.refersTo = 0;
                base_message.received.sec = 0;
                base_message.received.usec = 0;
                base_message.sent.sec = now.tv_sec;
                base_message.sent.usec = now.tv_usec;
                base_message.size = TIME_MESSAGE_SIZE;

                result = base_message_serialize(
                    &base_message,
                    base_message_serialized,
                    BASE_MESSAGE_SIZE
                );
                if (result) {
                    ESP_LOGE(TAG, "Failed to serialize base message for time\r\n");
                    continue;
                }

                result = time_message_serialize(&time_message, buff, BUFF_LEN);
                if (result) {
                    ESP_LOGI(TAG, "Failed to serialize time message\r\b");
                    continue;
                }

                write(sockfd, base_message_serialized, BASE_MESSAGE_SIZE);
                write(sockfd, buff, TIME_MESSAGE_SIZE);
            }
        }
        ESP_LOGI(TAG, "... done reading from socket\r\n");
        close(sockfd);
        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d... ", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
}

void set_time_from_sntp() {
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
	  sntp_setservername(1, "europe.pool.ntp.org");
	  sntp_setservername(2, "uk.pool.ntp.org ");
	  sntp_setservername(3, "us.pool.ntp.org");
	  sntp_init();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    char strftime_buf[64];

    // Set timezone to Eastern Standard Time and print local time
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in UTC is: %s", strftime_buf);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    //setup_ma120();
    //ma120_setup_audio(0x20);

    setup_ma120x0();

    //setup_rtp_i2s();

    wifi_init_sta();

    uint8_t base_mac[6];
    // Get MAC address for WiFi station
    esp_read_mac(base_mac, ESP_MAC_WIFI_STA);
    sprintf(mac_address, "%02X:%02X:%02X:%02X:%02X:%02X", base_mac[0], base_mac[1], base_mac[2], base_mac[3], base_mac[4], base_mac[5]);

    vTaskDelay(5000/portTICK_PERIOD_MS);
    printf("Settime from sntp\n");
    set_time_from_sntp();
    printf("Called\n");

    xTaskCreatePinnedToCore(&http_get_task, "http_get_task", 2*4096, NULL, 8, NULL, 1);
    printf("Called http\n");
    while (1) {
        //audio_event_iface_msg_t msg;
        vTaskDelay(2000/portTICK_PERIOD_MS);

        esp_err_t ret = 0; //audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

       }

}
