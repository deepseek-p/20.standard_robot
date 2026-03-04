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
#include "usart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "detect_task.h"
#include "voltage_task.h"
#include "remote_control.h"
#include "gimbal_task.h"
#include "chassis_task.h"
#include "shoot.h"
#include "keyboard_action.h"
#include "wifi_bridge.h"
#if VT03_ENABLE
#include "vt03_link.h"
#endif

extern shoot_control_t shoot_control;

#define USB_DEBUG_FRAME_MAX_LEN 1024u
#define USB_DEBUG_RAD_SCALE     1000.0f
#define USB_DEBUG_INVALID_INT   2147483647L
#define USB_CMD_LINE_MAX_LEN    128u
#define USB_CMD_REPLY_MAX_LEN   192u

#define USB_CMD_GAIN_LIMIT_MAX  50000.0f
#define USB_CMD_OUT_LIMIT_MAX   30000.0f

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
 *  9:ref_toe       -> referee offline/error flag
 *
 * 10:ch_mode       -> chassis mode enum snapshot
 * 11:vx_set        -> chassis vx setpoint (milli-scale)
 * 12:vy_set        -> chassis vy setpoint (milli-scale)
 * 13:wz_set        -> chassis wz setpoint (milli-scale)
 * 14:rel           -> chassis relative angle (milli-scale)
 * 15:rel_set       -> chassis relative angle setpoint (milli-scale)
 * 16:ch_yaw        -> chassis yaw (milli-scale)
 * 17:ch_yaw_set    -> chassis yaw setpoint (milli-scale)
 * 18:i1            -> wheel current set[0]
 * 19:i2            -> wheel current set[1]
 * 20:i3            -> wheel current set[2]
 * 21:i4            -> wheel current set[3]
 *
 * 22:yaw_mode      -> gimbal yaw mode enum
 * 23:pitch_mode    -> gimbal pitch mode enum
 * 24:cali_step     -> gimbal calibration step
 * 25:yaw_abs       -> gimbal yaw absolute angle (milli-scale)
 * 26:yaw_abs_set   -> gimbal yaw absolute setpoint (milli-scale)
 * 27:yaw_rel       -> gimbal yaw relative angle (milli-scale)
 * 28:yaw_rel_set   -> gimbal yaw relative setpoint (milli-scale)
 * 29:yaw_gyro      -> gimbal yaw gyro feedback (milli-scale)
 * 30:yaw_gyro_set  -> gimbal yaw gyro setpoint (milli-scale)
 * 31:yaw_cur       -> gimbal yaw given current
 * 32:pitch_rel     -> gimbal pitch relative angle (milli-scale)
 * 33:pitch_rel_set -> gimbal pitch relative setpoint (milli-scale)
 * 34:pitch_cur     -> gimbal pitch given current
 *
 * 35:rc_ch0        -> RC channel 0
 * 36:rc_ch1        -> RC channel 1
 * 37:rc_ch2        -> RC channel 2
 * 38:rc_ch3        -> RC channel 3
 * 39:rc_s0         -> RC switch s0
 * 40:rc_s1         -> RC switch s1
 * 41:mouse_x       -> mouse x
 * 42:mouse_y       -> mouse y
 * 43:key_v         -> keyboard bitmask
 *
 * 44:event_bits    -> event bitmap (bit0..7 legacy, bit8..20 VT03/keyboard-action debug)
 * 45:gimbal_ok     -> gimbal snapshot valid flag
 * 46:chassis_ok    -> chassis snapshot valid flag
 *
 * 47:shoot_mode    -> shoot mode enum
 * 48:high_freq     -> high frequency flag
 * 49:fric1_rpm     -> friction wheel 1 actual RPM
 * 50:fric2_rpm     -> friction wheel 2 actual RPM
 * 51:fric_set      -> friction wheel target speed (milli-scale)
 * 52:fric1_cur     -> friction wheel 1 given current
 * 53:fric2_cur     -> friction wheel 2 given current
 * 54:trigger_cur   -> trigger motor given current
 * 55:ff_k_hat      -> pitch adaptive feedforward K_hat (milli-scale)
 * 56:ff_b_hat      -> pitch adaptive feedforward b_hat (raw, no scale)
 * 57:trigger_spd   -> trigger speed feedback (milli-scale)
 * 58:trigger_spd_set -> trigger speed setpoint (milli-scale)
 * 59:trigger_ecd_fdb -> trigger continuous encoder feedback (raw)
 * 60:trigger_ecd_set -> trigger continuous encoder setpoint (raw)
 * 61:local_heat    -> local shoot heat predictor (milli-scale)
 * 62:bullet_cnt    -> local bullet fired counter (raw)
 * 63:vt03_toe      -> VT03 offline/error flag
 */

static uint8_t usb_buf[USB_DEBUG_FRAME_MAX_LEN];
static const error_t *error_list_usb_local;

