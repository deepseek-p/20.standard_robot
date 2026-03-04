/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       shoot.c/h
  * @brief      �������?
  * @note
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. ���?
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

#define shoot_laser_on()    laser_on()      //���⿪���궨��
#define shoot_laser_off()   laser_off()     //����رպ궨��?
//΢������IO
#define BUTTEN_TRIG_PIN HAL_GPIO_ReadPin(BUTTON_TRIG_GPIO_Port, BUTTON_TRIG_Pin)

/**
  * @brief          ���״̬�����ã�ң�����ϲ�һ�ο��������ϲ��رգ��²�?�η���1�ţ�һֱ�����£���������䣬����?min׼��ʱ�������ӵ�
  * @param[in]      void
  * @retval         void
  */
static void shoot_set_mode(void);
/**
  * @brief          ������ݸ���?
  * @param[in]      void
  * @retval         void
  */
static void shoot_feedback_update(void);

/**
  * @brief          ��ת��ת����
  * @param[in]      void
  * @retval         void
  */
static void trigger_motor_turn_back(void);

/**
  * @brief          ������ƣ����Ʋ�������Ƕȣ����һ�η���?
  * @param[in]      void
  * @retval         void
  */
static void shoot_bullet_control(void);

shoot_control_t shoot_control;          //�������?

/**
  * @brief          �����ʼ������ʼ��PID��ң����ָ�룬���ָ��?
  * @param[in]      void
  * @retval         ���ؿ�
  */
void shoot_init(void)
{
    static const fp32 fric_speed_pid[3] = {FRIC_SPEED_PID_KP, FRIC_SPEED_PID_KI, FRIC_SPEED_PID_KD};

    shoot_control.shoot_mode = SHOOT_STOP;
    //ң����ָ��
    shoot_control.shoot_rc = get_remote_control_point();
    //���ָ��?
    shoot_control.shoot_motor_measure = get_trigger_motor_measure_point();
    shoot_control.fric1_motor_measure = get_fric1_motor_measure_point();
    shoot_control.fric2_motor_measure = get_fric2_motor_measure_point();

    //��ʼ��PID
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
    shoot_control.move_flag = 0;
    shoot_control.reverse_flag = 0;
    shoot_control.speed = 0.0f;
    shoot_control.speed_set = 0.0f;
    shoot_control.trigger_speed_set = 0.0f;
    shoot_control.key_time = 0;

    shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
    shoot_control.trigger_ecd_last_fire = shoot_control.trigger_ecd_fdb;

    shoot_control.block_time = 0;
    shoot_control.reverse_time = 0;
    shoot_control.reverse_count = 0;

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
}

/**
  * @brief          ���ѭ��?
  * @param[in]      void
  * @retval         ����can����ֵ
  */
