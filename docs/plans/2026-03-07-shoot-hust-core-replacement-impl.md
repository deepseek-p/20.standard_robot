# Shoot HUST Control Core Replacement — Codex Implementation Plan

**Goal:** 把发射机构控制核心替换为 HUST_Infantry_2023 的方案（rpm 单位级联 PID、连发跳过位置环、简单反转），保留现有操作接口不变。

**分支:** `shoot-state-machine-align-hust`

**设计文档:** `docs/plans/2026-03-06-shoot-hust-control-core-replacement.md`

---

## 禁止事项（DO NOT TOUCH）

以下内容**绝对不能修改**，任何情况下都不要动：

1. `shoot_feedback_update()` 中编码器累加逻辑（ecd_count / trigger_ecd_total_count）
2. `shoot_feedback_update()` 中微动开关读取和鼠标/VT03 按键计时
3. `shoot_set_mode()` 中从开头到 `shoot_control.reverse_req = reverse_req;`（第472行）的全部输入处理代码
4. 摩擦轮 PID 计算（fric1_pid / fric2_pid）
5. `shoot_get_fric_current()` 函数
6. `application/usb_task.c`（遥测导出代码）
7. 摩擦轮 PID 参数（FRIC_SPEED_PID_KP 等）
8. 位置环 PID 参数（TRIGGER_POS_KP 等，已和 HUST 一致）
9. 除 `shoot.c`、`shoot.h`、`shoot_logic.c`、`shoot_logic.h`、`standard_robot.uvprojx` 以外的任何文件

---

## Task 1: 重写 `application/shoot.h`

把整个 `application/shoot.h` 替换为以下完整内容：

```c
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

//摩擦轮目标转速（不变）
#define FRIC_SPEED_LOW              4900.0f
#define FRIC_SPEED_MID              5800.0f
#define FRIC_SPEED_HIGH             8000.0f
#define FRIC_SPEED_ADJUST_STEP      100.0f
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

#endif
```

### 提交

```bash
git add application/shoot.h
git commit -m "refactor(shoot): align shoot.h with HUST rpm PID params and clean struct"
```

---

## Task 2: 重写 `application/shoot.c`

把整个 `application/shoot.c` 替换为以下完整内容。

**注意：** `shoot_feedback_update()` 只改了一行（去掉 `MOTOR_RPM_TO_SPEED`），`shoot_set_mode()` 只替换了底部的模式转换段。其余全部保留原样。为了消除歧义，下面给出完整文件。

