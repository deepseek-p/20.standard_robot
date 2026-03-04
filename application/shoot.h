/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       shoot.c/h
  * @brief      ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ü¡ï¿?
  * @note
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. ï¿½ï¿½ï¿?
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

//ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ä¿ªï¿½ï¿½Í¨ï¿½ï¿½ï¿½ï¿½ï¿½ï¿?
#define SHOOT_RC_MODE_CHANNEL       1

#define SHOOT_CONTROL_TIME          GIMBAL_CONTROL_TIME

//ï¿½ï¿½ï¿½ï¿½Ä£Ê½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿?
#define SHOOT_TOGGLE_KEYBOARD       KEY_PRESSED_OFFSET_Q
#define SHOOT_HIGH_FREQ_KEYBOARD    KEY_PRESSED_OFFSET_C
#define SHOOT_FRIC_DEC_KEYBOARD     KEY_PRESSED_OFFSET_F
#define SHOOT_FRIC_INC_KEYBOARD     KEY_PRESSED_OFFSET_SHIFT
#define SHOOT_REVERSE_KEYBOARD      KEY_PRESSED_OFFSET_G
#define SHOOT_BURST_KEYBOARD        KEY_PRESSED_OFFSET_R

//ï¿½ï¿½ï¿½ï¿½ï¿½Éºï¿½ ï¿½Óµï¿½ï¿½ï¿½ï¿½ï¿½È¥ï¿½ï¿½ï¿½Ð¶ï¿½Ê±ï¿½ä£¬ï¿½Ô·ï¿½ï¿½ó´¥·ï¿½
#define SHOOT_DONE_KEY_OFF_TIME     100
//ï¿½ï¿½ê³¤ï¿½ï¿½ï¿½Ð¶ï¿?
#define MOUSE_LONG_PRESS_TIME       100
//Ò£ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ø´ï¿½ï¿½Âµï¿½Ò»ï¿½ï¿½Ê±ï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Óµï¿½ ï¿½ï¿½ï¿½ï¿½ï¿½åµ¯
#define RC_S_LONG_TIME              2000
//ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Öµï¿½ï¿½Î?
#define HALF_ECD_RANGE              4096
#define ECD_RANGE                   8191
//ï¿½ï¿½ï¿½rmp ï¿½ä»¯ï¿½ï¿½ ï¿½ï¿½×ªï¿½Ù¶ÈµÄ±ï¿½ï¿½ï¿½
#define MOTOR_RPM_TO_SPEED          0.00290888208665721596153948461415f
#define MOTOR_ECD_TO_ANGLE          0.000021305288720633905968306772076277f
#define FULL_COUNT                  18

#define CONTINUE_TRIGGER_SPEED      15.0f
#define READY_TRIGGER_SPEED         5.0f

#define KEY_OFF_JUGUE_TIME          500
#define SWITCH_TRIGGER_ON           0
#define SWITCH_TRIGGER_OFF          1

//ï¿½ï¿½ï¿½ï¿½Ê±ï¿½ï¿½ ï¿½Ô¼ï¿½ï¿½ï¿½×ªÊ±ï¿½ï¿½
#define BLOCK_TRIGGER_SPEED         1.0f
#define BLOCK_TIME                  700
#define REVERSE_TIME                500
#define REVERSE_SPEED_LIMIT         13.0f

//Ä¦ï¿½ï¿½ï¿½ï¿½ï¿½Ù¶ï¿½PID
#define FRIC_SPEED_PID_KP           10.0f
#define FRIC_SPEED_PID_KI           0.0f
#define FRIC_SPEED_PID_KD           0.0f
#define FRIC_PID_MAX_OUT            9900.0f
#define FRIC_PID_MAX_IOUT           1500.0f

//Ä¦ï¿½ï¿½ï¿½ï¿½Ä¿ï¿½ï¿½×ªï¿½ï¿½
#define FRIC_SPEED_LOW              4900.0f
#define FRIC_SPEED_MID              5800.0f
#define FRIC_SPEED_HIGH             8000.0f
#define FRIC_SPEED_ADJUST_STEP      100.0f
#define FRIC_READY_SPEED_ERR        200.0f
#define FRIC_READY_MIN_SPEED        3000.0f

