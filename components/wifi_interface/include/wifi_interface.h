#ifndef _WIFI_INTERFACE_H_
#define _WIFI_INTERFACE_H_

#include "esp_event.h"

EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
                                
void wifi_init_sta(void);

#endif  /* _WIFI_INTERFACE_H_ */ 