```c
/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       shoot.c/h
  * @brief      shoot control
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

#include "shoot.h"
#include "keyboard_action.h"
#if VT03_ENABLE
#include "vt03_link.h"
#endif
#include "main.h"

#include "cmsis_os.h"

#include "bsp_laser.h"
#include "arm_math.h"
#include "user_lib.h"
#include "referee.h"

#include "CAN_receive.h"
#include "gimbal_behaviour.h"
#include "detect_task.h"
#include "pid.h"

/* 注意: 不再 #include "shoot_logic.h"，该模块已删除 */

#define shoot_laser_on()    laser_on()
#define shoot_laser_off()   laser_off()
#define BUTTEN_TRIG_PIN HAL_GPIO_ReadPin(BUTTON_TRIG_GPIO_Port, BUTTON_TRIG_Pin)

static void shoot_set_mode(void);
static void shoot_feedback_update(void);
static void shoot_clear_trigger_pid_state(void);

shoot_control_t shoot_control;

/* 注意: 不再有 shoot_cmd_state / shoot_exec_state 静态变量 */
/* 注意: 不再有 trigger_motor_turn_back / shoot_bullet_control 函数 */

static void shoot_clear_trigger_pid_state(void)
{
    PID_enhanced_clear(&shoot_control.trigger_pos_pid);
    PID_enhanced_clear(&shoot_control.trigger_spd_pid);
}

/**
  * @brief  射击初始化
  */
void shoot_init(void)
{
    static const fp32 fric_speed_pid[3] = {FRIC_SPEED_PID_KP, FRIC_SPEED_PID_KI, FRIC_SPEED_PID_KD};

    shoot_control.shoot_mode = SHOOT_STOP;
    shoot_control.shoot_rc = get_remote_control_point();
    shoot_control.shoot_motor_measure = get_trigger_motor_measure_point();
    shoot_control.fric1_motor_measure = get_fric1_motor_measure_point();
    shoot_control.fric2_motor_measure = get_fric2_motor_measure_point();

    /* 初始化 PID */
    PID_enhanced_init(&shoot_control.trigger_pos_pid,
                      TRIGGER_POS_KP,
                      TRIGGER_POS_KI,
                      TRIGGER_POS_KD,
                      TRIGGER_POS_MAX_OUT,
                      TRIGGER_POS_MAX_IOUT,
                      TRIGGER_POS_DEADZONE,
                      TRIGGER_POS_I_L,
                      TRIGGER_POS_I_U,
                      TRIGGER_POS_RC_DF);
    PID_enhanced_init(&shoot_control.trigger_spd_pid,
                      TRIGGER_SPD_KP,
                      TRIGGER_SPD_KI,
                      TRIGGER_SPD_KD,
                      TRIGGER_SPD_MAX_OUT,
                      TRIGGER_SPD_MAX_IOUT,
                      TRIGGER_SPD_DEADZONE,
                      TRIGGER_SPD_I_L,
                      TRIGGER_SPD_I_U,
                      TRIGGER_SPD_RC_DF);
    PID_init(&shoot_control.fric1_pid, PID_POSITION, fric_speed_pid, FRIC_PID_MAX_OUT, FRIC_PID_MAX_IOUT);
    PID_init(&shoot_control.fric2_pid, PID_POSITION, fric_speed_pid, FRIC_PID_MAX_OUT, FRIC_PID_MAX_IOUT);

    shoot_control.ecd_count = 0;
    shoot_control.trigger_ecd_total_count = 0;
    shoot_feedback_update();

    shoot_control.bullet_fired_count = 0;
    shoot_control.angle = shoot_control.shoot_motor_measure->ecd * MOTOR_ECD_TO_ANGLE;
    shoot_control.given_current = 0;
    shoot_control.reverse_flag = 0;
    shoot_control.speed = 0.0f;
    shoot_control.speed_set = 0.0f;
    shoot_control.trigger_speed_set = 0.0f;

    shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
    shoot_control.trigger_ecd_last_fire = shoot_control.trigger_ecd_fdb;

    shoot_control.fric_speed_set = 0.0f;
    shoot_control.fric_speed_base = FRIC_SPEED_LOW;
    shoot_control.fric1_given_current = 0;
    shoot_control.fric2_given_current = 0;

    shoot_control.heat_limit = 0;
    shoot_control.local_heat = 0.0f;
    shoot_control.referee_heat = 0;
    shoot_control.burst_mode = 0;

    shoot_control.high_freq_flag = 0;
    shoot_control.last_key_c = 0;
    shoot_control.last_key_q = 0;
    shoot_control.last_key_f = 0;
    shoot_control.last_key_r = 0;
    shoot_control.arm_enable = 0;
    shoot_control.single_fire_req = 0;
    shoot_control.continue_req = 0;
    shoot_control.reverse_req = 0;
    shoot_control.microswitch_on = 0;

    /* 注意: 不再调用 shoot_logic_init()，不再初始化 shoot_exec_state */
}

/**
  * @brief  射击主控制循环（对齐 HUST 架构）
  * @retval 拨弹电机 CAN 电流值
  */
int16_t shoot_control_loop(void)
{
    uint16_t referee_heat_now;
    shoot_mode_e prev_mode;

    shoot_feedback_update();
    prev_mode = shoot_control.shoot_mode;

    /* 热量冷却: 每次调用扣减 1ms 冷却量 */
    if (shoot_control.local_heat > 0.0f)
    {
        shoot_control.local_heat -= HEAT_COOL_RATE / 1000.0f;
        if (shoot_control.local_heat < 0.0f)
        {
            shoot_control.local_heat = 0.0f;
        }
    }

    /* 裁判系统热量同步 */
    get_shoot_heat0_limit_and_heat0(&shoot_control.heat_limit, &referee_heat_now);
    if (referee_heat_now != shoot_control.referee_heat)
    {
        shoot_control.referee_heat = referee_heat_now;
        shoot_control.local_heat = (fp32)referee_heat_now;
    }

    /* 模式决策（内部包含 HUST 风格模式转换） */
    shoot_set_mode();

    /* PID 复位: 从 STOP/READY_FRIC 进入有效模式时清零 */
    if (shoot_control.shoot_mode != prev_mode)
    {
        if (prev_mode == SHOOT_STOP || prev_mode == SHOOT_READY_FRIC)
        {
            shoot_clear_trigger_pid_state();
        }
    }

    /* ===================== 各模式控制输出 ===================== */

    if (shoot_control.shoot_mode == SHOOT_STOP)
    {
        /* HUST Shoot_Powerdown: 锁位置、零电流、复位参数 */
        shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
        shoot_control.speed_set = 0.0f;
        shoot_control.given_current = 0;
        shoot_control.fric_speed_base = FRIC_SPEED_LOW;
        shoot_control.high_freq_flag = 0;
        shoot_control.burst_mode = 0;
    }
    else if (shoot_control.shoot_mode == SHOOT_READY_FRIC)
    {
        /* 摩擦轮加速中，拨弹不动 */
        shoot_control.speed_set = 0.0f;
        shoot_control.given_current = 0;
    }
    else if (shoot_control.shoot_mode == SHOOT_READY_BULLET)
    {
        /* HUST 风格位置 hold: 位置环→速度环级联，setpoint 不动 */
        shoot_control.speed_set = PID_enhanced_calc(&shoot_control.trigger_pos_pid,
                                                    (fp32)shoot_control.trigger_ecd_fdb,
                                                    (fp32)shoot_control.trigger_ecd_set);
        shoot_control.given_current = (int16_t)PID_enhanced_calc(&shoot_control.trigger_spd_pid,
                                                                  shoot_control.speed,
                                                                  shoot_control.speed_set);
    }
    else if (shoot_control.shoot_mode == SHOOT_BULLET)
    {
        /* HUST Shoot_Fire 非连发: 位置环→速度环级联，步进一格 */
        shoot_control.speed_set = PID_enhanced_calc(&shoot_control.trigger_pos_pid,
                                                    (fp32)shoot_control.trigger_ecd_fdb,
                                                    (fp32)shoot_control.trigger_ecd_set);
        shoot_control.given_current = (int16_t)PID_enhanced_calc(&shoot_control.trigger_spd_pid,
                                                                  shoot_control.speed,
                                                                  shoot_control.speed_set);
    }
    else if (shoot_control.shoot_mode == SHOOT_CONTINUE_BULLET)
    {
        /* HUST ShootContinue: 跳过位置环，速度环直接驱动 */
        shoot_control.speed_set = shoot_control.high_freq_flag
                                  ? PULLER_SPEED_HIGH_FREQ
                                  : PULLER_SPEED_NORMAL;
        shoot_control.given_current = (int16_t)PID_enhanced_calc(&shoot_control.trigger_spd_pid,
                                                                  shoot_control.speed,
                                                                  shoot_control.speed_set);

        /* 编码器弹丸计数（比 HUST 摩擦轮误差检测更可靠） */
        while ((shoot_control.trigger_ecd_fdb - shoot_control.trigger_ecd_last_fire)
                >= (int32_t)TRIGGER_ONEGRID)
        {
            shoot_control.trigger_ecd_last_fire += (int32_t)TRIGGER_ONEGRID;
            shoot_control.bullet_fired_count++;
            shoot_control.local_heat += HEAT_PER_BULLET;
        }
    }

    /* ===================== 激光 ===================== */
    if (shoot_control.shoot_mode == SHOOT_STOP)
    {
        shoot_laser_off();
    }
    else
    {
        shoot_laser_on();
    }

    /* ===================== 电流限幅 ===================== */
    shoot_control.given_current = (int16_t)fp32_constrain(
        (fp32)shoot_control.given_current,
        -(fp32)TRIGGER_CAN_CURRENT_LIMIT,
        (fp32)TRIGGER_CAN_CURRENT_LIMIT);

    /* ===================== 摩擦轮 ===================== */
    if (shoot_control.shoot_mode == SHOOT_STOP)
    {
        shoot_control.fric_speed_set = 0.0f;
    }
    else
    {
        shoot_control.fric_speed_set = shoot_control.fric_speed_base;
    }

    /* 摩擦轮1正转，摩擦轮2反转 */
    PID_calc(&shoot_control.fric1_pid,
             (fp32)shoot_control.fric1_motor_measure->speed_rpm,
             shoot_control.fric_speed_set);
    PID_calc(&shoot_control.fric2_pid,
             (fp32)shoot_control.fric2_motor_measure->speed_rpm,
             -shoot_control.fric_speed_set);
    shoot_control.fric1_given_current = (int16_t)shoot_control.fric1_pid.out;
    shoot_control.fric2_given_current = (int16_t)shoot_control.fric2_pid.out;

    return shoot_control.given_current;
}

/**
  * @brief  射击模式设定（输入处理 + HUST 风格模式转换）
  *
  *   输入处理段（从函数开头到 shoot_control.reverse_req 赋值）完全保留原样，
  *   只有底部的模式转换段替换为内联逻辑（取代 shoot_logic.c 状态机）。
  */
static void shoot_set_mode(void)
{
    static int8_t last_s = RC_SW_UP;
    bool_t key_q;
    bool_t key_c;
    bool_t key_f;
    bool_t key_r;
    bool_t fric_ready;
    bool_t allow_fire;
    bool_t continue_req;
    bool_t reverse_req;
    bool_t fire_cmd_edge;
    bool_t fire_input_active;
    bool_t vt03_shoot_enable;
    bool_t shoot_aux_enable;
    const keyboard_cmd_t *kb_cmd;

    kb_cmd = get_keyboard_cmd();
    fire_cmd_edge = 0;
    vt03_shoot_enable = 0;
#if VT03_ENABLE
    if (!toe_is_error(VT03_TOE) && !switch_is_down(shoot_control.shoot_rc->rc.s[0]))
    {
        vt03_shoot_enable = 1;
    }
#endif
    shoot_aux_enable = (bool_t)(switch_is_mid(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]) || vt03_shoot_enable);
    fire_cmd_edge = (bool_t)(((switch_is_down(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]) && !switch_is_down(last_s)) ||
                              (shoot_control.press_l && (shoot_control.last_press_l == 0)) ||
                              (shoot_control.press_r && (shoot_control.last_press_r == 0))));
    fire_input_active = (bool_t)(switch_is_down(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]) ||
                                 shoot_control.press_l ||
                                 shoot_control.press_r);

    if (switch_is_up(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]) && !switch_is_up(last_s))
    {
        shoot_control.arm_enable = !shoot_control.arm_enable;
    }

    if (kb_cmd->shoot_toggle && shoot_aux_enable)
    {
        shoot_control.arm_enable = !shoot_control.arm_enable;
    }

    key_q = (shoot_control.shoot_rc->key.v & SHOOT_TOGGLE_KEYBOARD) ? 1 : 0;
    key_c = (shoot_control.shoot_rc->key.v & SHOOT_HIGH_FREQ_KEYBOARD) ? 1 : 0;
    key_f = (shoot_control.shoot_rc->key.v & SHOOT_FRIC_DEC_KEYBOARD) ? 1 : 0;
    key_r = (shoot_control.shoot_rc->key.v & SHOOT_BURST_KEYBOARD) ? 1 : 0;

    if (shoot_aux_enable && ((key_r && !shoot_control.last_key_r) || kb_cmd->burst_toggle))
    {
        shoot_control.burst_mode = !shoot_control.burst_mode;
    }

    if (shoot_aux_enable)
    {
        if ((key_c && !shoot_control.last_key_c) || kb_cmd->high_freq_toggle)
        {
            shoot_control.high_freq_flag = !shoot_control.high_freq_flag;
        }

        if (kb_cmd->fric_speed_adj == 1u)
        {
            shoot_control.fric_speed_base += FRIC_SPEED_ADJUST_STEP;
        }
        else if (kb_cmd->fric_speed_adj == 2u)
        {
            shoot_control.fric_speed_base -= FRIC_SPEED_ADJUST_STEP;
        }
        else if (kb_cmd->fric_speed_adj == 3u && shoot_control.arm_enable)
        {
            shoot_control.fric_speed_base += FRIC_SPEED_ADJUST_STEP;
        }
        else if (kb_cmd->fric_speed_adj == 4u && shoot_control.arm_enable)
        {
            shoot_control.fric_speed_base -= FRIC_SPEED_ADJUST_STEP;
        }
        else if (key_f && !shoot_control.last_key_f)
        {
            if (shoot_control.shoot_rc->key.v & SHOOT_FRIC_INC_KEYBOARD)
            {
                shoot_control.fric_speed_base += FRIC_SPEED_ADJUST_STEP;
            }
            else
            {
                shoot_control.fric_speed_base -= FRIC_SPEED_ADJUST_STEP;
            }
        }

        /* 硬限幅 fric_speed_base */
        if (shoot_control.fric_speed_base < FRIC_SPEED_LOW)
        {
            shoot_control.fric_speed_base = FRIC_SPEED_LOW;
        }
        else if (shoot_control.fric_speed_base > FRIC_SPEED_HIGH)
        {
            shoot_control.fric_speed_base = FRIC_SPEED_HIGH;
        }
    }

    shoot_control.last_key_q = key_q;
    shoot_control.last_key_c = key_c;
    shoot_control.last_key_f = key_f;
    shoot_control.last_key_r = key_r;

    fric_ready = (bool_t)(shoot_control.arm_enable
                          && fabs(shoot_control.fric1_motor_measure->speed_rpm - shoot_control.fric_speed_set) < FRIC_READY_SPEED_ERR
                          && fabs((fp32)shoot_control.fric1_motor_measure->speed_rpm) > FRIC_READY_MIN_SPEED);

    continue_req = (bool_t)(shoot_control.arm_enable
                            && ((shoot_control.press_l_time == MOUSE_LONG_PRESS_TIME)
                                || (shoot_control.press_r_time == MOUSE_LONG_PRESS_TIME)
                                || (shoot_control.rc_s_time == RC_S_LONG_TIME)));
    reverse_req = (bool_t)(shoot_control.arm_enable
                           && (((shoot_control.shoot_rc->key.v & SHOOT_REVERSE_KEYBOARD) != 0)
                               || kb_cmd->reverse_trigger));

    allow_fire = 1;

#if SHOOT_DEBUG_MODE == 0
    {
        fp32 effective_limit = shoot_control.burst_mode ? HEAT_LIMIT_BURST : HEAT_LIMIT_SAFE;
        if (shoot_control.local_heat + HEAT_PER_BULLET > effective_limit)
        {
            allow_fire = 0;
        }
    }
#endif

    if (gimbal_cmd_to_shoot_stop())
    {
        shoot_control.arm_enable = 0;
    }

    shoot_control.single_fire_req = (bool_t)(fire_cmd_edge && allow_fire);
    shoot_control.continue_req = (bool_t)(continue_req && allow_fire);
    shoot_control.reverse_req = reverse_req;

    /* ====== 以上为输入处理（保留原样） ====== */
    /* ====== 以下为 HUST 风格模式转换（替代 shoot_logic.c） ====== */

    {
        static bool_t last_reverse_req = 0;
        bool_t reverse_edge;

        reverse_edge = (bool_t)(reverse_req && !last_reverse_req);

        if (!shoot_control.arm_enable)
        {
            /* HUST Shoot_Powerdown: 全部停止 */
            shoot_control.shoot_mode = SHOOT_STOP;
        }
        else if (!fric_ready)
        {
            /* 摩擦轮未就绪: 锁住拨弹位置等待 */
            shoot_control.shoot_mode = SHOOT_READY_FRIC;
            shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
        }
        else if (shoot_control.continue_req)
        {
            /* 连发请求: 跳过位置环，速度直驱 (HUST ShootContinue) */
            if (shoot_control.shoot_mode != SHOOT_CONTINUE_BULLET)
            {
                /* 进入连发时记录起始位置，用于编码器计数 */
                shoot_control.trigger_ecd_last_fire = shoot_control.trigger_ecd_fdb;
                shoot_clear_trigger_pid_state();
            }
            shoot_control.shoot_mode = SHOOT_CONTINUE_BULLET;
        }
        else if (shoot_control.shoot_mode == SHOOT_CONTINUE_BULLET)
        {
            /* 松开触发，退出连发: 锁住当前位置 */
            shoot_control.shoot_mode = SHOOT_READY_BULLET;
            shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
            shoot_clear_trigger_pid_state();
        }
        else if (reverse_edge)
        {
            /* 手动反转: 位置目标退一格 (HUST ReverseRotation) */
            shoot_control.shoot_mode = SHOOT_BULLET;
            shoot_control.trigger_ecd_set -= (int32_t)TRIGGER_ONEGRID;
            shoot_control.reverse_flag = 1;
        }
        else if (shoot_control.single_fire_req
                 && shoot_control.shoot_mode == SHOOT_READY_BULLET)
        {
            /* 单发: 位置目标进一格 (HUST MirocPosition) */
            shoot_control.shoot_mode = SHOOT_BULLET;
            shoot_control.trigger_ecd_set += (int32_t)TRIGGER_ONEGRID;
            shoot_control.reverse_flag = 0;
        }
        else if (shoot_control.shoot_mode == SHOOT_BULLET)
        {
            /* 正在步进中: 检查是否到位 */
            int32_t pos_err = shoot_control.trigger_ecd_set - shoot_control.trigger_ecd_fdb;
            int32_t abs_err = (pos_err >= 0) ? pos_err : -pos_err;
            if (abs_err < (int32_t)TRIGGER_POS_THRESHOLD)
            {
                /* 到位 */
                if (!shoot_control.reverse_flag)
                {
                    /* 正向到位 = 成功发射一颗弹丸 */
                    shoot_control.bullet_fired_count++;
                    shoot_control.local_heat += HEAT_PER_BULLET;
                }
                shoot_control.reverse_flag = 0;
                shoot_control.shoot_mode = SHOOT_READY_BULLET;
            }
        }
        else if (shoot_control.shoot_mode != SHOOT_BULLET)
        {
            /* 默认就绪 */
            shoot_control.shoot_mode = SHOOT_READY_BULLET;
        }

        last_reverse_req = reverse_req;
    }

    last_s = shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL];
}

/**
  * @brief  数据反馈更新
  *
  *   改动点: 速度滤波去掉 MOTOR_RPM_TO_SPEED，现在 shoot_control.speed 单位为 rpm
  *   其余全部保留原样
  */
static void shoot_feedback_update(void)
{
    int16_t ecd_delta;
#if VT03_ENABLE
    const keyboard_cmd_t *cmd;
#endif
    static fp32 speed_fliter_1 = 0.0f;
    static fp32 speed_fliter_2 = 0.0f;
    static fp32 speed_fliter_3 = 0.0f;

    //二阶 Butterworth 滤波器系数
    static const fp32 fliter_num[3] = {1.725709860247969f, -0.75594777109163436f, 0.030237910843665373f};

    //滤波（改动: 去掉 * MOTOR_RPM_TO_SPEED，直接用 rpm）
    speed_fliter_1 = speed_fliter_2;
    speed_fliter_2 = speed_fliter_3;
    speed_fliter_3 = speed_fliter_2 * fliter_num[0] + speed_fliter_1 * fliter_num[1] + (fp32)shoot_control.shoot_motor_measure->speed_rpm * fliter_num[2];
    shoot_control.speed = speed_fliter_3;

    ecd_delta = shoot_control.shoot_motor_measure->ecd - shoot_control.shoot_motor_measure->last_ecd;

    //多圈累计
    if (ecd_delta > HALF_ECD_RANGE)
    {
        shoot_control.ecd_count--;
        shoot_control.trigger_ecd_total_count--;
    }
    else if (ecd_delta < -HALF_ECD_RANGE)
    {
        shoot_control.ecd_count++;
        shoot_control.trigger_ecd_total_count++;
    }

    if (shoot_control.ecd_count == FULL_COUNT)
    {
        shoot_control.ecd_count = -(FULL_COUNT - 1);
    }
    else if (shoot_control.ecd_count == -FULL_COUNT)
    {
        shoot_control.ecd_count = FULL_COUNT - 1;
    }

    //计算连续角度
    shoot_control.angle = (shoot_control.ecd_count * ECD_RANGE + shoot_control.shoot_motor_measure->ecd) * MOTOR_ECD_TO_ANGLE;
    shoot_control.trigger_ecd_fdb = (shoot_control.trigger_ecd_total_count * ECD_RANGE + shoot_control.shoot_motor_measure->ecd);

    //微动开关
    shoot_control.key = BUTTEN_TRIG_PIN;
    shoot_control.microswitch_on = (bool_t)(shoot_control.key == SWITCH_TRIGGER_ON);
    //鼠标按键
    shoot_control.last_press_l = shoot_control.press_l;
    shoot_control.last_press_r = shoot_control.press_r;
    shoot_control.press_l = shoot_control.shoot_rc->mouse.press_l;
    shoot_control.press_r = shoot_control.shoot_rc->mouse.press_r;

    /* VT03 trigger 同步 */
#if VT03_ENABLE
    cmd = get_keyboard_cmd();
    if (!toe_is_error(VT03_TOE))
    {
        if (cmd->vt03_trigger)
        {
            shoot_control.press_l = 1;
        }
    }
#endif

    //鼠标左键长按计时
    if (shoot_control.press_l)
    {
        if (shoot_control.press_l_time < MOUSE_LONG_PRESS_TIME)
        {
            shoot_control.press_l_time++;
        }
    }
    else
    {
        shoot_control.press_l_time = 0;
    }

    if (shoot_control.press_r)
    {
        if (shoot_control.press_r_time < MOUSE_LONG_PRESS_TIME)
        {
            shoot_control.press_r_time++;
        }
    }
    else
    {
        shoot_control.press_r_time = 0;
    }

    //遥控器拨杆持续按下计时
    if (shoot_control.arm_enable && switch_is_down(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]))
    {
        if (shoot_control.rc_s_time < RC_S_LONG_TIME)
        {
            shoot_control.rc_s_time++;
        }
    }
    else
    {
        shoot_control.rc_s_time = 0;
    }
}

/* 注意: trigger_motor_turn_back() 和 shoot_bullet_control() 已删除 */
/* 堵转检测和反转逻辑已简化为 shoot_set_mode() 中的一格步退          */

void shoot_get_fric_current(int16_t *fric1, int16_t *fric2)
{
    if (fric1 != NULL)
    {
        *fric1 = shoot_control.fric1_given_current;
    }

    if (fric2 != NULL)
    {
        *fric2 = shoot_control.fric2_given_current;
    }
}
```

