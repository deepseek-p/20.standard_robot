/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       usb_task.c/h
  * @brief      usb debug telemetry output (FireWater format for VOFA+)
  * @note
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Nov-11-2019     RM              1. done
  *  V1.1.0     Feb-20-2026     Codex           1. add reusable usb telemetry framework
  *  V1.2.0     Feb-20-2026     Codex           1. switch to FireWater fixed-frame output
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#include "usb_task.h"

#include "cmsis_os.h"

#include "usb_device.h"
#include "usbd_cdc_if.h"

#include <stdio.h>

#include "detect_task.h"
#include "voltage_task.h"
#include "remote_control.h"
#include "gimbal_task.h"
#include "chassis_task.h"

#define USB_DEBUG_FRAME_MAX_LEN 1024u
#define USB_DEBUG_RAD_SCALE     1000.0f
#define USB_DEBUG_INVALID_INT   2147483647L

/*
 * FireWater fixed field order (index -> value):
 * 0:tick_ms 1:seq 2:drop 3:bat 4:dbus 5:yaw_toe 6:pitch_toe 7:gyro_toe 8:acc_toe 9:mag_toe 10:ref_toe
 * 11:ch_mode 12:vx_set 13:vy_set 14:wz_set 15:rel 16:rel_set 17:ch_yaw 18:ch_yaw_set 19:i1 20:i2 21:i3 22:i4
 * 23:yaw_mode 24:pitch_mode 25:cali_step 26:yaw_abs 27:yaw_abs_set 28:yaw_rel 29:yaw_rel_set 30:yaw_gyro 31:yaw_gyro_set 32:yaw_cur
 * 33:pitch_abs 34:pitch_abs_set 35:pitch_rel 36:pitch_rel_set 37:pitch_gyro 38:pitch_gyro_set 39:pitch_cur
 * 40:rc_ch0 41:rc_ch1 42:rc_ch2 43:rc_ch3 44:rc_s0 45:rc_s1 46:mouse_x 47:mouse_y 48:key_v
 * 49:event_bits 50:gimbal_ok 51:chassis_ok
 * Angle/speed fields use milli-scale (rad*1000, rad/s*1000).
 */

static uint8_t usb_buf[USB_DEBUG_FRAME_MAX_LEN];
static const error_t *error_list_usb_local;

static uint32_t usb_debug_seq = 0;
static uint32_t usb_debug_drop_cnt = 0;
static uint16_t usb_debug_channel_mask = USB_DEBUG_CHANNEL_MASK_DEFAULT;

static uint8_t usb_last_dbus_error = 0xFFu;
static uint8_t usb_last_yaw_motor_error = 0xFFu;
static uint8_t usb_last_pitch_motor_error = 0xFFu;
static uint8_t usb_last_chassis_mode = 0xFFu;
static uint8_t usb_last_yaw_mode = 0xFFu;
static uint8_t usb_last_pitch_mode = 0xFFu;

static uint32_t usb_debug_now_ms(void);
static uint32_t usb_debug_next_seq(void);
static int32_t usb_debug_fp32_to_milli(fp32 value);
static int32_t usb_debug_masked_u8(uint16_t bit, uint8_t value);
static int32_t usb_debug_masked_i16(uint16_t bit, int16_t value);
static int32_t usb_debug_masked_i32(uint16_t bit, int32_t value);
static int32_t usb_debug_masked_fp32_milli(uint16_t bit, fp32 value);
static bool_t usb_emit_firewater_frame(uint32_t now_ms);

void usb_debug_set_channel_mask(uint16_t channel_mask)
{
    usb_debug_channel_mask = channel_mask;
}

uint16_t usb_debug_get_channel_mask(void)
{
    return usb_debug_channel_mask;
}

void usb_task(void const *argument)
{
    uint32_t last_frame_ms = 0;

    (void)argument;

    MX_USB_DEVICE_Init();
    error_list_usb_local = get_error_list_point();

    while (1)
    {
        uint32_t now_ms = usb_debug_now_ms();

#if USB_DEBUG_OUTPUT_ENABLE
        if (now_ms - last_frame_ms >= USB_DEBUG_FRAME_PERIOD_MS)
        {
            usb_emit_firewater_frame(now_ms);
            last_frame_ms = now_ms;
        }
#endif

        osDelay(USB_DEBUG_TASK_PERIOD_MS);
    }
}

