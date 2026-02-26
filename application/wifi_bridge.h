#ifndef WIFI_BRIDGE_H
#define WIFI_BRIDGE_H

#include "struct_typedef.h"

#ifndef WIFI_BRIDGE_ENABLE
#define WIFI_BRIDGE_ENABLE 1
#endif

#if WIFI_BRIDGE_ENABLE
uint16_t uart1_rx_available(void);
uint8_t uart1_rx_read_byte(void);
#endif

#endif