static uint32_t usb_debug_seq = 0;
static uint32_t usb_debug_drop_cnt = 0;
#if WIFI_BRIDGE_ENABLE
static uint32_t wifi_debug_drop_cnt = 0;
#endif
static uint16_t usb_debug_channel_mask = USB_DEBUG_CHANNEL_MASK_DEFAULT;

static uint8_t usb_last_dbus_error = 0xFFu;
static uint8_t usb_last_yaw_motor_error = 0xFFu;
static uint8_t usb_last_pitch_motor_error = 0xFFu;
static uint8_t usb_last_chassis_mode = 0xFFu;
static uint8_t usb_last_yaw_mode = 0xFFu;
static uint8_t usb_last_pitch_mode = 0xFFu;

typedef enum
{
    USB_PID_PARAM_KP = 0,
    USB_PID_PARAM_KI,
    USB_PID_PARAM_KD,
    USB_PID_PARAM_MAX_OUT,
    USB_PID_PARAM_MAX_IOUT,
} usb_pid_param_e;

typedef enum
{
    USB_PID_TARGET_PITCH_SPEED = 0,
    USB_PID_TARGET_PITCH_ANGLE,
    USB_PID_TARGET_PITCH_ENCODE,
    USB_PID_TARGET_YAW_SPEED,
    USB_PID_TARGET_YAW_ANGLE,
    USB_PID_TARGET_YAW_ENCODE,
    USB_PID_TARGET_CHASSIS_FOLLOW,
    USB_PID_TARGET_CHASSIS_WHEEL,
    USB_PID_TARGET_FRIC_SPEED,
    USB_PID_TARGET_TRIGGER,
    USB_PID_TARGET_COUNT,
} usb_pid_target_e;

typedef void (*cmd_reply_fn)(const char *format, ...);

static uint32_t usb_debug_now_ms(void);
static uint32_t usb_debug_next_seq(void);
static int32_t usb_debug_fp32_to_milli(fp32 value);
static int32_t usb_debug_masked_u8(uint16_t bit, uint8_t value);
static int32_t usb_debug_masked_i16(uint16_t bit, int16_t value);
static int32_t usb_debug_masked_i32(uint16_t bit, int32_t value);
static int32_t usb_debug_masked_fp32_milli(uint16_t bit, fp32 value);
static bool_t usb_emit_firewater_frame(uint32_t now_ms);
static void usb_cmd_process(void);
static void usb_cmd_process_line(char *line, cmd_reply_fn reply_fn);
static void usb_cmd_dump_all(cmd_reply_fn reply_fn);
static int usb_cmd_stricmp(const char *lhs, const char *rhs);
static const char *usb_cmd_target_name(usb_pid_target_e target);
static const char *usb_cmd_param_name(usb_pid_param_e param);
static bool_t usb_cmd_parse_target(const char *text, usb_pid_target_e *target);
static bool_t usb_cmd_parse_param(const char *text, usb_pid_param_e *param);
static bool_t usb_cmd_parse_value(const char *text, fp32 *value);
static bool_t usb_cmd_value_in_range(usb_pid_param_e param, fp32 value);
static bool_t usb_cmd_get_target_param(usb_pid_target_e target, usb_pid_param_e param, fp32 *value);
static bool_t usb_cmd_set_target_param(usb_pid_target_e target, usb_pid_param_e param, fp32 value);
static bool_t usb_cmd_get_gimbal_param(gimbal_PID_t *pid, usb_pid_param_e param, fp32 *value);
static bool_t usb_cmd_get_common_param(pid_type_def *pid, usb_pid_param_e param, fp32 *value);
static bool_t usb_cmd_get_enhanced_param(pid_enhanced_t *pid, usb_pid_param_e param, fp32 *value);
static bool_t usb_cmd_set_gimbal_param(gimbal_PID_t *pid, usb_pid_param_e param, fp32 value);
static bool_t usb_cmd_set_common_param(pid_type_def *pid, usb_pid_param_e param, fp32 value);
static bool_t usb_cmd_set_enhanced_param(pid_enhanced_t *pid, usb_pid_param_e param, fp32 value);
static void usb_cmd_replyf(const char *format, ...);
#if WIFI_BRIDGE_ENABLE
static void wifi_uart1_init(void);
static void wifi_cmd_process(void);
static void wifi_cmd_replyf(const char *format, ...);
#endif

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
#if WIFI_BRIDGE_ENABLE
    (void)usb_debug_drop_cnt;
#endif

    MX_USB_DEVICE_Init();
#if WIFI_BRIDGE_ENABLE
    wifi_uart1_init();
#endif
    error_list_usb_local = get_error_list_point();

    while (1)
    {
        uint32_t now_ms = usb_debug_now_ms();

#if (TELEM_OUTPUT_MODE != TELEM_MODE_NONE)
        if (now_ms - last_frame_ms >= USB_DEBUG_FRAME_PERIOD_MS)
        {
            usb_emit_firewater_frame(now_ms);
            last_frame_ms = now_ms;
        }
#endif
        usb_cmd_process();
#if WIFI_BRIDGE_ENABLE
        wifi_cmd_process();
#endif

        osDelay(USB_DEBUG_TASK_PERIOD_MS);
    }
}