static uint32_t usb_debug_now_ms(void)
{
    return osKernelSysTick();
}

static uint32_t usb_debug_next_seq(void)
{
    return ++usb_debug_seq;
}

static int32_t usb_debug_fp32_to_milli(fp32 value)
{
    return (int32_t)(value * USB_DEBUG_RAD_SCALE);
}

static int32_t usb_debug_masked_u8(uint16_t bit, uint8_t value)
{
    if ((usb_debug_channel_mask & bit) == 0u)
    {
        return USB_DEBUG_INVALID_INT;
    }
    return (int32_t)value;
}

static int32_t usb_debug_masked_i16(uint16_t bit, int16_t value)
{
    if ((usb_debug_channel_mask & bit) == 0u)
    {
        return USB_DEBUG_INVALID_INT;
    }
    return (int32_t)value;
}

static int32_t usb_debug_masked_i32(uint16_t bit, int32_t value)
{
    if ((usb_debug_channel_mask & bit) == 0u)
    {
        return USB_DEBUG_INVALID_INT;
    }
    return value;
}

static int32_t usb_debug_masked_fp32_milli(uint16_t bit, fp32 value)
{
    if ((usb_debug_channel_mask & bit) == 0u)
    {
        return USB_DEBUG_INVALID_INT;
    }
    return usb_debug_fp32_to_milli(value);
}

