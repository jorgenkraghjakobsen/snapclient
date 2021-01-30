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

//ESP-IDF stuff
#include "board.h"
#include "es8388.h"
//#include "audio_hal.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "mdns.h"
#include "opus.h"
#include "driver/i2s.h"
#include "rtprx.h"
#include "MerusAudio.h"
#include "dsp_processor.h"
#include "snapcast.h"

#include <sys/time.h>

xQueueHandle i2s_queue;
uint32_t buffer_ms = 400;
uint8_t  muteCH[4] = {0};
audio_board_handle_t board_handle = NULL;

/* Hardcoded WiFi configuration that you can set via
   'make menuconfig'.
*/
#define ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASSWORD  CONFIG_ESP_WIFI_PASSWORD
#define ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* snapast parameters; configurable in menuconfig */
#define SNAPCAST_SERVER_HOST      CONFIG_SNAPSERVER_HOST
#define SNAPCAST_SERVER_PORT      CONFIG_SNAPSERVER_PORT
#define SNAPCAST_BUFF_LEN         CONFIG_SNAPCLIENT_BUFF_LEN
#define SNAPCAST_CLIENT_NAME      CONFIG_SNAPCLIENT_NAME

unsigned int addr;
uint32_t port = 1704;
/* Logging tag */
static const char *TAG = "SNAPCAST";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
//static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */

