/* Play flac file by audio pipeline
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "wifi_interface.h"

// Minimum ESP-IDF stuff only hardware abstraction stuff
#include "board.h"
#include "es8388.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mdns.h"
#include "net_functions.h"

// Web socket server 
#include "websocket_if.h"
//#include "websocket_server.h"

// Opus decoder is implemented as a subcomponet from master git repo
#include <sys/time.h>

#include "driver/i2s.h"
#include "dsp_processor.h"
#include "opus.h"
#include "ota_server.h"
#include "snapcast.h"

//#include "ma120.h"
void timeravg(struct timeval *tavg,struct timeval *tdif) ;

xTaskHandle t_http_get_task;
xQueueHandle prot_queue;
xQueueHandle flow_queue;
xQueueHandle i2s_queue;
uint32_t buffer_ms = 400;
uint8_t muteCH[4] = {0};
struct timeval tdif,tavg;
audio_board_handle_t board_handle = NULL;

int timeval_subtract(struct timeval *result, struct timeval *x,
                     struct timeval *y);

/* snapast parameters; configurable in menuconfig */
#define SNAPCAST_SERVER_USE_MDNS CONFIG_SNAPSERVER_USE_MDNS
#define SNAPCAST_SERVER_HOST CONFIG_SNAPSERVER_HOST
#define SNAPCAST_SERVER_PORT CONFIG_SNAPSERVER_PORT
#define SNAPCAST_BUFF_LEN CONFIG_SNAPCLIENT_BUFF_LEN
#define SNAPCAST_CLIENT_NAME CONFIG_SNAPCLIENT_NAME

unsigned int addr;
uint32_t port = SNAPCAST_SERVER_PORT;
/* Logging tag */
static const char *TAG = "SNAPCAST";

static char buff[SNAPCAST_BUFF_LEN];

extern char mac_address[18];
extern EventGroupHandle_t s_wifi_event_group;

enum codec_type { PCM, FLAC, OGG, OPUS };

static void http_get_task(void *pvParameters) {
  struct sockaddr_in servaddr;
  char *start;
  int sockfd;

  char base_message_serialized[BASE_MESSAGE_SIZE];
  char *hello_message_serialized;
  int result, size, id_counter;

  uint32_t client_state_muted = 0;
  struct timeval now, ttx, trx, tv1, last_time_sync;

  uint8_t timestampSize[12];

  time_message_t time_message;
  double time_diff;

  last_time_sync.tv_sec = 0;
  last_time_sync.tv_usec = 0;
  id_counter = 0;

  int codec = 0;

  OpusDecoder *decoder = NULL;

  int16_t *audio =
      (int16_t *)malloc(960 * 2 * sizeof(int16_t));  // 960*2: 20ms, 960*1: 10ms

  int16_t pcm_size = 120;
  uint16_t channels;
  uint32_t cnt = 0;
  int chunk_res;
  ESP_LOGI("I2S", "Call dsp setup" );
  dsp_i2s_task_init(44100, false);

  while (1) {
    /* Wait for the callback to set the CONNECTED_BIT in the
       event group.
    */

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true,
                        portMAX_DELAY);

    // configure a failsafe snapserver according to CONFIG values
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, CONFIG_SNAPSERVER_HOST, &(servaddr.sin_addr.s_addr));
    servaddr.sin_port = htons(CONFIG_SNAPSERVER_PORT);