int16_t shoot_control_loop(void)
{
    uint16_t referee_heat_now;

    shoot_feedback_update(); //��������

    // ��������Ԥ��ÿ������ȴ��1ms��
    if (shoot_control.local_heat > 0.0f)
    {
        shoot_control.local_heat -= HEAT_COOL_RATE / 1000.0f;
        if (shoot_control.local_heat < 0.0f)
        {
            shoot_control.local_heat = 0.0f;
        }
    }

    // ����ϵͳ����ͬ������У׼Ԥ��ֵ
    get_shoot_heat0_limit_and_heat0(&shoot_control.heat_limit, &referee_heat_now);
    if (referee_heat_now != shoot_control.referee_heat)
    {
        shoot_control.referee_heat = referee_heat_now;
        shoot_control.local_heat = (fp32)referee_heat_now;
    }

    shoot_set_mode();        //����״̬��

    if (shoot_control.shoot_mode == SHOOT_STOP)
    {
        shoot_control.speed_set = 0.0f;
        shoot_control.fric_speed_base = FRIC_SPEED_LOW;
        shoot_control.high_freq_flag = 0;
        shoot_control.burst_mode = 0;
        shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
    }
    else if (shoot_control.shoot_mode == SHOOT_READY_FRIC)
    {
        //���ò����ֵ��ٶ�
        shoot_control.speed_set = 0.0f;
        shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
    }
    else if (shoot_control.shoot_mode == SHOOT_READY_BULLET)
    {
        //READY_BULLET: soft position hold (HUST style), ecd_set not tracked.
        shoot_control.trigger_pos_pid.max_out = TRIGGER_POS_MAX_OUT_HOLD;
        shoot_control.speed_set = PID_enhanced_calc(&shoot_control.trigger_pos_pid,
                                                     shoot_control.trigger_ecd_fdb,
                                                     shoot_control.trigger_ecd_set);
    }
    else if (shoot_control.shoot_mode == SHOOT_READY)
    {
        //���ò����ֵ��ٶ�
        shoot_control.speed_set = 0.0f;
        shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
    }
    else if (shoot_control.shoot_mode == SHOOT_BULLET)
    {
        shoot_bullet_control();
    }
    else if (shoot_control.shoot_mode == SHOOT_CONTINUE_BULLET)
    {
        //���ò����ֵĲ����ٶ�,��������ת��ת����
        if (shoot_control.high_freq_flag)
        {
            shoot_control.trigger_speed_set = CONTINUE_TRIGGER_SPEED * 1.5f;
        }
        else
        {
            shoot_control.trigger_speed_set = CONTINUE_TRIGGER_SPEED;
        }

        shoot_control.speed_set = shoot_control.trigger_speed_set;
        trigger_motor_turn_back();

        while ((shoot_control.trigger_ecd_fdb - shoot_control.trigger_ecd_last_fire) >= TRIGGER_ONEGRID)
        {
            shoot_control.trigger_ecd_last_fire += TRIGGER_ONEGRID;
            shoot_control.bullet_fired_count++;
            shoot_control.local_heat += HEAT_PER_BULLET;
        }
    }
    else if (shoot_control.shoot_mode == SHOOT_DONE)
    {
        //DONE: soft position PID hold (same as READY_BULLET).
        shoot_control.trigger_pos_pid.max_out = TRIGGER_POS_MAX_OUT_HOLD;
        shoot_control.speed_set = PID_enhanced_calc(&shoot_control.trigger_pos_pid,
                                                     shoot_control.trigger_ecd_fdb,
                                                     shoot_control.trigger_ecd_set);
    }

    if (shoot_control.shoot_mode != SHOOT_CONTINUE_BULLET)
    {
        shoot_control.trigger_ecd_last_fire = shoot_control.trigger_ecd_fdb;
    }

    if (shoot_control.shoot_mode == SHOOT_STOP)
    {
        shoot_laser_off();
        shoot_control.given_current = 0;
    }
    else
    {
        shoot_laser_on();
        shoot_control.given_current = (int16_t)PID_enhanced_calc(&shoot_control.trigger_spd_pid, shoot_control.speed, shoot_control.speed_set);
        if (shoot_control.shoot_mode <= SHOOT_READY_FRIC)
        {
            shoot_control.given_current = 0;
        }
    }

    /* 反转覆盖：放在零电流门控之后，reverse 时重�?PID 覆盖 given_current */
    if (shoot_control.shoot_mode != SHOOT_STOP)
    {
        const keyboard_cmd_t *kb_cmd_reverse;
        kb_cmd_reverse = get_keyboard_cmd();
        if ((shoot_control.shoot_rc->key.v & SHOOT_REVERSE_KEYBOARD) || kb_cmd_reverse->reverse_trigger)
        {
            shoot_control.speed_set = -REVERSE_SPEED_LIMIT;
            shoot_control.given_current = (int16_t)PID_enhanced_calc(&shoot_control.trigger_spd_pid,
                                                                       shoot_control.speed,
                                                                       shoot_control.speed_set);
        }
    }

    if (shoot_control.shoot_mode == SHOOT_STOP)
    {
        shoot_control.fric_speed_set = 0.0f;
    }
    else
    {
        shoot_control.fric_speed_set = shoot_control.fric_speed_base;
    }

    // Ħ����1��ת��Ħ����2��ת
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
  * @brief          ���״̬�����ã�ң�����ϲ�һ�ο��������ϲ��رգ��²�?�η���1�ţ�һֱ�����£���������䣬����?min׼��ʱ�������ӵ�
  * @param[in]      void
  * @retval         void
  */
static void shoot_set_mode(void)
{
    static int8_t last_s = RC_SW_UP;
    bool_t key_q;
    bool_t key_c;
    bool_t key_f;
    bool_t key_r;
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

    /* ---- DBUS ·��: s[1] �ϲ� toggle��VT03 �� s[1]=DOWN���˶β������� ---- */
    if ((switch_is_up(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]) && !switch_is_up(last_s) && shoot_control.shoot_mode == SHOOT_STOP))
    {
        shoot_control.shoot_mode = SHOOT_READY_FRIC;
    }
    else if ((switch_is_up(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]) && !switch_is_up(last_s) && shoot_control.shoot_mode != SHOOT_STOP))
    {
        shoot_control.shoot_mode = SHOOT_STOP;
    }

    /* ---- Q �� / fn_1 �̰�: ����ϵͳ toggle��ͨ�� keyboard_cmd�� ---- */
    if (kb_cmd->shoot_toggle && shoot_aux_enable)
    {
        if (shoot_control.shoot_mode == SHOOT_STOP)
        {
            shoot_control.shoot_mode = SHOOT_READY_FRIC;
        }
        else
        {
            shoot_control.shoot_mode = SHOOT_STOP;
        }
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
        else if (kb_cmd->fric_speed_adj == 3u && shoot_control.shoot_mode != SHOOT_STOP)
        {
            shoot_control.fric_speed_base += FRIC_SPEED_ADJUST_STEP;
        }
        else if (kb_cmd->fric_speed_adj == 4u && shoot_control.shoot_mode != SHOOT_STOP)
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

        /* Ӳ�޷���fric_speed_base ���ó�����ȫ���� */
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

    if (shoot_control.shoot_mode == SHOOT_READY_FRIC
        && fabs(shoot_control.fric1_motor_measure->speed_rpm - shoot_control.fric_speed_set) < FRIC_READY_SPEED_ERR
        && fabs((fp32)shoot_control.fric1_motor_measure->speed_rpm) > FRIC_READY_MIN_SPEED)
    {
        shoot_control.shoot_mode = SHOOT_READY_BULLET;
    }
    else if (shoot_control.shoot_mode == SHOOT_READY_BULLET && shoot_control.key == SWITCH_TRIGGER_ON)
    {
        shoot_control.shoot_mode = SHOOT_READY;
    }
    else if (shoot_control.shoot_mode == SHOOT_READY && shoot_control.key == SWITCH_TRIGGER_OFF)
    {
        shoot_control.shoot_mode = SHOOT_READY_BULLET;
    }
    else if ((shoot_control.shoot_mode == SHOOT_READY || shoot_control.shoot_mode == SHOOT_READY_BULLET) && fire_cmd_edge)
    {
        //Allow VT03 trigger/mouse fire command directly from READY_BULLET.
        shoot_control.shoot_mode = SHOOT_BULLET;
    }
    else if (shoot_control.shoot_mode == SHOOT_DONE)
    {
        shoot_control.key_time++;
        if ((shoot_control.key_time > SHOOT_DONE_KEY_OFF_TIME && fabs(shoot_control.speed) < 0.3f)
            || shoot_control.key_time > 500)
        {
            shoot_control.key_time = 0;
            shoot_control.shoot_mode = SHOOT_READY_BULLET;
        }
    }

    if (shoot_control.shoot_mode > SHOOT_READY_FRIC)
    {
        //����һֱ��ס����������״̬ ���ǵ���
        if ((shoot_control.press_l_time == MOUSE_LONG_PRESS_TIME) || (shoot_control.press_r_time == MOUSE_LONG_PRESS_TIME) || (shoot_control.rc_s_time == RC_S_LONG_TIME))
        {
            shoot_control.shoot_mode = SHOOT_CONTINUE_BULLET;
        }
        else if (shoot_control.shoot_mode == SHOOT_CONTINUE_BULLET)
        {
            shoot_control.shoot_mode = SHOOT_READY_BULLET;
        }
    }

#if SHOOT_DEBUG_MODE == 0
    {
        fp32 effective_limit = shoot_control.burst_mode ? HEAT_LIMIT_BURST : HEAT_LIMIT_SAFE;
        if (shoot_control.local_heat + HEAT_PER_BULLET > effective_limit)
        {
            if (shoot_control.shoot_mode == SHOOT_BULLET || shoot_control.shoot_mode == SHOOT_CONTINUE_BULLET)
            {
                shoot_control.shoot_mode = SHOOT_READY_BULLET;
            }
        }
    }
#endif

    //�����̨״̬���ԣ�?���״ֱ̬�ӹر�״�?
    if (gimbal_cmd_to_shoot_stop())
    {
        shoot_control.shoot_mode = SHOOT_STOP;
    }

    last_s = shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL];
}
/**
  * @brief          ������ݸ���?
  * @param[in]      void
  * @retval         void
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

    //�����ֵ���ٶ��˲�һ��?
    static const fp32 fliter_num[3] = {1.725709860247969f, -0.75594777109163436f, 0.030237910843665373f};

    //���׵�ͨ�˲�
    speed_fliter_1 = speed_fliter_2;
    speed_fliter_2 = speed_fliter_3;
    speed_fliter_3 = speed_fliter_2 * fliter_num[0] + speed_fliter_1 * fliter_num[1] + (shoot_control.shoot_motor_measure->speed_rpm * MOTOR_RPM_TO_SPEED) * fliter_num[2];
    shoot_control.speed = speed_fliter_3;

    ecd_delta = shoot_control.shoot_motor_measure->ecd - shoot_control.shoot_motor_measure->last_ecd;

    //���Ȧ�����ã�?��Ϊ�������תһȦ��?��������?36Ȧ������������ݴ�������������ݣ����ڿ��������Ƕ�
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

    //���������Ƕ�
    shoot_control.angle = (shoot_control.ecd_count * ECD_RANGE + shoot_control.shoot_motor_measure->ecd) * MOTOR_ECD_TO_ANGLE;
    shoot_control.trigger_ecd_fdb = (fp32)(shoot_control.trigger_ecd_total_count * ECD_RANGE + shoot_control.shoot_motor_measure->ecd);

    //΢������
    shoot_control.key = BUTTEN_TRIG_PIN;
    //��갴��?
    shoot_control.last_press_l = shoot_control.press_l;
    shoot_control.last_press_r = shoot_control.press_r;
    shoot_control.press_l = shoot_control.shoot_rc->mouse.press_l;
    shoot_control.press_r = shoot_control.shoot_rc->mouse.press_r;

    /* VT03 trigger ��ͬ������ */
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

    //������ʱ
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

    //��������µ�ʱ���ʱ
    if (shoot_control.shoot_mode != SHOOT_STOP && switch_is_down(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]))
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

