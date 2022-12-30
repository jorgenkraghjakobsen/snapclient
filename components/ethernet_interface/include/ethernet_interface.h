/*
 * ethernet_interface.h
 *
 *  Created on: 06.12.2022
 *      Author: bg
 */

#ifndef ETHERNET_INTERFACE_H_
#define ETHERNET_INTERFACE_H_

#include "freertos/event_groups.h"

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define NETWORK_CONNECTED_BIT BIT0
#define NETWORK_FAIL_BIT BIT1

void ethernet_interface_init(void);

#endif /* ETHERNET_INTERFACE_H_ */
