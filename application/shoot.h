/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       shoot.c/h
  * @brief      shoot module
  * @note
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. done
  *  V2.0.0     2026-03-07      refactor        2. align HUST control core
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */

#ifndef SHOOT_H
#define SHOOT_H
#include "struct_typedef.h"

#include "pid.h"
#include "CAN_receive.h"
#include "gimbal_task.h"
#include "remote_control.h"
#include "user_lib.h"

//射击开关通道数据
#define SHOOT_RC_MODE_CHANNEL       1

#define SHOOT_CONTROL_TIME          GIMBAL_CONTROL_TIME

//射击模式键盘按键
#define SHOOT_TOGGLE_KEYBOARD       KEY_PRESSED_OFFSET_Q
#define SHOOT_HIGH_FREQ_KEYBOARD    KEY_PRESSED_OFFSET_C
#define SHOOT_FRIC_DEC_KEYBOARD     KEY_PRESSED_OFFSET_F
#define SHOOT_FRIC_INC_KEYBOARD     KEY_PRESSED_OFFSET_SHIFT
#define SHOOT_REVERSE_KEYBOARD      KEY_PRESSED_OFFSET_G
#define SHOOT_BURST_KEYBOARD        KEY_PRESSED_OFFSET_R

//鼠标长按判定时间
#define MOUSE_LONG_PRESS_TIME       100
//遥控器拨杆持续按下判定时间，超过则连发
#define RC_S_LONG_TIME              2000
//编码器相关
#define HALF_ECD_RANGE              4096
#define ECD_RANGE                   8191
//电机 rpm → rad/s（保留定义，shoot.c 不再使用，其他模块可能用到）
#define MOTOR_RPM_TO_SPEED          0.00290888208665721596153948461415f
#define MOTOR_ECD_TO_ANGLE          0.000021305288720633905968306772076277f
#define FULL_COUNT                  18

//连发拨弹速度（rpm，对齐 HUST PullerSpeed）
#define PULLER_SPEED_NORMAL         3500.0f
#define PULLER_SPEED_HIGH_FREQ      4500.0f

#define KEY_OFF_JUGUE_TIME          500
#define SWITCH_TRIGGER_ON           0
#define SWITCH_TRIGGER_OFF          1

//摩擦轮速度PID（不变）
#define FRIC_SPEED_PID_KP           10.0f
#define FRIC_SPEED_PID_KI           0.0f
#define FRIC_SPEED_PID_KD           0.0f
#define FRIC_PID_MAX_OUT            9900.0f
#define FRIC_PID_MAX_IOUT           1500.0f

//摩擦轮挡位（两挡）
#define FRIC_GEAR_COUNT             2
#define FRIC_SPEED_GEAR_0           5980.0f   // ~19m/s
#define FRIC_SPEED_GEAR_1           6580.0f   // ~23m/s (max<25)
#define FRIC_READY_SPEED_ERR        200.0f
#define FRIC_READY_MIN_SPEED        3000.0f

//拨弹位置环（对齐 HUST，不变）
#define TRIGGER_POS_KP              0.4f
#define TRIGGER_POS_KI              0.02f
#define TRIGGER_POS_KD              0.0f
#define TRIGGER_POS_MAX_OUT         20000.0f
#define TRIGGER_POS_MAX_IOUT        1500.0f
#define TRIGGER_POS_DEADZONE        0.0f
#define TRIGGER_POS_I_L             8000.0f
#define TRIGGER_POS_I_U             12000.0f
#define TRIGGER_POS_RC_DF           0.5f

//拨弹速度环（对齐 HUST rpm 单位，Robot_ID=3/4/14/46）
#define TRIGGER_SPD_KP              6.2f
#define TRIGGER_SPD_KI              3.2f
#define TRIGGER_SPD_KD              0.0f
#define TRIGGER_SPD_MAX_OUT         10000.0f
#define TRIGGER_SPD_MAX_IOUT        1000.0f
#define TRIGGER_SPD_DEADZONE        50.0f
#define TRIGGER_SPD_I_L             100.0f
#define TRIGGER_SPD_I_U             200.0f
#define TRIGGER_SPD_RC_DF           0.5f

// 8192 * 36 / 8 = 36864 (1/8 turn per bullet)
#define TRIGGER_ONEGRID             36864.0f
#define TRIGGER_POS_THRESHOLD       5000.0f

#define TRIGGER_CAN_CURRENT_LIMIT   10000   // 对齐 HUST speed PID OutMax

#define HEAT_PER_BULLET             10.0f
#define HEAT_LIMIT_SAFE             80.0f
#define HEAT_LIMIT_BURST            180.0f
#define HEAT_COOL_RATE              12.0f

// 设为 1 = 调试模式（跳过热量门控），设 0 = 正常模式
#define SHOOT_DEBUG_MODE            0

typedef enum
{
    SHOOT_STOP = 0,
    SHOOT_READY_FRIC,
    SHOOT_READY_BULLET,
    SHOOT_BULLET,
    SHOOT_CONTINUE_BULLET,
} shoot_mode_e;

typedef enum
{
    SHOOT_UI_GEAR_LOW = 0,
    SHOOT_UI_GEAR_HIGH = 1,
} shoot_ui_gear_e;

typedef struct
{
    shoot_mode_e shoot_mode;
    const RC_ctrl_t *shoot_rc;
    const motor_measure_t *shoot_motor_measure;
    const motor_measure_t *fric1_motor_measure;
    const motor_measure_t *fric2_motor_measure;

    pid_type_def fric1_pid;
    pid_type_def fric2_pid;
    fp32 fric_speed_set;
    fp32 fric_speed_base;
    int16_t fric1_given_current;
    int16_t fric2_given_current;

    pid_enhanced_t trigger_pos_pid;
    pid_enhanced_t trigger_spd_pid;
    fp32 trigger_speed_set;
    fp32 speed;                     // 速度反馈（现在是 rpm，不再是 rad/s）
    fp32 speed_set;                 // 速度目标（rpm）
    fp32 angle;
    int16_t given_current;

    int8_t ecd_count;
    int32_t trigger_ecd_total_count;
    int32_t trigger_ecd_set;
    int32_t trigger_ecd_fdb;
    int32_t trigger_ecd_last_fire;
    bool_t reverse_flag;

    bool_t press_l;
    bool_t press_r;
    bool_t last_press_l;
    bool_t last_press_r;
    uint16_t press_l_time;
    uint16_t press_r_time;
    uint16_t rc_s_time;

    bool_t key;
    bool_t microswitch_on;

    uint16_t heat_limit;
    fp32 local_heat;
    uint16_t referee_heat;
    bool_t burst_mode;
    bool_t last_key_r;

    uint16_t bullet_fired_count;
    bool_t high_freq_flag;
    bool_t last_key_c;
    bool_t last_key_q;
    bool_t last_key_f;
    bool_t arm_enable;
    bool_t single_fire_req;
    bool_t continue_req;
    bool_t reverse_req;
} shoot_control_t;

extern void shoot_init(void);
extern int16_t shoot_control_loop(void);
extern void shoot_get_fric_current(int16_t *fric1, int16_t *fric2);
extern shoot_ui_gear_e shoot_get_ui_gear(void);
extern bool_t shoot_consume_reverse_success_pulse(void);

#endif