static void trigger_motor_turn_back(void)
{
    fp32 fallback_speed;

    fallback_speed = fabs(shoot_control.trigger_speed_set);
    if (fallback_speed < 1e-6f)
    {
        fallback_speed = REVERSE_SPEED_LIMIT;
    }

    if (shoot_control.block_time >= BLOCK_TIME)
    {
        if (shoot_control.reverse_count >= MAX_REVERSE_COUNT)
        {
            // Too many reversals: give up this shot, return to READY_BULLET
            shoot_control.speed_set = 0.0f;
            shoot_control.move_flag = 0;
            shoot_control.reverse_flag = 0;
            shoot_control.block_time = 0;
            shoot_control.reverse_time = 0;
            shoot_control.shoot_mode = SHOOT_READY_BULLET;
            return;
        }

        shoot_control.speed_set = -fallback_speed;

        if (!shoot_control.reverse_flag)
        {
            shoot_control.reverse_flag = 1;
            shoot_control.reverse_count++;
            if (shoot_control.move_flag)
            {
                shoot_control.trigger_ecd_set -= TRIGGER_ONEGRID;
            }
        }
    }

    if (fabs(shoot_control.speed) < BLOCK_TRIGGER_SPEED && shoot_control.block_time < BLOCK_TIME)
    {
        shoot_control.block_time++;
        shoot_control.reverse_time = 0;
    }
    else if (shoot_control.block_time == BLOCK_TIME && shoot_control.reverse_time < REVERSE_TIME)
    {
        shoot_control.reverse_time++;
    }
    else
    {
        shoot_control.block_time = 0;
        shoot_control.reverse_time = 0;
    }
}

