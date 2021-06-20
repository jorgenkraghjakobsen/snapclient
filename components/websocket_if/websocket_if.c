#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "websocket.h"
#include "websocket_if.h"
#include "websocket_server.h"

#include "protocol.h"

static QueueHandle_t client_queue;
const static int client_queue_size = 10;

int websocket_if_start(void) {
  int ws_res = ws_server_start();
  if (ws_res == 0) {
    printf("Websocket error\n");
  }
  xTaskCreate(&server_task, "server_task", 8 * 1024, NULL, 6, NULL);
  xTaskCreate(&server_handle_task, "server_handle_task", 8 * 1024, NULL, 9,
              NULL);
  return 1;
}

int websocket_if_stop(void) { return 1; }

// Handles websocket events - Pass on to protocol handler using queue
void websocket_callback(uint8_t num, WEBSOCKET_TYPE_t type, char* msg,
                        uint64_t len) {
  const static char* TAG = "websocket_callback";
  int value;

  switch (type) {
    case WEBSOCKET_CONNECT:
      ESP_LOGI(TAG, "client %i connected!", num);
      break;
    case WEBSOCKET_DISCONNECT_EXTERNAL:
      ESP_LOGI(TAG, "client %i sent a disconnect message", num);
      break;
    case WEBSOCKET_DISCONNECT_INTERNAL:
      ESP_LOGI(TAG, "client %i was disconnected", num);
      break;
    case WEBSOCKET_DISCONNECT_ERROR:
      ESP_LOGI(TAG, "client %i was disconnected due to an error", num);
      break;
    case WEBSOCKET_TEXT:
      if (len) {  // if the message length was greater than zero
        switch (msg[0]) {
          case 'L':
            if (sscanf(msg, "L%i", &value)) {
              ESP_LOGI(TAG, "LED value: %i", value);
              ws_server_send_text_all_from_callback(msg, len);  // broadcast it!
            }
            break;
          case 'M':
            ESP_LOGI(TAG, "got message length %i: %s", (int)len - 1, &(msg[1]));
            break;
          default:
            ESP_LOGI(TAG, "got an unknown message with length %i", (int)len);
            break;
        }
      }
      break;
    case WEBSOCKET_BIN: {
      // ESP_LOGI(TAG,"client %i sent binary message of size
      // %i:\n",num,(uint32_t)len);
      uint8_t(*protmsg)[] = malloc(len);
      memcpy(protmsg, msg, len);
      xQueueSendToBack(prot_queue, &protmsg, portMAX_DELAY);
      break;
    }
    case WEBSOCKET_PING:
      ESP_LOGI(TAG, "client %i pinged us with message of size %i:\n%s", num,
               (uint32_t)len, msg);
      break;
    case WEBSOCKET_PONG:
      ESP_LOGI(TAG, "client %i responded to the ping", num);
      break;
  }
}

// serves any clients
void http_serve(struct netconn* conn) {
  const static char* TAG = "http_server";
  struct netbuf* inbuf;
  static char* buf;
  static uint16_t buflen;
  static err_t err;

  netconn_set_recvtimeout(conn,
                          2000);  // allow a connection timeout of 1 second
  ESP_LOGI(TAG, "reading from client...");
  err = netconn_recv(conn, &inbuf);
  ESP_LOGI(TAG, "read from client");
  if (err == ERR_OK) {
    netbuf_data(inbuf, (void**)&buf, &buflen);
    if (buf) {
      if (strstr(buf, "GET / ") && strstr(buf, "Upgrade: websocket")) {
        ESP_LOGI(TAG, "Requesting websocket on /");
        ws_server_add_client(conn, buf, buflen, "/", websocket_callback);
        netbuf_delete(inbuf);
      }

      else {
        ESP_LOGI(TAG, "Unknown request");
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }
    } else {
      ESP_LOGI(TAG, "Unknown request (empty?...)");
      netconn_close(conn);
      netconn_delete(conn);
      netbuf_delete(inbuf);
    }
  } else {  // if err==ERR_OK
    ESP_LOGI(TAG, "error on read, closing connection");
    netconn_close(conn);
    netconn_delete(conn);
    netbuf_delete(inbuf);
  }
}

// handles clients when they first connect. passes to a queue
void server_task(void* pvParameters) {
  const static char* TAG = "server_task";
  struct netconn *conn, *newconn;
  static err_t err;
  client_queue = xQueueCreate(client_queue_size, sizeof(struct netconn*));

  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 8088);
  netconn_listen(conn);
  ESP_LOGI(TAG, "server listening");
  do {
    err = netconn_accept(conn, &newconn);
    ESP_LOGI(TAG, "new client");
    if (err == ERR_OK) {
      xQueueSendToBack(client_queue, &newconn, portMAX_DELAY);
      // http_serve(newconn);
    }
  } while (err == ERR_OK);
  netconn_close(conn);
  netconn_delete(conn);
  ESP_LOGE(TAG, "task ending, rebooting board");
  esp_restart();
}

// receives clients from queue, handles them
void server_handle_task(void* pvParameters) {
  const static char* TAG = "server_handle_task";
  struct netconn* conn;
  ESP_LOGI(TAG, "task starting");
  for (;;) {
    xQueueReceive(client_queue, &conn, portMAX_DELAY);
    if (!conn) continue;
    http_serve(conn);
  }
  vTaskDelete(NULL);
}