static void usb_cmd_process(void)
{
    static char cmd_line[USB_CMD_LINE_MAX_LEN];
    static uint16_t cmd_len = 0;
    static uint8_t cmd_overflow = 0;

    while (usb_rx_available() > 0u)
    {
        uint8_t byte = usb_rx_read_byte();

        if (byte == '\r')
        {
            continue;
        }

        if (byte == '\n')
        {
            if (cmd_overflow)
            {
                usb_cmd_replyf("ERR line_too_long\r\n");
            }
            else if (cmd_len > 0u)
            {
                cmd_line[cmd_len] = '\0';
                usb_cmd_process_line(cmd_line, usb_cmd_replyf);
            }

            cmd_len = 0u;
            cmd_overflow = 0u;
            continue;
        }

        if (cmd_overflow)
        {
            continue;
        }

        if (cmd_len < (USB_CMD_LINE_MAX_LEN - 1u))
        {
            cmd_line[cmd_len++] = (char)byte;
        }
        else
        {
            cmd_overflow = 1u;
        }
    }
}

#if WIFI_BRIDGE_ENABLE
static void wifi_cmd_process(void)
{
    static char cmd_line[USB_CMD_LINE_MAX_LEN];
    static uint16_t cmd_len = 0;
    static uint8_t cmd_overflow = 0;

    while (uart1_rx_available() > 0u)
    {
        uint8_t byte = uart1_rx_read_byte();

        if (byte == '\r')
        {
            continue;
        }

        if (byte == '\n')
        {
            if (cmd_overflow)
            {
                wifi_cmd_replyf("ERR line_too_long\r\n");
            }
            else if (cmd_len > 0u)
            {
                cmd_line[cmd_len] = '\0';
                usb_cmd_process_line(cmd_line, wifi_cmd_replyf);
            }

            cmd_len = 0u;
            cmd_overflow = 0u;
            continue;
        }

        if (cmd_overflow)
        {
            continue;
        }

        if (cmd_len < (USB_CMD_LINE_MAX_LEN - 1u))
        {
            cmd_line[cmd_len++] = (char)byte;
        }
        else
        {
            cmd_overflow = 1u;
        }
    }
}
#endif

static void usb_cmd_process_line(char *line, cmd_reply_fn reply_fn)
{
    char *cmd;
    char *target_text;
    char *param_text;
    char *value_text;
    char *extra;
    usb_pid_target_e target;
    usb_pid_param_e param;
    fp32 value;

    if ((line == NULL) || (reply_fn == NULL))
    {
        return;
    }

    cmd = strtok(line, " \t");
    if (cmd == NULL)
    {
        return;
    }

    if (usb_cmd_stricmp(cmd, "SET") == 0)
    {
        target_text = strtok(NULL, " \t");
        param_text = strtok(NULL, " \t");
        value_text = strtok(NULL, " \t");
        extra = strtok(NULL, " \t");
        if ((target_text == NULL) || (param_text == NULL) || (value_text == NULL) || (extra != NULL))
        {
            reply_fn("ERR format SET\r\n");
            return;
        }

        if (!usb_cmd_parse_target(target_text, &target))
        {
            reply_fn("ERR target %s\r\n", target_text);
            return;
        }

        if (!usb_cmd_parse_param(param_text, &param))
        {
            reply_fn("ERR param %s\r\n", param_text);
            return;
        }

        if (!usb_cmd_parse_value(value_text, &value))
        {
            reply_fn("ERR value %s\r\n", value_text);
            return;
        }

        if (!usb_cmd_value_in_range(param, value))
        {
            reply_fn("ERR range %s %s %.6f\r\n",
                     usb_cmd_target_name(target),
                     usb_cmd_param_name(param),
                     (double)value);
            return;
        }

        if (!usb_cmd_set_target_param(target, param, value))
        {
            reply_fn("ERR target %s\r\n", usb_cmd_target_name(target));
            return;
        }

        reply_fn("OK SET %s %s %.6f\r\n",
                 usb_cmd_target_name(target),
                 usb_cmd_param_name(param),
                 (double)value);
        return;
    }

    if (usb_cmd_stricmp(cmd, "GET") == 0)
    {
        target_text = strtok(NULL, " \t");
        param_text = strtok(NULL, " \t");
        extra = strtok(NULL, " \t");

        if ((target_text == NULL) || (param_text == NULL) || (extra != NULL))
        {
            reply_fn("ERR format GET\r\n");
            return;
        }

        if (!usb_cmd_parse_target(target_text, &target))
        {
            reply_fn("ERR target %s\r\n", target_text);
            return;
        }

        if (!usb_cmd_parse_param(param_text, &param))
        {
            reply_fn("ERR param %s\r\n", param_text);
            return;
        }

        if (!usb_cmd_get_target_param(target, param, &value))
        {
            reply_fn("ERR target %s\r\n", usb_cmd_target_name(target));
            return;
        }

        reply_fn("OK GET %s %s %.6f\r\n",
                 usb_cmd_target_name(target),
                 usb_cmd_param_name(param),
                 (double)value);
        return;
    }

    if (usb_cmd_stricmp(cmd, "DUMP") == 0)
    {
        extra = strtok(NULL, " \t");
        if (extra != NULL)
        {
            reply_fn("ERR format DUMP\r\n");
            return;
        }

        usb_cmd_dump_all(reply_fn);
        return;
    }

    reply_fn("ERR cmd %s\r\n", cmd);
}