### 提交

```bash
git add application/shoot.c
git commit -m "refactor(shoot): replace control core with HUST cascade PID (rpm units)"
```

---

## Task 3: 删除 shoot_logic 模块

### Step 1: 从 Keil 工程移除

在 `MDK-ARM/standard_robot.uvprojx` 中找到并删除以下 4 行 XML：

```xml
              <File>
                <FileName>shoot_logic.c</FileName>
                <FileType>1</FileType>
                <FilePath>..\application\shoot_logic.c</FilePath>
              </File>
```

搜索关键字 `shoot_logic` 即可定位。删除整个 `<File>...</File>` 块。

### Step 2: 删除源文件

```bash
rm application/shoot_logic.c application/shoot_logic.h
```

### 提交

```bash
git add -u application/shoot_logic.c application/shoot_logic.h
git add MDK-ARM/standard_robot.uvprojx
git commit -m "refactor(shoot): remove shoot_logic module"
```

---

## Task 4: 编译验证

用 Keil MDK 打开 `MDK-ARM/standard_robot.uvprojx`，执行 Rebuild All。

**预期结果:** 0 Error。

**可能的 Warning（可忽略）:**
- `MOTOR_RPM_TO_SPEED` defined but not used（shoot.h 保留定义但 shoot.c 不再使用）