//ï¿½ï¿½ï¿½ï¿½Î»ï¿½Ã»ï¿½ï¿½ï¿½ï¿½ï¿½Ç¿ï¿½Í£ï¿½
#define TRIGGER_POS_KP              0.4f
#define TRIGGER_POS_KI              0.02f
#define TRIGGER_POS_KD              0.0f
#define TRIGGER_POS_MAX_OUT         20000.0f
#define TRIGGER_POS_MAX_IOUT        1500.0f
#define TRIGGER_POS_DEADZONE        0.0f
#define TRIGGER_POS_I_L             8000.0f
#define TRIGGER_POS_I_U             12000.0f
#define TRIGGER_POS_RC_DF           0.5f

//ï¿½ï¿½ï¿½ï¿½ï¿½Ù¶È»ï¿½ï¿½ï¿½ï¿½ï¿½Ç¿ï¿½Í£ï¿½
#define TRIGGER_SPD_KP              200.0f
#define TRIGGER_SPD_KI              3.0f
#define TRIGGER_SPD_KD              0.0f
#define TRIGGER_SPD_MAX_OUT         10000.0f
#define TRIGGER_SPD_MAX_IOUT        5000.0f
#define TRIGGER_SPD_DEADZONE        0.0f
#define TRIGGER_SPD_I_L             100.0f
#define TRIGGER_SPD_I_U             200.0f
#define TRIGGER_SPD_RC_DF           0.5f

// 8192 * 36 / 8 = 36864 (1/8 turn per bullet)
#define TRIGGER_ONEGRID             36864.0f
#define TRIGGER_POS_THRESHOLD       5000.0f

#define TRIGGER_POS_MAX_OUT_HOLD    10.0f    // DONE/READY_BULLET ÈáºÍ³Ö²ÖÏÞ·ù
#define TRIGGER_POS_MAX_OUT_FIRE    20000.0f // BULLET È«Á¦¾Ø
#define MAX_REVERSE_COUNT           3        // ¶Â×ª·´×ª×î´ó´ÎÊý£¬³¬¹ý·ÅÆú±¾´Î

#define HEAT_PER_BULLET             10.0f
#define HEAT_LIMIT_SAFE             80.0f
#define HEAT_LIMIT_BURST            180.0f
#define HEAT_COOL_RATE              12.0f

// ï¿½ï¿½Îª 1 = ï¿½ï¿½ï¿½ï¿½Ä£Ê½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Þµï¿½ï¿½Ù£ï¿½ï¿½ï¿½0 = ï¿½ï¿½ï¿½ï¿½Ä£Ê½
#define SHOOT_DEBUG_MODE            0

typedef enum
{
    SHOOT_STOP = 0,
    SHOOT_READY_FRIC,
    SHOOT_READY_BULLET,
    SHOOT_READY,
    SHOOT_BULLET,
    SHOOT_CONTINUE_BULLET,
    SHOOT_DONE,
} shoot_mode_e;

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
    fp32 speed;
    fp32 speed_set;
    fp32 angle;
    int16_t given_current;

    int8_t ecd_count;
    int32_t trigger_ecd_total_count;
    fp32 trigger_ecd_set;
    fp32 trigger_ecd_fdb;
    fp32 trigger_ecd_last_fire;
    bool_t reverse_flag;

    bool_t press_l;
    bool_t press_r;
    bool_t last_press_l;
    bool_t last_press_r;
    uint16_t press_l_time;
    uint16_t press_r_time;
    uint16_t rc_s_time;

    uint16_t block_time;
    uint16_t reverse_time;
    uint8_t reverse_count;       // ÀÛ¼Æ·´×ª´ÎÊý
    bool_t move_flag;

    bool_t key;
    uint8_t key_time;

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
} shoot_control_t;

//ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ì¨Ê¹ï¿½ï¿½Í¬Ò»ï¿½ï¿½canï¿½ï¿½idï¿½ï¿½Ò²ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ì¨ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ö´ï¿½ï¿?
extern void shoot_init(void);
extern int16_t shoot_control_loop(void);
extern void shoot_get_fric_current(int16_t *fric1, int16_t *fric2);

#endif