static void usb_cmd_dump_all(cmd_reply_fn reply_fn)
{
    usb_pid_target_e target;
    fp32 kp;
    fp32 ki;
    fp32 kd;
    fp32 max_out;
    fp32 max_iout;

    if (reply_fn == NULL)
    {
        return;
    }

    for (target = USB_PID_TARGET_PITCH_SPEED; target < USB_PID_TARGET_COUNT; target++)
    {
        if (!usb_cmd_get_target_param(target, USB_PID_PARAM_KP, &kp) ||
            !usb_cmd_get_target_param(target, USB_PID_PARAM_KI, &ki) ||
            !usb_cmd_get_target_param(target, USB_PID_PARAM_KD, &kd) ||
            !usb_cmd_get_target_param(target, USB_PID_PARAM_MAX_OUT, &max_out) ||
            !usb_cmd_get_target_param(target, USB_PID_PARAM_MAX_IOUT, &max_iout))
        {
            reply_fn("ERR dump %s\r\n", usb_cmd_target_name(target));
            continue;
        }

        reply_fn("DUMP %s Kp=%.6f Ki=%.6f Kd=%.6f max_out=%.6f max_iout=%.6f\r\n",
                 usb_cmd_target_name(target),
                 (double)kp,
                 (double)ki,
                 (double)kd,
                 (double)max_out,
                 (double)max_iout);
    }

    reply_fn("DUMP END\r\n");
}

static int usb_cmd_stricmp(const char *lhs, const char *rhs)
{
    unsigned char cl;
    unsigned char cr;

    if ((lhs == NULL) || (rhs == NULL))
    {
        return -1;
    }

    while ((*lhs != '\0') && (*rhs != '\0'))
    {
        cl = (unsigned char)tolower((unsigned char)*lhs);
        cr = (unsigned char)tolower((unsigned char)*rhs);
        if (cl != cr)
        {
            return (int)cl - (int)cr;
        }
        lhs++;
        rhs++;
    }

    return (int)((unsigned char)tolower((unsigned char)*lhs) - (unsigned char)tolower((unsigned char)*rhs));
}

static const char *usb_cmd_target_name(usb_pid_target_e target)
{
    switch (target)
    {
    case USB_PID_TARGET_PITCH_SPEED:
        return "pitch_speed";
    case USB_PID_TARGET_PITCH_ANGLE:
        return "pitch_angle";
    case USB_PID_TARGET_PITCH_ENCODE:
        return "pitch_encode";
    case USB_PID_TARGET_YAW_SPEED:
        return "yaw_speed";
    case USB_PID_TARGET_YAW_ANGLE:
        return "yaw_angle";
    case USB_PID_TARGET_YAW_ENCODE:
        return "yaw_encode";
    case USB_PID_TARGET_CHASSIS_FOLLOW:
        return "chassis_follow";
    case USB_PID_TARGET_CHASSIS_WHEEL:
        return "chassis_wheel";
    case USB_PID_TARGET_FRIC_SPEED:
        return "fric_speed";
    case USB_PID_TARGET_TRIGGER:
        return "trigger";
    default:
        return "unknown";
    }
}

static const char *usb_cmd_param_name(usb_pid_param_e param)
{
    switch (param)
    {
    case USB_PID_PARAM_KP:
        return "Kp";
    case USB_PID_PARAM_KI:
        return "Ki";
    case USB_PID_PARAM_KD:
        return "Kd";
    case USB_PID_PARAM_MAX_OUT:
        return "max_out";
    case USB_PID_PARAM_MAX_IOUT:
        return "max_iout";
    default:
        return "unknown";
    }
}

static bool_t usb_cmd_parse_target(const char *text, usb_pid_target_e *target)
{
    if ((text == NULL) || (target == NULL))
    {
        return 0;
    }

    if (usb_cmd_stricmp(text, "pitch_speed") == 0)
    {
        *target = USB_PID_TARGET_PITCH_SPEED;
    }
    else if (usb_cmd_stricmp(text, "pitch_angle") == 0)
    {
        *target = USB_PID_TARGET_PITCH_ANGLE;
    }
    else if (usb_cmd_stricmp(text, "pitch_encode") == 0)
    {
        *target = USB_PID_TARGET_PITCH_ENCODE;
    }
    else if (usb_cmd_stricmp(text, "yaw_speed") == 0)
    {
        *target = USB_PID_TARGET_YAW_SPEED;
    }
    else if (usb_cmd_stricmp(text, "yaw_angle") == 0)
    {
        *target = USB_PID_TARGET_YAW_ANGLE;
    }
    else if (usb_cmd_stricmp(text, "yaw_encode") == 0)
    {
        *target = USB_PID_TARGET_YAW_ENCODE;
    }
    else if (usb_cmd_stricmp(text, "chassis_follow") == 0)
    {
        *target = USB_PID_TARGET_CHASSIS_FOLLOW;
    }
    else if (usb_cmd_stricmp(text, "chassis_wheel") == 0)
    {
        *target = USB_PID_TARGET_CHASSIS_WHEEL;
    }
    else if (usb_cmd_stricmp(text, "fric_speed") == 0)
    {
        *target = USB_PID_TARGET_FRIC_SPEED;
    }
    else if (usb_cmd_stricmp(text, "trigger") == 0)
    {
        *target = USB_PID_TARGET_TRIGGER;
    }
    else
    {
        return 0;
    }

    return 1;
}

