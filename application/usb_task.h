/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       usb_task.c/h
  * @brief      usb debug telemetry output (FireWater format for VOFA+)
  * @note
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. done
  *  V1.1.0     Feb-20-2026     Codex           1. add reusable usb telemetry framework
  *  V1.2.0     Feb-20-2026     Codex           1. switch to FireWater fixed-frame output
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#ifndef USB_TASK_H
#define USB_TASK_H
#include "struct_typedef.h"

#define USB_DEBUG_OUTPUT_ENABLE 1

#define USB_DEBUG_TASK_PERIOD_MS  5u
#define USB_DEBUG_FRAME_PERIOD_MS 20u

typedef enum
{
    USB_DBG_CH_HEALTH  = (1u << 0),
    USB_DBG_CH_GIMBAL  = (1u << 1),
    USB_DBG_CH_CHASSIS = (1u << 2),
    USB_DBG_CH_RC      = (1u << 3),
    USB_DBG_CH_EVENT   = (1u << 4),
} usb_dbg_channel_bit_e;

#define USB_DEBUG_CHANNEL_MASK_DEFAULT (USB_DBG_CH_HEALTH | USB_DBG_CH_GIMBAL | USB_DBG_CH_CHASSIS | USB_DBG_CH_RC | USB_DBG_CH_EVENT)
#define USB_DEBUG_CHANNEL_MASK_ALL     (USB_DBG_CH_HEALTH | USB_DBG_CH_GIMBAL | USB_DBG_CH_CHASSIS | USB_DBG_CH_RC | USB_DBG_CH_EVENT)

extern void usb_task(void const *argument);
extern void usb_debug_set_channel_mask(uint16_t channel_mask);
extern uint16_t usb_debug_get_channel_mask(void);

#endif