**如果出现编译错误，按以下排查:**

| 错误信息 | 原因 | 修复 |
|---------|------|------|
| `CONTINUE_TRIGGER_SPEED undeclared` | 其他文件引用了旧宏 | 搜索全项目替换为 `PULLER_SPEED_NORMAL` |
| `READY_TRIGGER_SPEED undeclared` | 同上 | 该宏已在新 shoot.h 中删除，删掉引用 |
| `SHOOT_DONE undeclared` | 其他文件引用了旧枚举 | 搜索全项目，只在 shoot.c/h 中有，应已处理 |
| `shoot_logic.h not found` | 其他文件 include 了 | 只有 shoot.c 和 test 文件 include，应已处理 |
| `block_time` / `move_flag` 等未定义 | usb_task.c 遥测导出引用了删除的字段 | 从 usb_task.c 中删除对应遥测行 |

编译通过后提交：

```bash
git commit --allow-empty -m "build: compile verified after HUST control core replacement"
```

---

## Task 5: 硬件验证清单

烧录后逐项测试：

| # | 测试 | 操作 | 预期结果 |
|---|------|------|---------|
| 1 | 停止态 | 上电，不使能 | 摩擦轮不转，拨弹不动，激光关 |
| 2 | 使能 | VT13 按键 / 遥控 s1 上拨 | 摩擦轮开始转，激光开，mode=1(READY_FRIC) |
| 3 | 就绪 | 等摩擦轮到速 (~2s) | mode 变为 2(READY_BULLET)，拨弹锁住不动 |
| 4 | 单发 | 单击触发 | 拨弹转一格(36864 ecd)后停住，mode 3→2 |
| 5 | 连发 | 长按触发 (>100ms) | 拨弹持续转，~3500 rpm，mode=4 |
| 6 | 高频 | 连发中按 C | 速度升到 ~4500 rpm |
| 7 | 停连发 | 松开触发 | 拨弹迅速停住，mode→2 |
| 8 | 反转 | 按 G | 拨弹倒退一格 |
| 9 | 摩擦轮调速 | F / Shift+F | 摩擦轮 ±100 rpm |
| 10 | 热量门控 | 连续单发直到 local_heat>80 | 拒绝发射 |
| 11 | 爆发 | 按 R 后继续发射 | 可射到 heat=180 |
| 12 | 关闭 | 再按 VT13 使能键 | 全部停止，摩擦轮减速 |