static bool_t usb_emit_firewater_frame(uint32_t now_ms)
{
    int len;
    uint32_t seq;
    uint32_t event_bits = 0;
    uint8_t dbus_error;
    uint8_t yaw_error;
    uint8_t pitch_error;
    uint8_t gyro_error;
    uint8_t accel_error;
    uint8_t mag_error;
    uint8_t referee_error;
    uint8_t gimbal_ok;
    uint8_t chassis_ok;

    gimbal_debug_snapshot_t gimbal_snapshot;
    chassis_debug_snapshot_t chassis_snapshot;
    const RC_ctrl_t *rc_ctrl;

    dbus_error = error_list_usb_local[DBUS_TOE].error_exist;
    yaw_error = error_list_usb_local[YAW_GIMBAL_MOTOR_TOE].error_exist;
    pitch_error = error_list_usb_local[PITCH_GIMBAL_MOTOR_TOE].error_exist;
    gyro_error = error_list_usb_local[BOARD_GYRO_TOE].error_exist;
    accel_error = error_list_usb_local[BOARD_ACCEL_TOE].error_exist;
    mag_error = error_list_usb_local[BOARD_MAG_TOE].error_exist;
    referee_error = error_list_usb_local[REFEREE_TOE].error_exist;

    gimbal_ok = get_gimbal_debug_snapshot(&gimbal_snapshot);
    chassis_ok = get_chassis_debug_snapshot(&chassis_snapshot);
    rc_ctrl = get_remote_control_point();

    if (dbus_error != usb_last_dbus_error)
    {
        event_bits |= (1u << 0);
    }
    if (yaw_error != usb_last_yaw_motor_error)
    {
        event_bits |= (1u << 1);
    }
    if (pitch_error != usb_last_pitch_motor_error)
    {
        event_bits |= (1u << 2);
    }

    if (chassis_ok)
    {
        if ((uint8_t)chassis_snapshot.mode != usb_last_chassis_mode)
        {
            event_bits |= (1u << 3);
        }
    }
    else
    {
        event_bits |= (1u << 6);
    }

    if (gimbal_ok)
    {
        if ((uint8_t)gimbal_snapshot.yaw_mode != usb_last_yaw_mode)
        {
            event_bits |= (1u << 4);
        }
        if ((uint8_t)gimbal_snapshot.pitch_mode != usb_last_pitch_mode)
        {
            event_bits |= (1u << 5);
        }
    }
    else
    {
        event_bits |= (1u << 7);
    }

    usb_last_dbus_error = dbus_error;
    usb_last_yaw_motor_error = yaw_error;
    usb_last_pitch_motor_error = pitch_error;
    if (chassis_ok)
    {
        usb_last_chassis_mode = (uint8_t)chassis_snapshot.mode;
    }
    if (gimbal_ok)
    {
        usb_last_yaw_mode = (uint8_t)gimbal_snapshot.yaw_mode;
        usb_last_pitch_mode = (uint8_t)gimbal_snapshot.pitch_mode;
    }

    seq = usb_debug_next_seq();

    len = snprintf((char *)usb_buf, USB_DEBUG_FRAME_MAX_LEN,
                   "%lu,%lu,%lu,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\r\n",
                   now_ms,
                   seq,
                   usb_debug_drop_cnt,
                   usb_debug_masked_i32(USB_DBG_CH_HEALTH, get_battery_percentage()),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, dbus_error),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, yaw_error),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, pitch_error),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, gyro_error),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, accel_error),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, mag_error),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, referee_error),
                   usb_debug_masked_i32(USB_DBG_CH_CHASSIS, chassis_ok ? (int32_t)chassis_snapshot.mode : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.vx_set : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.vy_set : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.wz_set : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.chassis_relative_angle : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.chassis_relative_angle_set : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.chassis_yaw : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.chassis_yaw_set : (fp32)0.0f),
                   usb_debug_masked_i16(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.wheel_current_set[0] : (int16_t)0),
                   usb_debug_masked_i16(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.wheel_current_set[1] : (int16_t)0),
                   usb_debug_masked_i16(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.wheel_current_set[2] : (int16_t)0),
                   usb_debug_masked_i16(USB_DBG_CH_CHASSIS, chassis_ok ? chassis_snapshot.wheel_current_set[3] : (int16_t)0),
                   usb_debug_masked_i32(USB_DBG_CH_GIMBAL, gimbal_ok ? (int32_t)gimbal_snapshot.yaw_mode : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_GIMBAL, gimbal_ok ? (int32_t)gimbal_snapshot.pitch_mode : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_GIMBAL, gimbal_ok ? (int32_t)gimbal_snapshot.cali_step : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.yaw_absolute : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.yaw_absolute_set : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.yaw_relative : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.yaw_relative_set : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.yaw_gyro : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.yaw_gyro_set : (fp32)0.0f),
                   usb_debug_masked_i16(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.yaw_given_current : (int16_t)0),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.pitch_absolute : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.pitch_absolute_set : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.pitch_relative : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.pitch_relative_set : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.pitch_gyro : (fp32)0.0f),
                   usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.pitch_gyro_set : (fp32)0.0f),
                   usb_debug_masked_i16(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.pitch_given_current : (int16_t)0),
                   usb_debug_masked_i32(USB_DBG_CH_RC, rc_ctrl != NULL ? (int32_t)rc_ctrl->rc.ch[0] : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_RC, rc_ctrl != NULL ? (int32_t)rc_ctrl->rc.ch[1] : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_RC, rc_ctrl != NULL ? (int32_t)rc_ctrl->rc.ch[2] : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_RC, rc_ctrl != NULL ? (int32_t)rc_ctrl->rc.ch[3] : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_RC, rc_ctrl != NULL ? (int32_t)rc_ctrl->rc.s[0] : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_RC, rc_ctrl != NULL ? (int32_t)rc_ctrl->rc.s[1] : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_RC, rc_ctrl != NULL ? (int32_t)rc_ctrl->mouse.x : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_RC, rc_ctrl != NULL ? (int32_t)rc_ctrl->mouse.y : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_RC, rc_ctrl != NULL ? (int32_t)rc_ctrl->key.v : USB_DEBUG_INVALID_INT),
                   usb_debug_masked_i32(USB_DBG_CH_EVENT, (int32_t)event_bits),
                   usb_debug_masked_i32(USB_DBG_CH_EVENT, (int32_t)gimbal_ok),
                   usb_debug_masked_i32(USB_DBG_CH_EVENT, (int32_t)chassis_ok));

    if (len <= 0)
    {
        return 0;
    }

    if (len >= (int)USB_DEBUG_FRAME_MAX_LEN)
    {
        usb_debug_drop_cnt++;
        return 0;
    }

    if (CDC_Transmit_FS(usb_buf, (uint16_t)len) == 0)
    {
        return 1;
    }

    usb_debug_drop_cnt++;
    return 0;
}
