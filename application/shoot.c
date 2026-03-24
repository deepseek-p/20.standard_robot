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

static uint8_t fric_gear = 0;
static bool_t shoot_reverse_success_pulse = 0;
static const fp32 fric_speed_table[FRIC_GEAR_COUNT] = {
    FRIC_SPEED_GEAR_0, FRIC_SPEED_GEAR_1
};

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
    fric_gear = 0;
    shoot_control.fric_speed_base = fric_speed_table[0];
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
    shoot_reverse_success_pulse = 0;

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
        fric_gear = 0;
        shoot_control.fric_speed_base = fric_speed_table[0];
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

        /* 挡位切换：adj 1/3 升挡，adj 2/4 降挡 */
        if (kb_cmd->fric_speed_adj == 1u || (kb_cmd->fric_speed_adj == 3u && shoot_control.arm_enable))
        {
            if (fric_gear < FRIC_GEAR_COUNT - 1)
            {
                fric_gear++;
            }
        }
        else if (kb_cmd->fric_speed_adj == 2u || (kb_cmd->fric_speed_adj == 4u && shoot_control.arm_enable))
        {
            if (fric_gear > 0)
            {
                fric_gear--;
            }
        }
        else if (key_f && !shoot_control.last_key_f)
        {
            if (shoot_control.shoot_rc->key.v & SHOOT_FRIC_INC_KEYBOARD)
            {
                /* Shift+F: 升挡 */
                if (fric_gear < FRIC_GEAR_COUNT - 1)
                {
                    fric_gear++;
                }
            }
            else
            {
                /* F alone: 降挡 */
                if (fric_gear > 0)
                {
                    fric_gear--;
                }
            }
        }
        shoot_control.fric_speed_base = fric_speed_table[fric_gear];
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
                else
                {
                    shoot_reverse_success_pulse = 1;
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

    //速度反馈: 直接用 rpm（对齐 HUST，不加滤波）
    shoot_control.speed = (fp32)shoot_control.shoot_motor_measure->speed_rpm;

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

    /* VT03 trigger: 直接用原始持续状态（非 keyboard_action 的 25ms 脉冲） */
#if VT03_ENABLE
    if (!toe_is_error(VT03_TOE))
    {
        if (vt03_ext.trigger)
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

shoot_ui_gear_e shoot_get_ui_gear(void)
{
    if (fric_gear == 0u)
    {
        return SHOOT_UI_GEAR_LOW;
    }

    return SHOOT_UI_GEAR_HIGH;
}

bool_t shoot_consume_reverse_success_pulse(void)
{
    bool_t pulse = shoot_reverse_success_pulse;
    shoot_reverse_success_pulse = 0;
    return pulse;
}