static bool_t usb_cmd_parse_param(const char *text, usb_pid_param_e *param)
{
    if ((text == NULL) || (param == NULL))
    {
        return 0;
    }

    if (usb_cmd_stricmp(text, "kp") == 0)
    {
        *param = USB_PID_PARAM_KP;
    }
    else if (usb_cmd_stricmp(text, "ki") == 0)
    {
        *param = USB_PID_PARAM_KI;
    }
    else if (usb_cmd_stricmp(text, "kd") == 0)
    {
        *param = USB_PID_PARAM_KD;
    }
    else if (usb_cmd_stricmp(text, "max_out") == 0)
    {
        *param = USB_PID_PARAM_MAX_OUT;
    }
    else if (usb_cmd_stricmp(text, "max_iout") == 0)
    {
        *param = USB_PID_PARAM_MAX_IOUT;
    }
    else
    {
        return 0;
    }

    return 1;
}

static bool_t usb_cmd_parse_value(const char *text, fp32 *value)
{
    char *endptr;
    fp32 parsed;

    if ((text == NULL) || (value == NULL))
    {
        return 0;
    }

    parsed = strtof(text, &endptr);
    if ((endptr == text) || (*endptr != '\0'))
    {
        return 0;
    }

    *value = parsed;
    return 1;
}

static bool_t usb_cmd_value_in_range(usb_pid_param_e param, fp32 value)
{
    fp32 max_value;

    if (value < 0.0f)
    {
        return 0;
    }

    if ((param == USB_PID_PARAM_KP) || (param == USB_PID_PARAM_KI) || (param == USB_PID_PARAM_KD))
    {
        max_value = USB_CMD_GAIN_LIMIT_MAX;
    }
    else
    {
        max_value = USB_CMD_OUT_LIMIT_MAX;
    }

    if (value > max_value)
    {
        return 0;
    }

    return 1;
}

static bool_t usb_cmd_get_target_param(usb_pid_target_e target, usb_pid_param_e param, fp32 *value)
{
    gimbal_control_t *gimbal;
    chassis_move_t *chassis;

    gimbal = get_gimbal_control_point();
    chassis = get_chassis_control_point();
    if ((gimbal == NULL) || (chassis == NULL))
    {
        return 0;
    }

    switch (target)
    {
    case USB_PID_TARGET_PITCH_SPEED:
        return usb_cmd_get_common_param(&gimbal->gimbal_pitch_motor.gimbal_motor_gyro_pid, param, value);
    case USB_PID_TARGET_PITCH_ANGLE:
        return usb_cmd_get_gimbal_param(&gimbal->gimbal_pitch_motor.gimbal_motor_absolute_angle_pid, param, value);
    case USB_PID_TARGET_PITCH_ENCODE:
        return usb_cmd_get_gimbal_param(&gimbal->gimbal_pitch_motor.gimbal_motor_relative_angle_pid, param, value);
    case USB_PID_TARGET_YAW_SPEED:
        return usb_cmd_get_common_param(&gimbal->gimbal_yaw_motor.gimbal_motor_gyro_pid, param, value);
    case USB_PID_TARGET_YAW_ANGLE:
        return usb_cmd_get_gimbal_param(&gimbal->gimbal_yaw_motor.gimbal_motor_absolute_angle_pid, param, value);
    case USB_PID_TARGET_YAW_ENCODE:
        return usb_cmd_get_gimbal_param(&gimbal->gimbal_yaw_motor.gimbal_motor_relative_angle_pid, param, value);
    case USB_PID_TARGET_CHASSIS_FOLLOW:
        return usb_cmd_get_common_param(&chassis->chassis_angle_pid, param, value);
    case USB_PID_TARGET_CHASSIS_WHEEL:
        return usb_cmd_get_common_param(&chassis->motor_speed_pid[0], param, value);
    case USB_PID_TARGET_FRIC_SPEED:
        return usb_cmd_get_common_param(&shoot_control.fric1_pid, param, value);
    case USB_PID_TARGET_TRIGGER:
        return usb_cmd_get_enhanced_param(&shoot_control.trigger_spd_pid, param, value);
    default:
        return 0;
    }
}