/**
  * @brief          ������ƣ����Ʋ�������Ƕȣ����һ�η���?
  * @param[in]      void
  * @retval         void
  */
static void shoot_bullet_control(void)
{
    fp32 pos_err;

    // Restore full torque for firing
    shoot_control.trigger_pos_pid.max_out = TRIGGER_POS_MAX_OUT_FIRE;

    if (shoot_control.move_flag == 0)
    {
        shoot_control.trigger_ecd_set += TRIGGER_ONEGRID;
        shoot_control.move_flag = 1;
        shoot_control.reverse_count = 0;
    }

    shoot_control.speed_set = PID_enhanced_calc(&shoot_control.trigger_pos_pid, shoot_control.trigger_ecd_fdb, shoot_control.trigger_ecd_set);
    pos_err = shoot_control.trigger_ecd_set - shoot_control.trigger_ecd_fdb;

    if (fabs(pos_err) >= TRIGGER_POS_THRESHOLD)
    {
        trigger_motor_turn_back();
    }
    else
    {
        if (!shoot_control.reverse_flag)
        {
            shoot_control.bullet_fired_count++;
            shoot_control.local_heat += HEAT_PER_BULLET;
            shoot_control.trigger_ecd_last_fire = shoot_control.trigger_ecd_fdb;
        }

        shoot_control.move_flag = 0;
        shoot_control.reverse_flag = 0;
        shoot_control.block_time = 0;
        shoot_control.reverse_time = 0;
        shoot_control.shoot_mode = SHOOT_DONE;
    }
}

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

