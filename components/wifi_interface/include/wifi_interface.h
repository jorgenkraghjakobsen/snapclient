#ifndef _WIFI_INTERFACE_H_
#define _WIFI_INTERFACE_H_

#include "freertos/event_groups.h"

// use wifi provisioning
#define ENABLE_WIFI_PROVISIONING CONFIG_ENABLE_WIFI_PROVISIONING

/* Hardcoded WiFi configuration that you can set via
   'make menuconfig'.
*/
#if !ENABLE_WIFI_PROVISIONING
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD
#endif

#define WIFI_MAXIMUM_RETRY CONFIG_WIFI_MAXIMUM_RETRY

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

void wifi_init(void);

#endif /* _WIFI_INTERFACE_H_ */