static bool_t usb_cmd_set_target_param(usb_pid_target_e target, usb_pid_param_e param, fp32 value)
{
    gimbal_control_t *gimbal;
    chassis_move_t *chassis;
    uint8_t i;

    gimbal = get_gimbal_control_point();
    chassis = get_chassis_control_point();
    if ((gimbal == NULL) || (chassis == NULL))
    {
        return 0;
    }

    switch (target)
    {
    case USB_PID_TARGET_PITCH_SPEED:
        return usb_cmd_set_common_param(&gimbal->gimbal_pitch_motor.gimbal_motor_gyro_pid, param, value);
    case USB_PID_TARGET_PITCH_ANGLE:
        return usb_cmd_set_gimbal_param(&gimbal->gimbal_pitch_motor.gimbal_motor_absolute_angle_pid, param, value);
    case USB_PID_TARGET_PITCH_ENCODE:
        return usb_cmd_set_gimbal_param(&gimbal->gimbal_pitch_motor.gimbal_motor_relative_angle_pid, param, value);
    case USB_PID_TARGET_YAW_SPEED:
        return usb_cmd_set_common_param(&gimbal->gimbal_yaw_motor.gimbal_motor_gyro_pid, param, value);
    case USB_PID_TARGET_YAW_ANGLE:
        return usb_cmd_set_gimbal_param(&gimbal->gimbal_yaw_motor.gimbal_motor_absolute_angle_pid, param, value);
    case USB_PID_TARGET_YAW_ENCODE:
        return usb_cmd_set_gimbal_param(&gimbal->gimbal_yaw_motor.gimbal_motor_relative_angle_pid, param, value);
    case USB_PID_TARGET_CHASSIS_FOLLOW:
        return usb_cmd_set_common_param(&chassis->chassis_angle_pid, param, value);
    case USB_PID_TARGET_CHASSIS_WHEEL:
        for (i = 0; i < 4u; i++)
        {
            if (!usb_cmd_set_common_param(&chassis->motor_speed_pid[i], param, value))
            {
                return 0;
            }
        }
        return 1;
    case USB_PID_TARGET_FRIC_SPEED:
        if (!usb_cmd_set_common_param(&shoot_control.fric1_pid, param, value))
        {
            return 0;
        }
        return usb_cmd_set_common_param(&shoot_control.fric2_pid, param, value);
    case USB_PID_TARGET_TRIGGER:
        return usb_cmd_set_enhanced_param(&shoot_control.trigger_spd_pid, param, value);
    default:
        return 0;
    }
}

static bool_t usb_cmd_get_gimbal_param(gimbal_PID_t *pid, usb_pid_param_e param, fp32 *value)
{
    if ((pid == NULL) || (value == NULL))
    {
        return 0;
    }

    switch (param)
    {
    case USB_PID_PARAM_KP:
        *value = pid->kp;
        break;
    case USB_PID_PARAM_KI:
        *value = pid->ki;
        break;
    case USB_PID_PARAM_KD:
        *value = pid->kd;
        break;
    case USB_PID_PARAM_MAX_OUT:
        *value = pid->max_out;
        break;
    case USB_PID_PARAM_MAX_IOUT:
        *value = pid->max_iout;
        break;
    default:
        return 0;
    }

    return 1;
}

static bool_t usb_cmd_get_common_param(pid_type_def *pid, usb_pid_param_e param, fp32 *value)
{
    if ((pid == NULL) || (value == NULL))
    {
        return 0;
    }

    switch (param)
    {
    case USB_PID_PARAM_KP:
        *value = pid->Kp;
        break;
    case USB_PID_PARAM_KI:
        *value = pid->Ki;
        break;
    case USB_PID_PARAM_KD:
        *value = pid->Kd;
        break;
    case USB_PID_PARAM_MAX_OUT:
        *value = pid->max_out;
        break;
    case USB_PID_PARAM_MAX_IOUT:
        *value = pid->max_iout;
        break;
    default:
        return 0;
    }

    return 1;
}

static bool_t usb_cmd_get_enhanced_param(pid_enhanced_t *pid, usb_pid_param_e param, fp32 *value)
{
    if ((pid == NULL) || (value == NULL))
    {
        return 0;
    }

    switch (param)
    {
    case USB_PID_PARAM_KP:
        *value = pid->Kp;
        break;
    case USB_PID_PARAM_KI:
        *value = pid->Ki;
        break;
    case USB_PID_PARAM_KD:
        *value = pid->Kd;
        break;
    case USB_PID_PARAM_MAX_OUT:
        *value = pid->max_out;
        break;
    case USB_PID_PARAM_MAX_IOUT:
        *value = pid->max_iout;
        break;
    default:
        return 0;
    }

    return 1;
}

