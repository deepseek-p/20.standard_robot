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
 * FireWater fixed field order for VOFA+ (index:name -> meaning):
 *  0:tick_ms       -> os tick timestamp (ms)
 *  1:seq           -> telemetry frame sequence
 *  2:drop          -> dropped frame counter
 *  3:bat_pct       -> battery percentage
 *  4:dbus_toe      -> DBUS offline/error flag
 *  5:yaw_toe       -> yaw motor offline/error flag
 *  6:pitch_toe     -> pitch motor offline/error flag
 *  7:gyro_toe      -> board gyro offline/error flag
 *  8:accel_toe     -> board accel offline/error flag
 *  9:mag_toe       -> board mag offline/error flag
 * 10:ref_toe       -> referee offline/error flag
 *
 * 11:ch_mode       -> chassis mode enum snapshot
 * 12:vx_set        -> chassis vx setpoint (milli-scale)
 * 13:vy_set        -> chassis vy setpoint (milli-scale)
 * 14:wz_set        -> chassis wz setpoint (milli-scale)
 * 15:rel           -> chassis relative angle (milli-scale)
 * 16:rel_set       -> chassis relative angle setpoint (milli-scale)
 * 17:ch_yaw        -> chassis yaw (milli-scale)
 * 18:ch_yaw_set    -> chassis yaw setpoint (milli-scale)
 * 19:i1            -> wheel current set[0]
 * 20:i2            -> wheel current set[1]
 * 21:i3            -> wheel current set[2]
 * 22:i4            -> wheel current set[3]
 *
 * 23:yaw_mode      -> gimbal yaw mode enum
 * 24:pitch_mode    -> gimbal pitch mode enum
 * 25:cali_step     -> gimbal calibration step
 * 26:yaw_abs       -> gimbal yaw absolute angle (milli-scale)
 * 27:yaw_abs_set   -> gimbal yaw absolute setpoint (milli-scale)
 * 28:yaw_rel       -> gimbal yaw relative angle (milli-scale)
 * 29:yaw_rel_set   -> gimbal yaw relative setpoint (milli-scale)
 * 30:yaw_gyro      -> gimbal yaw gyro feedback (milli-scale)
 * 31:yaw_gyro_set  -> gimbal yaw gyro setpoint (milli-scale)
 * 32:yaw_cur       -> gimbal yaw given current
 * 33:pitch_abs     -> gimbal pitch absolute angle (milli-scale)
 * 34:pitch_abs_set -> gimbal pitch absolute setpoint (milli-scale)
 * 35:pitch_rel     -> gimbal pitch relative angle (milli-scale)
 * 36:pitch_rel_set -> gimbal pitch relative setpoint (milli-scale)
 * 37:pitch_gyro    -> gimbal pitch gyro feedback (milli-scale)
 * 38:pitch_gyro_set-> gimbal pitch gyro setpoint (milli-scale)
 * 39:pitch_cur     -> gimbal pitch given current
 *
 * 40:rc_ch0        -> RC channel 0
 * 41:rc_ch1        -> RC channel 1
 * 42:rc_ch2        -> RC channel 2
 * 43:rc_ch3        -> RC channel 3
 * 44:rc_s0         -> RC switch s0
 * 45:rc_s1         -> RC switch s1
 * 46:mouse_x       -> mouse x
 * 47:mouse_y       -> mouse y
 * 48:key_v         -> keyboard bitmask
 *
 * 49:event_bits    -> event bitmap
 *                     bit0:dbus_toe change, bit1:yaw_toe change, bit2:pitch_toe change
 *                     bit3:ch_mode change, bit4:yaw_mode change, bit5:pitch_mode change
 *                     bit6:chassis snapshot invalid, bit7:gimbal snapshot invalid
 * 50:gimbal_ok     -> gimbal snapshot valid flag
 * 51:chassis_ok    -> chassis snapshot valid flag
 *
 * Masked-off channels output USB_DEBUG_INVALID_INT.
 *
 * 中文释义（按列号）:
 *  0 tick_ms: 系统tick时间戳（毫秒）
 *  1 seq: 遥测帧序号
 *  2 drop: USB发送丢帧计数
 *  3 bat_pct: 电池电量百分比
 *  4 dbus_toe: 遥控DBUS离线/错误标志
 *  5 yaw_toe: 云台yaw电机离线/错误标志
 *  6 pitch_toe: 云台pitch电机离线/错误标志
 *  7 gyro_toe: 板载陀螺仪离线/错误标志
 *  8 accel_toe: 板载加速度计离线/错误标志
 *  9 mag_toe: 板载磁力计离线/错误标志
 * 10 ref_toe: 裁判系统离线/错误标志
 *
 * 11 ch_mode: 底盘模式枚举快照
 * 12 vx_set: 底盘x向速度设定（milli缩放）
 * 13 vy_set: 底盘y向速度设定（milli缩放）
 * 14 wz_set: 底盘旋转速度设定（milli缩放）
 * 15 rel: 底盘-云台相对角（milli缩放）
 * 16 rel_set: 底盘-云台相对角目标（milli缩放）
 * 17 ch_yaw: 底盘yaw角（milli缩放）
 * 18 ch_yaw_set: 底盘yaw目标（milli缩放）
 * 19 i1: 底盘轮1电流设定
 * 20 i2: 底盘轮2电流设定
 * 21 i3: 底盘轮3电流设定
 * 22 i4: 底盘轮4电流设定
 *
 * 23 yaw_mode: 云台yaw控制模式枚举
 * 24 pitch_mode: 云台pitch控制模式枚举
 * 25 cali_step: 云台校准步骤
 * 26 yaw_abs: 云台yaw绝对角（milli缩放）
 * 27 yaw_abs_set: 云台yaw绝对角目标（milli缩放）
 * 28 yaw_rel: 云台yaw相对角（milli缩放）
 * 29 yaw_rel_set: 云台yaw相对角目标（milli缩放）
 * 30 yaw_gyro: 云台yaw角速度反馈（milli缩放）
 * 31 yaw_gyro_set: 云台yaw角速度目标（milli缩放）
 * 32 yaw_cur: 云台yaw给定电流
 * 33 pitch_abs: 云台pitch绝对角（milli缩放）
 * 34 pitch_abs_set: 云台pitch绝对角目标（milli缩放）
 * 35 pitch_rel: 云台pitch相对角（milli缩放）
 * 36 pitch_rel_set: 云台pitch相对角目标（milli缩放）
 * 37 pitch_gyro: 云台pitch角速度反馈（milli缩放）
 * 38 pitch_gyro_set: 云台pitch角速度目标（milli缩放）
 * 39 pitch_cur: 云台pitch给定电流
 *
 * 40 rc_ch0: 遥控通道0
 * 41 rc_ch1: 遥控通道1
 * 42 rc_ch2: 遥控通道2
 * 43 rc_ch3: 遥控通道3
 * 44 rc_s0: 遥控拨杆s0
 * 45 rc_s1: 遥控拨杆s1
 * 46 mouse_x: 鼠标x
 * 47 mouse_y: 鼠标y
 * 48 key_v: 键盘按键位图
 *
 * 49 event_bits: 事件位图
 * 50 gimbal_ok: 云台快照有效标志（1有效/0无效）
 * 51 chassis_ok: 底盘快照有效标志（1有效/0无效）
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
