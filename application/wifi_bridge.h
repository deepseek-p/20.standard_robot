#ifndef WIFI_BRIDGE_H
#define WIFI_BRIDGE_H

#include "struct_typedef.h"
#include "usb_task.h"
#include "uart_mode.h"

#ifndef WIFI_BRIDGE_ENABLE
#if USART1_WIFI && (TELEM_OUTPUT_MODE == TELEM_MODE_WIFI)
#define WIFI_BRIDGE_ENABLE  1
#else
#define WIFI_BRIDGE_ENABLE  0
#endif
#endif

#if WIFI_BRIDGE_ENABLE
uint16_t uart1_rx_available(void);
uint8_t uart1_rx_read_byte(void);
#endif

#endif