static bool_t usb_cmd_set_gimbal_param(gimbal_PID_t *pid, usb_pid_param_e param, fp32 value)
{
    if (pid == NULL)
    {
        return 0;
    }

    switch (param)
    {
    case USB_PID_PARAM_KP:
        pid->kp = value;
        break;
    case USB_PID_PARAM_KI:
        pid->ki = value;
        break;
    case USB_PID_PARAM_KD:
        pid->kd = value;
        break;
    case USB_PID_PARAM_MAX_OUT:
        pid->max_out = value;
        break;
    case USB_PID_PARAM_MAX_IOUT:
        pid->max_iout = value;
        break;
    default:
        return 0;
    }

    return 1;
}

static bool_t usb_cmd_set_common_param(pid_type_def *pid, usb_pid_param_e param, fp32 value)
{
    if (pid == NULL)
    {
        return 0;
    }

    switch (param)
    {
    case USB_PID_PARAM_KP:
        pid->Kp = value;
        break;
    case USB_PID_PARAM_KI:
        pid->Ki = value;
        break;
    case USB_PID_PARAM_KD:
        pid->Kd = value;
        break;
    case USB_PID_PARAM_MAX_OUT:
        pid->max_out = value;
        break;
    case USB_PID_PARAM_MAX_IOUT:
        pid->max_iout = value;
        break;
    default:
        return 0;
    }

    return 1;
}

static bool_t usb_cmd_set_enhanced_param(pid_enhanced_t *pid, usb_pid_param_e param, fp32 value)
{
    if (pid == NULL)
    {
        return 0;
    }

    switch (param)
    {
    case USB_PID_PARAM_KP:
        pid->Kp = value;
        break;
    case USB_PID_PARAM_KI:
        pid->Ki = value;
        break;
    case USB_PID_PARAM_KD:
        pid->Kd = value;
        break;
    case USB_PID_PARAM_MAX_OUT:
        pid->max_out = value;
        break;
    case USB_PID_PARAM_MAX_IOUT:
        pid->max_iout = value;
        break;
    default:
        return 0;
    }

    return 1;
}

static void usb_cmd_replyf(const char *format, ...)
{
    char line[USB_CMD_REPLY_MAX_LEN];
    va_list args;
    int len;

    if (format == NULL)
    {
        return;
    }

    va_start(args, format);
    len = vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if ((len <= 0) || (len >= (int)sizeof(line)))
    {
        return;
    }

    {
        uint8_t retry;
        for (retry = 0; retry < 50u; retry++)
        {
            if (CDC_Transmit_FS((uint8_t *)line, (uint16_t)len) == 0)
            {
                break;
            }
            osDelay(1);
        }
    }
}

#if WIFI_BRIDGE_ENABLE
static void wifi_cmd_replyf(const char *format, ...)
{
    char line[USB_CMD_REPLY_MAX_LEN];
    va_list args;
    int len;

    if (format == NULL)
    {
        return;
    }

    va_start(args, format);
    len = vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if ((len <= 0) || (len >= (int)sizeof(line)))
    {
        return;
    }

    HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)len, 50u);
}

static void wifi_uart1_init(void)
{
    CLEAR_BIT(huart1.Instance->CR3, USART_CR3_DMAR);
    CLEAR_BIT(huart1.Instance->CR3, USART_CR3_DMAT);
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_IDLE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}
#endif

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
    uint8_t referee_error;
    uint8_t vt03_error;
    uint8_t gimbal_ok;
    uint8_t chassis_ok;
    uint8_t vt03_online;
    int16_t vt03_dial_ch;

    gimbal_debug_snapshot_t gimbal_snapshot;
    chassis_debug_snapshot_t chassis_snapshot;
    const RC_ctrl_t *rc_ctrl;
    const keyboard_cmd_t *kb_cmd;
    const key_state_t *key_state;

    dbus_error = error_list_usb_local[DBUS_TOE].error_exist;
    yaw_error = error_list_usb_local[YAW_GIMBAL_MOTOR_TOE].error_exist;
    pitch_error = error_list_usb_local[PITCH_GIMBAL_MOTOR_TOE].error_exist;
    gyro_error = error_list_usb_local[BOARD_GYRO_TOE].error_exist;
    accel_error = error_list_usb_local[BOARD_ACCEL_TOE].error_exist;
    referee_error = error_list_usb_local[REFEREE_TOE].error_exist;
    vt03_error = error_list_usb_local[VT03_TOE].error_exist;

    gimbal_ok = get_gimbal_debug_snapshot(&gimbal_snapshot);
    chassis_ok = get_chassis_debug_snapshot(&chassis_snapshot);
    rc_ctrl = get_remote_control_point();
    kb_cmd = get_keyboard_cmd();
    key_state = get_key_state();
    vt03_online = 0u;
    vt03_dial_ch = 0;
#if VT03_ENABLE
    if (!vt03_error)
    {
        vt03_online = 1u;
    }
    if (rc_ctrl != NULL)
    {
        vt03_dial_ch = rc_ctrl->rc.ch[4];
    }