static char buff[SNAPCAST_BUFF_LEN];
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
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
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

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASSWORD,
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
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
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASSWORD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
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
    //double time_diff;

    last_time_sync.tv_sec = 0;
    last_time_sync.tv_usec = 0;
    //uint32_t old_usec = 0;
    int32_t diff = 0;
    id_counter = 0;

    OpusDecoder *decoder = NULL;

    //int size = opus_decoder_get_size(2);
    //int oe = 0;
    //decoder = opus_decoder_create(48000,2,&oe);
    //  int error = opus_decoder_init(decoder, 48000, 2);
  	//printf("Initialized Decoder: %d", oe);
	int16_t *audio = (int16_t *)malloc(960*2*sizeof(int16_t)); // 960*2: 20ms, 960*1: 10ms
    int16_t pcm_size = 120;
    uint16_t channels;

    dsp_i2s_task_init(48000, false);

    while(1) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        // Find snapcast server
        // Connect to first snapcast server found
        ESP_LOGI(TAG, "Enable mdns") ;
        mdns_init();
        mdns_result_t * r = NULL;
        esp_err_t err = 0;
        while ( !r || err )
        {  ESP_LOGI(TAG, "Lookup snapcast service on the local network");
           esp_err_t err = mdns_query_ptr("_snapcast", "_tcp", 3000, 20,  &r);
           if(err){
             ESP_LOGE(TAG, "Query Failed");
           }
           if(!r){
             ESP_LOGW(TAG, "No results found!");
           }
		   // mdns config failed, wait 1s and try again
           vTaskDelay(1000/portTICK_PERIOD_MS);
        }

        ESP_LOGI(TAG,"Found %08x", r->addr->addr.u_addr.ip4.addr);

        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = r->addr->addr.u_addr.ip4.addr;
        servaddr.sin_port = htons(r->port);
        mdns_query_results_free(r);

		// servaddr is configured, now open a connection
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
            SNAPCAST_MESSAGE_HELLO,      // type
            0x0,                         // id
            0x0,                         // refersTo
            { now.tv_sec, now.tv_usec }, // sent
            { 0x0, 0x0 },                // received
            0x0,                         // size
        };

        hello_message_t hello_message = {
            mac_address,
            SNAPCAST_CLIENT_NAME,  // hostname
            "0.0.2",               // client version
            "libsnapcast",         // client name
            "esp32",               // os name
            "xtensa",              // arch
            1,                     // instance
            mac_address,           // id
            2,                     // protocol version
        };

        hello_message_serialized = hello_message_serialize(
			&hello_message, (size_t*) &(base_message.size));
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

		// main loop
        for (;;) {
            size = 0;

			// wait for the next message
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
                return;
            }
            diff = (int32_t)(base_message.sent.usec-now.tv_usec)/1000 ;
            if (diff < 0)
            { diff = diff + 1000; }
            //ESP_LOGI(TAG,"%d %d dif %d",base_message.sent.usec/1000,(int)now.tv_usec/1000,
            //                         (int32_t)(base_message.sent.usec-now.tv_usec)/1000 ) ;

            //diff = (uint32_t)now.tv_usec-old_usec;
            //if (diff < 0)
            //{ diff = diff + 1000000; }
            //ESP_LOGI(TAG,"%d %d %d %d",base_message.size, (uint32_t)now.tv_usec, old_usec, diff);
            base_message.received.sec = now.tv_sec;
            base_message.received.usec = now.tv_usec;
            //ESP_LOGI(TAG,"%d %d : %d %d : %d %d",base_message.size, base_message.refersTo,
            //base_message.sent.sec,base_message.sent.usec,
            //base_message.received.sec,base_message.received.usec);

            //old_usec = now.tv_usec;
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
                    result = codec_header_message_deserialize(
						&codec_header_message, start, size);
                    if (result) {
                        ESP_LOGI(TAG, "Failed to read codec header: %d\r\n", result);
                        return;
                    }

                    ESP_LOGI(TAG, "Received codec header message\r\n");

                    size = codec_header_message.size;
                    start = codec_header_message.payload;
                    if (strcmp(codec_header_message.codec, "opus") == 0) {
                       ESP_LOGI(TAG, "Codec : %s , Size: %d \n",
								codec_header_message.codec, size);
                    } else {
                       ESP_LOGI(TAG, "Codec : %s not supported\n",
								codec_header_message.codec);
                       ESP_LOGI(TAG, "Change encoder codec to opus in /etc/snapserver.conf on server\n");
                       return;
                    }
                    uint32_t rate;
                    memcpy(&rate, start+4, sizeof(rate));
                    uint16_t bits;
                    memcpy(&bits, start+8, sizeof(bits));
                    memcpy(&channels, start+10, sizeof(channels));
                    ESP_LOGI(TAG, "Opus sampleformat: %d:%d:%d\n", rate, bits, channels);
                    int error = 0;
                    decoder = opus_decoder_create(rate, channels, &error);
                    if (error != 0) {
						ESP_LOGI(TAG, "Failed to init opus coder");
					}

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
                    start = (wire_chunk_message.payload);
                    //ESP_LOGI(TAG, "size : %d\n",size);

                    int frame_size = 0;
                    while ((frame_size = opus_decode(
								decoder, (unsigned char *) start, size, (opus_int16*) audio,
								pcm_size/channels, 0)) == OPUS_BUFFER_TOO_SMALL)
                    {
						pcm_size = pcm_size * 2;
						ESP_LOGI(TAG, "OPUS encoding buffer too small, resizing to %d samples per channel", pcm_size/channels);
                    }
                    //ESP_LOGI(TAG, "Size in: %d -> %d,%d",size,frame_size, pcm_size);
                    if (frame_size < 0 ) {
						ESP_LOGE(TAG, "Decode error : %d \n",frame_size);
                    } else {
						write_ringbuf((unsigned char*) audio, frame_size*2*sizeof(uint16_t));
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
                    // log mute state, buffer, latency
                    buffer_ms = server_settings_message.buffer_ms;
                    ESP_LOGI(TAG, "Buffer length:  %d", server_settings_message.buffer_ms);
                    ESP_LOGI(TAG, "Ringbuffer size:%d", server_settings_message.buffer_ms*48*4);
                    ESP_LOGI(TAG, "Latency:        %d", server_settings_message.latency);
                    ESP_LOGI(TAG, "Mute:           %d", server_settings_message.muted);
                    ESP_LOGI(TAG, "Setting volume: %d", server_settings_message.volume);
                    muteCH[0] = server_settings_message.muted;
                    muteCH[1] = server_settings_message.muted;
                    muteCH[2] = server_settings_message.muted;
                    muteCH[3] = server_settings_message.muted;

                    // Volume setting using ADF HAL abstraction
                    audio_hal_set_volume(board_handle->audio_hal,server_settings_message.volume);
                    // move this implemntation to a Merus Audio hal
                    //uint8_t cmd[4];
                    //cmd[0] = 128-server_settings_message.volume  ;
                    //cmd[1] = cmd[0];
                    //ma_write(0x20,1,0x0040,cmd,1);
					break;

                case SNAPCAST_MESSAGE_TIME:
                    result = time_message_deserialize(&time_message, start, size);
                    if (result) {
                        ESP_LOGI(TAG, "Failed to deserialize time message\r\n");
                        return;
                    }
                    //ESP_LOGI(TAG, "BaseTX     : %d %d ", base_message.sent.sec , base_message.sent.usec);
                    //ESP_LOGI(TAG, "BaseRX     : %d %d ", base_message.received.sec , base_message.received.usec);
                    //ESP_LOGI(TAG, "baseTX->RX : %d s ", (base_message.received.sec - base_message.sent.sec)/1000);
                    //ESP_LOGI(TAG, "baseTX->RX : %d ms ", (base_message.received.usec - base_message.sent.usec)/1000);
                    //ESP_LOGI(TAG, "Latency : %d.%d ", time_message.latency.sec,  time_message.latency.usec/1000);
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
                    //ESP_LOGI(TAG, "c2s: %ld %ld", tv1.tv_sec, tv1.tv_usec);
                    //ESP_LOGI(TAG, "s2c: %ld %ld", tv2.tv_sec, tv2.tv_usec);
                    //time_diff = (((double)(tv1.tv_sec - tv2.tv_sec) / 2) * 1000) + (((double)(tv1.tv_usec - tv2.tv_usec) / 2) / 1000);
                    //ESP_LOGI(TAG, "Current latency: %fms\r\n", time_diff);
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

                result = time_message_serialize(&time_message, buff, SNAPCAST_BUFF_LEN);
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
    }
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

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    board_handle = audio_board_init();
    audio_hal_ctrl_codec(
		board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    i2s_mclk_gpio_select(0, 0);
    //audio_hal_set_volume(board_handle->audio_hal,40);


    //setup_ma120x0();
    //setup_rtp_i2s();

    wifi_init_sta();

    uint8_t base_mac[6];
    // Get MAC address for WiFi station
    esp_read_mac(base_mac, ESP_MAC_WIFI_STA);
    sprintf(mac_address,
			"%02X:%02X:%02X:%02X:%02X:%02X",
			base_mac[0], base_mac[1], base_mac[2], base_mac[3], base_mac[4], base_mac[5]);

    vTaskDelay(5000/portTICK_PERIOD_MS);

    xTaskCreatePinnedToCore(&http_get_task, "http_get_task", 3*4096, NULL, 5, NULL, 0);
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