#ifdef CONFIG_SNAPCLIENT_USE_MDNS
    // Find snapcast server using mDNS
    // Connect to first snapcast server found
    ESP_LOGI(TAG, "Enable mdns");
    mdns_init();
    mdns_result_t *r = NULL;
    esp_err_t err = 0;
    while (!r || err) {
      ESP_LOGI(TAG, "Lookup snapcast service on network");
      esp_err_t err = mdns_query_ptr("_snapcast", "_tcp", 3000, 20, &r);
      if (err) {
        ESP_LOGE(TAG, "Query Failed");
      }
      if (!r) {
        ESP_LOGW(TAG, "No results found!");
        break;
      }
      // mdns config failed, wait 1s and try again
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    char ip4[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(r->addr->addr.u_addr.ip4.addr), ip4, INET_ADDRSTRLEN);

    ESP_LOGI(TAG, "Found Snapcast server: %s:%d", ip4, r->port);
    servaddr.sin_addr.s_addr = r->addr->addr.u_addr.ip4.addr;
    servaddr.sin_port = htons(r->port);
    mdns_query_results_free(r);
#endif

    // servaddr is configured, now open a connection
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      ESP_LOGE(TAG, "... Failed to allocate socket.");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }
    ESP_LOGI(TAG, "... allocated socket");

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
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
        SNAPCAST_MESSAGE_HELLO,     // type
        0x0,                        // id
        0x0,                        // refersTo
        {now.tv_sec, now.tv_usec},  // sent
        {0x0, 0x0},                 // received
        0x0,                        // size
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

    hello_message_serialized =
        hello_message_serialize(&hello_message, (size_t *)&(base_message.size));
    if (!hello_message_serialized) {
      ESP_LOGI(TAG, "Failed to serialize hello message\r\b");
      return;
    }

    result = base_message_serialize(&base_message, base_message_serialized,
                                    BASE_MESSAGE_SIZE);
    if (result) {
      ESP_LOGI(TAG, "Failed to serialize base message\r\n");
      return;
    }

    write(sockfd, base_message_serialized, BASE_MESSAGE_SIZE);
    write(sockfd, hello_message_serialized, base_message.size);
    free(hello_message_serialized);

    fd_set read_push_set;
    // struct timeval to;
    int retval;
    for (;;) {
      // Need to have time out if 100 ms between packeage to signal
      size = 0;  // Audio flow stopped
      // Read from socket with time out
      FD_ZERO(&read_push_set);
      FD_SET(sockfd, &read_push_set);

      struct timeval to;
      to.tv_sec = 0;
      to.tv_usec = 300000;
      // Block until input arrives on one or more active sockets.
      retval = select(FD_SETSIZE, &read_push_set, NULL, NULL, &to);
      if (retval) {
        while (size < BASE_MESSAGE_SIZE) {
          result = read(sockfd, &(buff[size]), BASE_MESSAGE_SIZE - size);
          if (result < 0) {
            ESP_LOGI(TAG, "Failed to read from server: %d\r\n", result);
            return;
          }
          size += result;
        }
      } else {
        ESP_LOGI(TAG, "Socket timeout after %d ms\r\n",
                 (uint32_t)to.tv_usec / 1000);
        client_state_muted = 2;
        xQueueSend(flow_queue, &client_state_muted, 10);
        continue;  // Return wait for next socket package
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
      // ESP_LOGI(TAG,"Rx dif : %d %d", base_message.sent.sec,
      // base_message.sent.usec/1000);

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
          result = codec_header_message_deserialize(&codec_header_message,
                                                    start, size);
          if (result) {
            ESP_LOGI(TAG, "Failed to read codec header: %d\r\n", result);
            return;
          }

          ESP_LOGI(TAG, "Received codec header message\r\n");

          size = codec_header_message.size;
          start = codec_header_message.payload;
          if (strcmp(codec_header_message.codec, "opus") == 0) {
            uint32_t rate;
            memcpy(&rate, start + 4, sizeof(rate));
            uint16_t bits;
            memcpy(&bits, start + 8, sizeof(bits));
            memcpy(&channels, start + 10, sizeof(channels));
            ESP_LOGI(TAG, "Opus sampleformat: %d:%d:%d\n", rate, bits,
                     channels);
            int error = 0;
            decoder = opus_decoder_create(rate, channels, &error);
            if (error != 0) {
              ESP_LOGI(TAG, "Failed to init opus coder");
              return;
            }
            codec = OPUS;
            ESP_LOGI(TAG, "Initialized opus Decoder: %d", error);

          } else if (strcmp(codec_header_message.codec, "pcm") == 0) {
            codec = PCM;

          } else {
            ESP_LOGI(TAG, "Codec : %s not supported\n",
                     codec_header_message.codec);
            ESP_LOGI(TAG,
                     "Change encoder codec to opus in /etc/snapserver.conf on "
                     "server\n");
            return;
          }
          ESP_LOGI(TAG, "Codec : %s , Size: %d \n", codec_header_message.codec,
                   size);

          codec_header_message_free(&codec_header_message);
          received_header = true;

          break;

        case SNAPCAST_MESSAGE_WIRE_CHUNK:
          cnt++;
          if (!received_header) {  // Ignore audio packets until codec
                                   // configured
            continue;
          }

          result =
              wire_chunk_message_deserialize(&wire_chunk_message, start, size);
          if (result) {
            ESP_LOGI(TAG, "Failed to read wire chunk: %d\r\n", result);
            return;
          }
          size = wire_chunk_message.size;
          start = (wire_chunk_message.payload);
          int frame_size = 0;
          switch (codec) {
            case OPUS:
              while ((frame_size = opus_decode(decoder, (unsigned char *)start,
                                               size, (opus_int16 *)audio,
                                               pcm_size / channels, 0)) ==
                     OPUS_BUFFER_TOO_SMALL) {
                pcm_size = pcm_size * 2;
                ESP_LOGI(TAG,
                         "OPUS encoding buffer too small, resizing to %d "
                         "samples per channel",
                         pcm_size / channels);
              }
              // ESP_LOGI(TAG, "time stamp in :
              // %d",wire_chunk_message.timestamp.sec);
              if (frame_size < 0) {
                ESP_LOGE(TAG, "Decode error : %d \n", frame_size);
              } else {
                // pack(&timestampSize,wire_chunk_message.timestamp,frame_size*2*size(uint16_t))
                memcpy(timestampSize, &wire_chunk_message.timestamp.sec,
                       sizeof(wire_chunk_message.timestamp.sec));
                memcpy(timestampSize + 4, &wire_chunk_message.timestamp.usec,
                       sizeof(wire_chunk_message.timestamp.usec));
                uint32_t chunk_size = frame_size * 2 * sizeof(uint16_t);
                memcpy(timestampSize + 8, &chunk_size, sizeof(chunk_size));

                // ESP_LOGI(TAG, "Network jitter %d %d",(uint32_t)
                // wire_chunk_message.timestamp.usec/1000,
                //                                          (uint32_t)
                //                                          base_message.sent.usec/1000);

                if ((chunk_res = write_ringbuf(timestampSize,
                                               3 * sizeof(uint32_t))) != 12) {
                  ESP_LOGI(TAG, "Error writing timestamp to ring buffer: %d",
                           chunk_res);
                }
                if ((chunk_res = write_ringbuf((const uint8_t *)audio,
                                               chunk_size)) != chunk_size) {
                  ESP_LOGI(TAG, "Error writing data to ring buffer: %d",
                           chunk_res);
                }
              }
              break;

            case PCM:
              memcpy(timestampSize, &wire_chunk_message.timestamp.sec,
                     sizeof(wire_chunk_message.timestamp.sec));
              memcpy(timestampSize + 4, &wire_chunk_message.timestamp.usec,
                     sizeof(wire_chunk_message.timestamp.usec));
              uint32_t chunk_size = size;
              memcpy(timestampSize + 8, &chunk_size, sizeof(chunk_size));

              // ESP_LOGI(TAG, "Network jitter %d %d",(uint32_t)
              // wire_chunk_message.timestamp.usec/1000,
              //                                          (uint32_t)
              //                                          base_message.sent.usec/1000);

              if ((chunk_res = write_ringbuf(timestampSize,
                                             3 * sizeof(uint32_t))) != 12) {
                ESP_LOGI(TAG, "Error writing timestamp to ring buffer: %d",
                         chunk_res);
              }

              if ((chunk_res = write_ringbuf((const uint8_t *)start, size)) !=
                  size) {
                ESP_LOGE(TAG, "Error writing data to ring buffer: %d",
                         chunk_res);
              }
              break;
          }
          wire_chunk_message_free(&wire_chunk_message);
          break;

        case SNAPCAST_MESSAGE_SERVER_SETTINGS:
          // The first 4 bytes in the buffer are the size of the string.
          // We don't need this, so we'll shift the entire buffer over 4 bytes
          // and use the extra room to add a null character so cJSON can pares
          // it.
          memmove(start, start + 4, size - 4);
          start[size - 3] = '\0';
          result = server_settings_message_deserialize(&server_settings_message,
                                                       start);
          if (result) {
            ESP_LOGI(TAG, "Failed to read server settings: %d\r\n", result);
            return;
          }
          // log mute state, buffer, latency
          buffer_ms = server_settings_message.buffer_ms;
          ESP_LOGI(TAG, "Buffer length:  %d",
                   server_settings_message.buffer_ms);
          ESP_LOGI(TAG, "Ringbuffer size:%d",
                   server_settings_message.buffer_ms * 48 * 4);
          ESP_LOGI(TAG, "Latency:        %d", server_settings_message.latency);
          ESP_LOGI(TAG, "Mute:           %d", server_settings_message.muted);
          ESP_LOGI(TAG, "Setting volume: %d", server_settings_message.volume);
          muteCH[0] = server_settings_message.muted;
          muteCH[1] = server_settings_message.muted;
          muteCH[2] = server_settings_message.muted;
          muteCH[3] = server_settings_message.muted;
          if (server_settings_message.muted != client_state_muted) {
            client_state_muted = server_settings_message.muted;
            xQueueSend(flow_queue, &client_state_muted, 10);
          }
          // Volume setting using ADF HAL abstraction
          audio_hal_set_volume(board_handle->audio_hal,
                               server_settings_message.volume);
          break;

        case SNAPCAST_MESSAGE_TIME:
          result = time_message_deserialize(&time_message, start, size);
          if (result) {
            ESP_LOGI(TAG, "Failed to deserialize time message\r\n");
            return;
          }
          // Calculate TClientDif : Trx-Tsend-Tnetdelay/2
          ttx.tv_sec = base_message.sent.sec;
          ttx.tv_usec = base_message.sent.usec;
          trx.tv_sec = base_message.received.sec;
          trx.tv_usec = base_message.received.usec;

          timersub(&trx, &ttx, &tdif);
          timeravg(&tavg,&tdif);
          ESP_LOGI(TAG, "Tclientdif :% 11ld.%03ld ", tdif.tv_sec , tdif.tv_usec/1000);
          char retbuf[10];
          retbuf[0] = 5; 
          retbuf[1] = 5;
          uint32_t usec = tdif.tv_usec;
          uint32_t uavg = tavg.tv_usec;
          ESP_LOGI(TAG, "Tclientdif : return value %d ",usec); 
          
          retbuf[2] = (usec & 0xff000000) >> 24 ;
          retbuf[3] = (usec & 0x00ff0000) >> 16 ; 
          retbuf[4] = (usec & 0x0000ff00) >> 8 ; 
          retbuf[5] = (usec & 0x000000ff) ; 
          retbuf[6] = (uavg & 0xff000000) >> 24 ;
          retbuf[7] = (uavg & 0x00ff0000) >> 16 ; 
          retbuf[8] = (uavg & 0x0000ff00) >> 8 ; 
          retbuf[9] = (uavg & 0x000000ff) ; 
          ws_server_send_bin_client(0,(char*)retbuf, 10); 
                   
          // ESP_LOGI(TAG, "BaseTX  :% 11d.%03d ", base_message.sent.sec ,
          // base_message.sent.usec/1000); ESP_LOGI(TAG, "BaseRX  :% 11d.%03d ",
          // base_message.received.sec , base_message.received.usec/1000);
          // ESP_LOGI(TAG, "Latency :% 11d.%03d ", time_message.latency.sec,
          // time_message.latency.usec/1000); ESP_LOGI(TAG, "Sub     :% 11d.%03d
          // ", time_message.latency.sec + base_message.received.sec ,
          //                                      time_message.latency.usec/1000
          //                                      +
          //                                      base_message.received.usec/1000);
          time_diff = time_message.latency.usec / 1000 +
                      base_message.received.usec / 1000 -
                      base_message.sent.usec / 1000;
          time_diff = (time_diff > 1000) ? time_diff - 1000 : time_diff;
          ESP_LOGI(TAG, "TM loopback latency: %03.1f ms", time_diff);
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

        result = base_message_serialize(&base_message, base_message_serialized,
                                        BASE_MESSAGE_SIZE);
        if (result) {
          ESP_LOGE(TAG, "Failed to serialize base message for time\r\n");
          continue;
        }

        result = time_message_serialize(&time_message, buff, SNAPCAST_BUFF_LEN);
        if (result) {
          ESP_LOGI(TAG, "Failed to serialize time message\r\b");
          continue;
        }
        // ESP_LOGI(TAG, "---------------------------Write back time message
        // \r\b");
        write(sockfd, base_message_serialized, BASE_MESSAGE_SIZE);
        write(sockfd, buff, TIME_MESSAGE_SIZE);
      }
    }
    ESP_LOGI(TAG, "... done reading from socket\r\n");
    close(sockfd);
  }
}
static uint32_t avg[32];  
static int avgptr = 0; 
static int avgsync = 0; 
void timeravg(struct timeval *tavg,struct timeval *tdif) 
{  ESP_LOGI("TAVG","Time input : % 11lld.%06d",(int64_t) (tdif)->tv_sec,(int32_t) (tdif)->tv_usec );
   if (avgptr < 31 ) { 
     avgptr = avgptr + 1; 
   } else 
   { avgsync = 1;
     avgptr  = 0; 
   }     
   avg[avgptr] = (uint32_t) (tdif)->tv_usec;
   uint32_t avgsum = 0; 
   for (int i = 0; i < 32 ; i++) { 
     avgsum = avgsum + avg[i];  
   } 
   uint32_t avg_32 = ( avgsync == 0 ) ? avgsum/avgptr : avgsum/32; 
   ESP_LOGI("TAVG","Time avg :               %06d  %d ",avg_32, avgsync); 
  
}
void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "Start codec chip");
  board_handle = audio_board_init();
  ESP_LOGI(TAG, "Audio board_init done");

  //audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH,
  //                      AUDIO_HAL_CTRL_START);
  //i2s_mclk_gpio_select(0, 0);
  //setup_ma120(); 

  dsp_setup_flow(500, 44100);

  // Enable and setup WIFI in station mode  and connect to Access point setup in
  // menu config
  wifi_init_sta();
 
  // Enable websocket server  
  ESP_LOGI(TAG, "Connected to AP");
  ESP_LOGI(TAG, "Setup ws server");
  websocket_if_start();
 
  net_mdns_register("snapclient");
#ifdef CONFIG_SNAPCLIENT_SNTP_ENABLE
  set_time_from_sntp();
#endif
  flow_queue = xQueueCreate(10, sizeof(uint32_t));
  xTaskCreate(&ota_server_task, "ota_server_task", 4096, NULL, 15, NULL);

  
  xTaskCreatePinnedToCore(&http_get_task, "http_get_task", 3 * 4096, NULL, 5,
                          &t_http_get_task, 1);
  while (1) {
    // audio_event_iface_msg_t msg;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // ma120_read_error(0x20);

    esp_err_t ret = 0;  // audio_event_iface_listen(evt, &msg, portMAX_DELAY);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
      continue;
    }
  }
}