#endif

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

    /* event_bits[8..20]: VT03/keyboard-action observability for key-mapping debug */
    if (vt03_online)
    {
        event_bits |= (1u << 8);
    }
    if (vt03_online && key_state->fn1_cur)
    {
        event_bits |= (1u << 9);
    }
    if (vt03_online && key_state->fn2_cur)
    {
        event_bits |= (1u << 10);
    }
    if (vt03_online && key_state->trigger_cur)
    {
        event_bits |= (1u << 11);
    }
    if (vt03_online && key_state->pause_cur)
    {
        event_bits |= (1u << 12);
    }
    if (kb_cmd->shoot_toggle)
    {
        event_bits |= (1u << 13);
    }
    if (kb_cmd->high_freq_toggle)
    {
        event_bits |= (1u << 14);
    }
    if (kb_cmd->burst_toggle)
    {
        event_bits |= (1u << 15);
    }
    if (kb_cmd->reverse_trigger)
    {
        event_bits |= (1u << 16);
    }
    if (kb_cmd->vt03_trigger)
    {
        event_bits |= (1u << 17);
    }
    if (vt03_dial_ch > 200)
    {
        event_bits |= (1u << 18);
    }
    else if (vt03_dial_ch < -200)
    {
        event_bits |= (1u << 19);
    }
    if (rc_ctrl != NULL && !switch_is_down(rc_ctrl->rc.s[0]))
    {
        event_bits |= (1u << 20);
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
                   "%lu,%lu,%lu,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\r\n",
                   now_ms,
                   seq,
#if WIFI_BRIDGE_ENABLE
                   wifi_debug_drop_cnt,
#else
                   usb_debug_drop_cnt,
#endif
                   usb_debug_masked_i32(USB_DBG_CH_HEALTH, get_battery_percentage()),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, dbus_error),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, yaw_error),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, pitch_error),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, gyro_error),
                   usb_debug_masked_u8(USB_DBG_CH_HEALTH, accel_error),
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
                    usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.pitch_relative : (fp32)0.0f),
                    usb_debug_masked_fp32_milli(USB_DBG_CH_GIMBAL, gimbal_ok ? gimbal_snapshot.pitch_relative_set : (fp32)0.0f),
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
                    usb_debug_masked_i32(USB_DBG_CH_EVENT, (int32_t)chassis_ok),
                    usb_debug_masked_i32(USB_DBG_CH_SHOOT, (int32_t)shoot_control.shoot_mode),
                    usb_debug_masked_i32(USB_DBG_CH_SHOOT, (int32_t)shoot_control.high_freq_flag),
                    usb_debug_masked_i32(USB_DBG_CH_SHOOT, (int32_t)shoot_control.fric1_motor_measure->speed_rpm),
                    usb_debug_masked_i32(USB_DBG_CH_SHOOT, (int32_t)shoot_control.fric2_motor_measure->speed_rpm),
                    usb_debug_masked_fp32_milli(USB_DBG_CH_SHOOT, shoot_control.fric_speed_set),
                    usb_debug_masked_i32(USB_DBG_CH_SHOOT, (int32_t)shoot_control.fric1_given_current),
                    usb_debug_masked_i32(USB_DBG_CH_SHOOT, (int32_t)shoot_control.fric2_given_current),
                    usb_debug_masked_i32(USB_DBG_CH_SHOOT, (int32_t)shoot_control.given_current),
                    usb_debug_fp32_to_milli(get_pitch_ff_K_hat()),
                    (int32_t)(get_pitch_ff_b_hat()),
                    usb_debug_masked_fp32_milli(USB_DBG_CH_SHOOT, shoot_control.speed),
                    usb_debug_masked_fp32_milli(USB_DBG_CH_SHOOT, shoot_control.speed_set),
                    usb_debug_masked_i32(USB_DBG_CH_SHOOT, (int32_t)shoot_control.trigger_ecd_fdb),
                    usb_debug_masked_i32(USB_DBG_CH_SHOOT, (int32_t)shoot_control.trigger_ecd_set),
                    usb_debug_masked_fp32_milli(USB_DBG_CH_SHOOT, shoot_control.local_heat),
                    usb_debug_masked_i32(USB_DBG_CH_SHOOT, (int32_t)shoot_control.bullet_fired_count),
                    usb_debug_masked_u8(USB_DBG_CH_HEALTH, vt03_error));

    if (len <= 0)
    {
        return 0;
    }

    if (len >= (int)USB_DEBUG_FRAME_MAX_LEN)
    {
#if WIFI_BRIDGE_ENABLE
        wifi_debug_drop_cnt++;
#else
        usb_debug_drop_cnt++;
#endif
        return 0;
    }

#if (TELEM_OUTPUT_MODE == TELEM_MODE_USB)
    if (CDC_Transmit_FS(usb_buf, (uint16_t)len) == 0)
    {
        return 1;
    }

    usb_debug_drop_cnt++;
    return 0;

#elif (TELEM_OUTPUT_MODE == TELEM_MODE_WIFI)
    if (HAL_UART_Transmit(&huart1, usb_buf, (uint16_t)len, 100u) == HAL_OK)
    {
        return 1;
    }
    wifi_debug_drop_cnt++;
    return 0;

#else
    (void)len;
    return 0;
#endif
}
