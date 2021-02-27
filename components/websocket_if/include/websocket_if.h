#ifndef _WEBSOCKET_IF_H_
#define _WEBSOCKET_IF_H_

#include "websocket_server.h"

int websocket_if_start();
int websocket_if_stop();
void websocket_callback(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len);
void http_serve(struct netconn *conn);
void server_task(void* pvParameters); 
void server_handle_task(void* pvParameters);


#endif // WEBSOCKET_IF_H_