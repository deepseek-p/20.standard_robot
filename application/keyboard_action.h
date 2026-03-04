#ifndef KEYBOARD_ACTION_H
#define KEYBOARD_ACTION_H

#include "struct_typedef.h"
#include "remote_control.h"
#include "uart_mode.h"

/* Long-press threshold in ms (keyboard_action_update runs at 1ms in gimbal_task) */
#define BTN_LONG_PRESS_MS  300u

/* Dial repeat interval in ms (prevents continuous accumulation at 1kHz) */
#define DIAL_REPEAT_INTERVAL_MS  100u

/* VT03 trigger hold window in ms (ensures downstream tasks sample the pulse) */
#define VT03_TRIGGER_HOLD_MS     25u

/* VT13 long-press state */
typedef struct
{
    uint16_t hold_cnt;
    uint8_t long_fired;  /* 1 = long-press already fired in current hold cycle */
} btn_lp_t;

/* Edge-detection state */
typedef struct
{
    uint16_t rising;   /* key bits 0->1 in current cycle */
    uint16_t falling;  /* key bits 1->0 in current cycle */
    uint16_t held;
    uint16_t prev;

    /* VT03 extra key states */
    uint8_t fn1_cur;
    uint8_t fn2_cur;
    uint8_t trigger_cur;
    uint8_t pause_cur;
    uint8_t fn1_prev;
    uint8_t fn2_prev;
    uint8_t trigger_prev;
    uint8_t pause_prev;

    /* VT13 long-press trackers */
    btn_lp_t fn1_lp;
    btn_lp_t fn2_lp;
    btn_lp_t pause_lp;
} key_state_t;

/* Command outputs (reset and rebuilt every cycle) */
typedef struct
{
    /* Chassis */
    uint8_t chassis_mode_toggle;
    uint8_t cap_enable;

    /* Shoot */
    uint8_t shoot_toggle;
    uint8_t high_freq_toggle;
    uint8_t fric_speed_adj;  /* 0:none 1:+100 2:-100 3:dial+ 4:dial- */
    uint8_t reverse_trigger;
    uint8_t burst_toggle;
    uint8_t ui_refresh;

    /* VT03-specific */
    uint8_t vt03_trigger;

    /* Safety */
    uint8_t zero_force_toggle;
} keyboard_cmd_t;

/* Chassis keyboard mode toggled by CTRL */
typedef enum
{
    KB_CHASSIS_FOLLOW = 0,
    KB_CHASSIS_SPIN,
} kb_chassis_mode_e;

extern void keyboard_action_init(void);
extern void keyboard_action_update(void);
extern const keyboard_cmd_t *get_keyboard_cmd(void);
extern const key_state_t *get_key_state(void);
extern kb_chassis_mode_e get_kb_chassis_mode(void);
extern uint8_t get_kb_zero_force(void);

#endif
