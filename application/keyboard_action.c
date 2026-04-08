#include "keyboard_action.h"
#include "detect_task.h"

#if VT03_ENABLE
#include "vt03_link.h"
#endif

static key_state_t key_state = {0};
static keyboard_cmd_t kb_cmd = {0};
static kb_chassis_mode_e kb_chassis_mode = KB_CHASSIS_FOLLOW;
static uint8_t kb_zero_force = 0;
static uint8_t kb_cap_on = 0;
static uint16_t dial_repeat_cnt = 0u;
static uint8_t  trigger_hold_cnt = 0u;

void keyboard_action_init(void)
{
    uint8_t *p;
    uint16_t i;

    p = (uint8_t *)&key_state;
    for (i = 0; i < sizeof(key_state); i++)
    {
        p[i] = 0u;
    }

    p = (uint8_t *)&kb_cmd;
    for (i = 0; i < sizeof(kb_cmd); i++)
    {
        p[i] = 0u;
    }

    kb_chassis_mode = KB_CHASSIS_FOLLOW;
    kb_zero_force = 0u;
    kb_cap_on = 0u;
    dial_repeat_cnt = 0u;
    trigger_hold_cnt = 0u;
}

void keyboard_action_update(void)
{
    const RC_ctrl_t *rc;
    uint16_t cur;

    rc = get_remote_control_point();
    cur = rc->key.v;

    /* 16-bit keyboard edge detection */
    key_state.rising = (uint16_t)(cur & (uint16_t)(~key_state.prev));
    key_state.falling = (uint16_t)((uint16_t)(~cur) & key_state.prev);
    key_state.held = cur;
    key_state.prev = cur;

    /* VT03 extension key sampling */
#if VT03_ENABLE
    if (!toe_is_error(VT03_TOE))
    {
        key_state.fn1_cur = vt03_ext.fn_1;
        key_state.fn2_cur = vt03_ext.fn_2;
        key_state.trigger_cur = vt03_ext.trigger;
        key_state.pause_cur = vt03_ext.pause;
    }
    else
#endif
    {
        key_state.fn1_cur = 0u;
        key_state.fn2_cur = 0u;
        key_state.trigger_cur = 0u;
        key_state.pause_cur = 0u;
    }

    /* Clear command outputs and rebuild them this cycle */
    {
        uint8_t *p;
        uint16_t i;
        p = (uint8_t *)&kb_cmd;
        for (i = 0; i < sizeof(kb_cmd); i++)
        {
            p[i] = 0u;
        }
    }

    /* Keyboard mapped actions */

    /* CTRL: toggle chassis keyboard mode */
    if (key_state.rising & KEY_PRESSED_OFFSET_CTRL)
    {
        kb_cmd.chassis_mode_toggle = 1u;
        if (kb_chassis_mode == KB_CHASSIS_FOLLOW)
        {
            kb_chassis_mode = KB_CHASSIS_SPIN;
        }
        else
        {
            kb_chassis_mode = KB_CHASSIS_FOLLOW;
        }
    }

    /* SHIFT hold + pause short-press toggle */
    kb_cmd.cap_enable = ((cur & KEY_PRESSED_OFFSET_SHIFT) || kb_cap_on) ? 1u : 0u;

    /* Q: shoot system toggle */
    if (key_state.rising & KEY_PRESSED_OFFSET_Q)
    {
        kb_cmd.shoot_toggle = 1u;
    }

    /* C: high-frequency fire toggle */
    if (key_state.rising & KEY_PRESSED_OFFSET_C)
    {
        kb_cmd.high_freq_toggle = 1u;
    }

    /* R: burst mode toggle */
    if (key_state.rising & KEY_PRESSED_OFFSET_R)
    {
        kb_cmd.burst_toggle = 1u;
    }

    /* G: reverse trigger */
    if (key_state.rising & KEY_PRESSED_OFFSET_G)
    {
        kb_cmd.reverse_trigger = 1u;
    }

    /* F: fric speed adjust */
    if (key_state.rising & KEY_PRESSED_OFFSET_F)
    {
        if (cur & KEY_PRESSED_OFFSET_SHIFT)
        {
            kb_cmd.fric_speed_adj = 1u;
        }
        else
        {
            kb_cmd.fric_speed_adj = 2u;
        }
    }

    /* Z: zero-force toggle */
    if (key_state.rising & KEY_PRESSED_OFFSET_Z)
    {
        kb_cmd.zero_force_toggle = 1u;
        kb_zero_force = (uint8_t)!kb_zero_force;
    }

    /* B: UI refresh pulse */
    if (key_state.rising & KEY_PRESSED_OFFSET_B)
    {
        kb_cmd.ui_refresh = 1u;
    }

    /* VT03 trigger edge -> hold window for downstream sampling */
    if (key_state.trigger_cur && !key_state.trigger_prev)
    {
        trigger_hold_cnt = VT03_TRIGGER_HOLD_MS;
    }
    key_state.trigger_prev = key_state.trigger_cur;

    if (trigger_hold_cnt > 0u)
    {
        kb_cmd.vt03_trigger = 1u;
        trigger_hold_cnt--;
    }

    /* VT13 long-press decoding (fn_1/fn_2/pause) */

    /* fn_1: short->shoot toggle, long->high frequency toggle */
    if (key_state.fn1_cur)
    {
        key_state.fn1_lp.hold_cnt++;
        if (key_state.fn1_lp.hold_cnt >= BTN_LONG_PRESS_MS && !key_state.fn1_lp.long_fired)
        {
            key_state.fn1_lp.long_fired = 1u;
            kb_cmd.high_freq_toggle = 1u;
        }
    }
    else
    {
        if (key_state.fn1_prev && !key_state.fn1_lp.long_fired)
        {
            kb_cmd.shoot_toggle = 1u;
        }
        key_state.fn1_lp.hold_cnt = 0u;
        key_state.fn1_lp.long_fired = 0u;
    }
    key_state.fn1_prev = key_state.fn1_cur;

    /* fn_2: short->burst toggle, long->reverse trigger */
    if (key_state.fn2_cur)
    {
        key_state.fn2_lp.hold_cnt++;
        if (key_state.fn2_lp.hold_cnt >= BTN_LONG_PRESS_MS)
        {
            key_state.fn2_lp.long_fired = 1u;
            kb_cmd.reverse_trigger = 1u;
        }
    }
    else
    {
        if (key_state.fn2_prev && !key_state.fn2_lp.long_fired)
        {
            kb_cmd.burst_toggle = 1u;
        }
        key_state.fn2_lp.hold_cnt = 0u;
        key_state.fn2_lp.long_fired = 0u;
    }
    key_state.fn2_prev = key_state.fn2_cur;

    /* pause: short->cap toggle, long->reserved */
    if (key_state.pause_cur)
    {
        key_state.pause_lp.hold_cnt++;
        if (key_state.pause_lp.hold_cnt >= BTN_LONG_PRESS_MS && !key_state.pause_lp.long_fired)
        {
            key_state.pause_lp.long_fired = 1u;
            /* reserved */
        }
    }
    else
    {
        if (key_state.pause_prev && !key_state.pause_lp.long_fired)
        {
            kb_cap_on = (uint8_t)!kb_cap_on;
        }
        key_state.pause_lp.hold_cnt = 0u;
        key_state.pause_lp.long_fired = 0u;
    }
    key_state.pause_prev = key_state.pause_cur;

    /* VT03 dial: pulse trigger with repeat interval */
#if VT03_ENABLE
    if (!toe_is_error(VT03_TOE))
    {
        int16_t dial;
        dial = rc->rc.ch[4];
        if (dial > 200 || dial < -200)
        {
            if (dial_repeat_cnt == 0u)
            {
                kb_cmd.fric_speed_adj = (dial > 200) ? 3u : 4u;
                dial_repeat_cnt = DIAL_REPEAT_INTERVAL_MS;
            }
            else
            {
                dial_repeat_cnt--;
            }
        }
        else
        {
            dial_repeat_cnt = 0u;
        }
    }
#endif
}

const keyboard_cmd_t *get_keyboard_cmd(void)
{
    return &kb_cmd;
}

const key_state_t *get_key_state(void)
{
    return &key_state;
}

kb_chassis_mode_e get_kb_chassis_mode(void)
{
    return kb_chassis_mode;
}

uint8_t get_kb_zero_force(void)
{
    return kb_zero_force;
}