**遥测监控列（wifi://192.168.4.1）:**

| 列号 | 变量 | 单位变化说明 |
|------|------|------------|
| 47 | shoot_mode | 值域从 0-6 缩小为 0-4 |
| 54 | given_current | 不变 |
| 57 | speed | **单位变了: 原来 rad/s (~0-15)，现在 rpm (~0-4500)** |
| 58 | speed_set | **单位变了: 同上** |
| 59 | trigger_ecd_fdb | 不变 |
| 60 | trigger_ecd_set | 不变 |
| 61 | local_heat | 不变 |
| 62 | bullet_fired_count | 不变 |

---

## 改动文件汇总

| 文件 | 操作 | 说明 |
|------|------|------|
| `application/shoot.h` | 整体替换 | PID 参数、宏、枚举、结构体 |
| `application/shoot.c` | 整体替换 | 控制核心重写，保留输入处理 |
| `application/shoot_logic.c` | 删除 | 不再需要 |
| `application/shoot_logic.h` | 删除 | 不再需要 |
| `MDK-ARM/standard_robot.uvprojx` | 删除 shoot_logic 条目 | 4 行 XML |

**不要改动的文件:** `usb_task.c`、`gimbal_task.c`、`CAN_receive.c`、`pid.c`、`keyboard_action.c` 等所有其他文件。
