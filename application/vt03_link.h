#ifndef VT03_LINK_H
#define VT03_LINK_H

#include "struct_typedef.h"
#include "remote_control.h"
#include "uart_mode.h"

#if VT03_ENABLE

#define VT03_FRAME_LEN        21u   // SOF(2) + data(17) + CRC16(2)
#define VT03_SOF_1            0xA9u
#define VT03_SOF_2            0x53u
#define VT03_CH_VALUE_OFFSET  1024u

typedef struct
{
    uint8_t mode_sw;
    uint8_t pause;
    uint8_t fn_1;
    uint8_t fn_2;
    uint8_t trigger;
    uint8_t mouse_mid;
} vt03_ext_t;

extern vt03_ext_t vt03_ext;

void vt03_parse_byte(uint8_t byte);
uint8_t vt03_data_is_error(void);

#endif
#endif
