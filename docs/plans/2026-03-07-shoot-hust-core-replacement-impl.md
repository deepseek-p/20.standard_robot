# Shoot HUST Control Core Replacement — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the shoot control core with HUST_Infantry_2023's cascade PID approach (rpm units, direct speed continuous fire, simple reverse) while keeping the existing VT13/RC/keyboard operation interface.

**Architecture:** Remove `shoot_logic.c/h` state machine. Inline mode transitions into `shoot_set_mode()`. Speed loop feedback changed from rad/s to raw rpm. Continuous fire bypasses position loop. Reverse simplified to one-grid position step.

**Tech Stack:** STM32F407 + FreeRTOS, Keil MDK (armcc), CAN bus motor control

**Design doc:** `docs/plans/2026-03-06-shoot-hust-control-core-replacement.md`

---

### Task 1: Update shoot.h — PID parameters, macros, enum, struct

**Files:**
- Modify: `application/shoot.h`

**Step 1: Change speed loop PID to HUST values (rpm units)**

Replace lines 93-102:

```c
//拨弹速度环（对齐 HUST rpm 单位）
#define TRIGGER_SPD_KP              6.2f
#define TRIGGER_SPD_KI              3.2f
#define TRIGGER_SPD_KD              0.0f
#define TRIGGER_SPD_MAX_OUT         10000.0f
#define TRIGGER_SPD_MAX_IOUT        1000.0f
#define TRIGGER_SPD_DEADZONE        50.0f
#define TRIGGER_SPD_I_L             100.0f
#define TRIGGER_SPD_I_U             200.0f
#define TRIGGER_SPD_RC_DF           0.5f
```

**Step 2: Replace continuous fire speed macros (rpm instead of rad/s)**

Replace lines 54-55:

```c
#define PULLER_SPEED_NORMAL         3500.0f
#define PULLER_SPEED_HIGH_FREQ      4500.0f
```

**Step 3: Remove obsolete macros**

Delete these lines entirely:
- Line 62-65: `BLOCK_TRIGGER_SPEED`, `BLOCK_TIME`, `REVERSE_TIME`, `REVERSE_SPEED_LIMIT`
- Line 108-112: `TRIGGER_POS_MAX_OUT_HOLD`, `TRIGGER_POS_MAX_IOUT_HOLD`, `TRIGGER_POS_MAX_OUT_FIRE`, `TRIGGER_CAN_CURRENT_LIMIT`, `MAX_REVERSE_COUNT`

Add replacement current limit (HUST uses OutMax=10000 which caps naturally, but keep explicit limit for safety):

```c
#define TRIGGER_CAN_CURRENT_LIMIT   10000   // 对齐 HUST speed PID OutMax
```

**Step 4: Simplify shoot_mode_e enum**

Replace lines 122-130:

```c
typedef enum
{
    SHOOT_STOP = 0,
    SHOOT_READY_FRIC,
    SHOOT_READY_BULLET,
    SHOOT_BULLET,
    SHOOT_CONTINUE_BULLET,
} shoot_mode_e;
```

Note: `SHOOT_BULLET` no longer skips to `= 4`. This is fine because shoot_mode is only used internally (telemetry exports the numeric value, meaning changes but no code dependency on specific values).

**Step 5: Clean shoot_control_t struct**

Remove these fields:
- `uint16_t block_time;`
- `uint16_t reverse_time;`
- `uint8_t reverse_count;`
- `bool_t move_flag;`
- `bool_t jam_reverse_req;`
- `bool_t jam_abandon_req;`
- `uint8_t arm_state_dbg;`
- `uint8_t trigger_exec_state_dbg;`

The final struct should be:

```c
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
```

**Step 6: Commit**

```bash
git add application/shoot.h
git commit -m "refactor(shoot): align PID params and types with HUST rpm units"
```

---

### Task 2: Rewrite shoot.c — remove shoot_logic, new control core

**Files:**
- Modify: `application/shoot.c`

This is the main task. Apply ALL sub-steps before attempting compile.

**Step 2a: Remove shoot_logic dependency**

1. Delete line 36: `#include "shoot_logic.h"`
2. Delete lines 72-73:
   ```c
   static shoot_command_state_t shoot_cmd_state;
   static shoot_executor_state_t shoot_exec_state;
   ```
3. Delete the forward declaration of `trigger_motor_turn_back` (lines 57-62)
4. Delete the forward declaration of `shoot_bullet_control` (lines 64-69)

**Step 2b: Change speed feedback from rad/s to rpm**

In `shoot_feedback_update()`, change line 511 from:
```c
speed_fliter_3 = speed_fliter_2 * fliter_num[0] + speed_fliter_1 * fliter_num[1] + (shoot_control.shoot_motor_measure->speed_rpm * MOTOR_RPM_TO_SPEED) * fliter_num[2];
```
to:
```c
speed_fliter_3 = speed_fliter_2 * fliter_num[0] + speed_fliter_1 * fliter_num[1] + (fp32)shoot_control.shoot_motor_measure->speed_rpm * fliter_num[2];
```

**Step 2c: Rewrite shoot_set_mode() — replace shoot_command_logic_update with inline mode transitions**

Keep ALL input processing code (lines 337-472) unchanged. Replace lines 474-486 (the `shoot_command_logic_update` call and jam_req clearing) with:

```c
    /* --- Mode transition (replaces shoot_logic.c) --- */
    {
        static bool_t last_reverse_req = 0;
        bool_t reverse_edge;

        reverse_edge = (bool_t)(reverse_req && !last_reverse_req);

        if (!shoot_control.arm_enable)
        {
            shoot_control.shoot_mode = SHOOT_STOP;
        }
        else if (!fric_ready)
        {
            shoot_control.shoot_mode = SHOOT_READY_FRIC;
            shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
        }
        else if (shoot_control.continue_req)
        {
            if (shoot_control.shoot_mode != SHOOT_CONTINUE_BULLET)
            {
                shoot_control.trigger_ecd_last_fire = shoot_control.trigger_ecd_fdb;
                shoot_clear_trigger_pid_state();
            }
            shoot_control.shoot_mode = SHOOT_CONTINUE_BULLET;
        }
        else if (shoot_control.shoot_mode == SHOOT_CONTINUE_BULLET)
        {
            /* Exit continuous: lock position */
            shoot_control.shoot_mode = SHOOT_READY_BULLET;
            shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
            shoot_clear_trigger_pid_state();
        }
        else if (reverse_edge)
        {
            shoot_control.shoot_mode = SHOOT_BULLET;
            shoot_control.trigger_ecd_set -= (int32_t)TRIGGER_ONEGRID;
            shoot_control.reverse_flag = 1;
        }
        else if (shoot_control.single_fire_req
                 && shoot_control.shoot_mode == SHOOT_READY_BULLET)
        {
            shoot_control.shoot_mode = SHOOT_BULLET;
            shoot_control.trigger_ecd_set += (int32_t)TRIGGER_ONEGRID;
            shoot_control.reverse_flag = 0;
        }
        else if (shoot_control.shoot_mode == SHOOT_BULLET)
        {
            /* Check arrival */
            int32_t pos_err = shoot_control.trigger_ecd_set - shoot_control.trigger_ecd_fdb;
            int32_t abs_err = (pos_err >= 0) ? pos_err : -pos_err;
            if (abs_err < (int32_t)TRIGGER_POS_THRESHOLD)
            {
                if (!shoot_control.reverse_flag)
                {
                    shoot_control.bullet_fired_count++;
                    shoot_control.local_heat += HEAT_PER_BULLET;
                }
                shoot_control.reverse_flag = 0;
                shoot_control.shoot_mode = SHOOT_READY_BULLET;
            }
        }
        else if (shoot_control.shoot_mode != SHOOT_BULLET)
        {
            shoot_control.shoot_mode = SHOOT_READY_BULLET;
        }

        last_reverse_req = reverse_req;
    }
```

**Step 2d: Rewrite shoot_control_loop() — new control output section**

Replace the entire body of `shoot_control_loop()` (lines 181-329) with:

```c
int16_t shoot_control_loop(void)
{
    uint16_t referee_heat_now;
    shoot_mode_e prev_mode;

    shoot_feedback_update();
    prev_mode = shoot_control.shoot_mode;

    /* Heat cooling: 1ms tick */
    if (shoot_control.local_heat > 0.0f)
    {
        shoot_control.local_heat -= HEAT_COOL_RATE / 1000.0f;
        if (shoot_control.local_heat < 0.0f)
        {
            shoot_control.local_heat = 0.0f;
        }
    }

    /* Referee sync */
    get_shoot_heat0_limit_and_heat0(&shoot_control.heat_limit, &referee_heat_now);
    if (referee_heat_now != shoot_control.referee_heat)
    {
        shoot_control.referee_heat = referee_heat_now;
        shoot_control.local_heat = (fp32)referee_heat_now;
    }

    shoot_set_mode();

    /* PID reset on certain transitions */
    if (shoot_control.shoot_mode != prev_mode)
    {
        if (prev_mode == SHOOT_STOP || prev_mode == SHOOT_READY_FRIC)
        {
            shoot_clear_trigger_pid_state();
        }
    }

    /* --- Control output per mode --- */
    if (shoot_control.shoot_mode == SHOOT_STOP)
    {
        shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
        shoot_control.speed_set = 0.0f;
        shoot_control.given_current = 0;
        shoot_control.fric_speed_base = FRIC_SPEED_LOW;
        shoot_control.high_freq_flag = 0;
        shoot_control.burst_mode = 0;
    }
    else if (shoot_control.shoot_mode == SHOOT_READY_FRIC)
    {
        shoot_control.speed_set = 0.0f;
        shoot_control.given_current = 0;
    }
    else if (shoot_control.shoot_mode == SHOOT_READY_BULLET)
    {
        /* Position cascade PID hold (HUST style: full params, setpoint locked) */
        shoot_control.speed_set = PID_enhanced_calc(&shoot_control.trigger_pos_pid,
                                                    (fp32)shoot_control.trigger_ecd_fdb,
                                                    (fp32)shoot_control.trigger_ecd_set);
        shoot_control.given_current = (int16_t)PID_enhanced_calc(&shoot_control.trigger_spd_pid,
                                                                  shoot_control.speed,
                                                                  shoot_control.speed_set);
    }
    else if (shoot_control.shoot_mode == SHOOT_BULLET)
    {
        /* Position cascade PID step (single fire / reverse) */
        shoot_control.speed_set = PID_enhanced_calc(&shoot_control.trigger_pos_pid,
                                                    (fp32)shoot_control.trigger_ecd_fdb,
                                                    (fp32)shoot_control.trigger_ecd_set);
        shoot_control.given_current = (int16_t)PID_enhanced_calc(&shoot_control.trigger_spd_pid,
                                                                  shoot_control.speed,
                                                                  shoot_control.speed_set);
    }
    else if (shoot_control.shoot_mode == SHOOT_CONTINUE_BULLET)
    {
        /* Bypass position loop — direct speed (HUST ShootContinue) */
        shoot_control.speed_set = shoot_control.high_freq_flag
                                  ? PULLER_SPEED_HIGH_FREQ
                                  : PULLER_SPEED_NORMAL;
        shoot_control.given_current = (int16_t)PID_enhanced_calc(&shoot_control.trigger_spd_pid,
                                                                  shoot_control.speed,
                                                                  shoot_control.speed_set);
        /* Encoder-based bullet counting during continuous fire */
        while ((shoot_control.trigger_ecd_fdb - shoot_control.trigger_ecd_last_fire)
                >= (int32_t)TRIGGER_ONEGRID)
        {
            shoot_control.trigger_ecd_last_fire += (int32_t)TRIGGER_ONEGRID;
            shoot_control.bullet_fired_count++;
            shoot_control.local_heat += HEAT_PER_BULLET;
        }
    }

    /* Laser */
    if (shoot_control.shoot_mode == SHOOT_STOP)
    {
        shoot_laser_off();
    }
    else
    {
        shoot_laser_on();
    }

    /* Current limit */
    shoot_control.given_current = (int16_t)fp32_constrain(
        (fp32)shoot_control.given_current,
        -(fp32)TRIGGER_CAN_CURRENT_LIMIT,
        (fp32)TRIGGER_CAN_CURRENT_LIMIT);

    /* Friction wheel */
    if (shoot_control.shoot_mode == SHOOT_STOP)
    {
        shoot_control.fric_speed_set = 0.0f;
    }
    else
    {
        shoot_control.fric_speed_set = shoot_control.fric_speed_base;
    }

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
```

**Step 2e: Delete obsolete functions**

Delete entirely:
- `trigger_motor_turn_back()` (old lines 601-649)
- `shoot_bullet_control()` (old lines 656-687)

**Step 2f: Clean shoot_init()**

Remove these lines from `shoot_init()`:
- `shoot_control.move_flag = 0;` (line 129)
- `shoot_control.block_time = 0;` (line 138)
- `shoot_control.reverse_time = 0;` (line 139)
- `shoot_control.reverse_count = 0;` (line 140)
- `shoot_control.jam_reverse_req = 0;` (line 161)
- `shoot_control.jam_abandon_req = 0;` (line 162)
- `shoot_control.arm_state_dbg = 0;` (line 164)
- `shoot_control.trigger_exec_state_dbg = 0;` (line 165)
- Lines 167-173 (the entire `shoot_logic_init` block):
  ```c
  shoot_logic_init(&shoot_cmd_state, ...);
  shoot_exec_state.trigger_ecd_fdb = ...;
  shoot_exec_state.trigger_ecd_set = ...;
  shoot_exec_state.trigger_ecd_last_fire = ...;
  ```

**Step 2g: Commit**

```bash
git add application/shoot.c
git commit -m "refactor(shoot): replace shoot_logic with HUST inline control core"
```

---

### Task 3: Remove shoot_logic from project build

**Files:**
- Modify: `MDK-ARM/standard_robot.uvprojx`
- Delete: `application/shoot_logic.c`, `application/shoot_logic.h`

**Step 1: Remove from Keil project XML**

In `MDK-ARM/standard_robot.uvprojx`, find and delete lines 894-897:
```xml
<File>
  <FileName>shoot_logic.c</FileName>
  <FileType>1</FileType>
  <FilePath>..\application\shoot_logic.c</FilePath>
</File>
```

**Step 2: Delete source files**

```bash
rm application/shoot_logic.c application/shoot_logic.h
```

**Step 3: Commit**

```bash
git add -u application/shoot_logic.c application/shoot_logic.h
git add MDK-ARM/standard_robot.uvprojx
git commit -m "refactor(shoot): remove shoot_logic module from build"
```

---

### Task 4: Compile verification

**Step 1: Build with Keil**

Open `MDK-ARM/standard_robot.uvprojx` in Keil MDK and rebuild all (Project → Rebuild All).

Expected: 0 errors. Warnings about unused `MOTOR_RPM_TO_SPEED` are acceptable.

**Step 2: If compile errors occur**

Common issues to check:
- Missing `CONTINUE_TRIGGER_SPEED` or `READY_TRIGGER_SPEED` references elsewhere → search and replace with new macro names
- `SHOOT_DONE` referenced in other files → search project-wide (exploration found it only in shoot.c/h)
- `shoot_logic.h` included elsewhere → only shoot.c and test file included it

**Step 3: Commit build success**

```bash
git commit --allow-empty -m "build: verified compile after HUST control core replacement"
```

---

### Task 5: Hardware validation

Flash firmware to robot and test each operation:

| # | Test | Procedure | Expected |
|---|------|-----------|----------|
| 1 | STOP mode | Power on, don't enable | Friction off, trigger silent, laser off |
| 2 | Arm enable | VT13 toggle or RC s1 UP | Friction starts spinning, laser on |
| 3 | Fric ready | Wait ~2s for friction | Telemetry shoot_mode transitions 1→2 |
| 4 | Single fire | Single click trigger | Trigger steps one grid, stops, mode 3→2 |
| 5 | Single fire x5 | 5 rapid clicks | 5 bullets fired, bullet_count increments by 5 |
| 6 | Continuous fire | Hold trigger >2s (or long press) | Trigger runs continuously at ~3500 rpm |
| 7 | High-freq toggle | Press C during continuous | Speed jumps to ~4500 rpm |
| 8 | Release continuous | Release trigger | Trigger stops promptly, mode → 2 |
| 9 | Reverse | Press G key | Trigger reverses one grid |
| 10 | Fric adjust | F key / Shift+F | Friction speed ±100 rpm |
| 11 | Heat gating | Fire until local_heat > 80 | Firing stops, allow_fire = 0 |
| 12 | Burst mode | Press R, fire past 80 heat | Fires up to 180 heat limit |
| 13 | Disable | VT13 toggle off | Everything stops, friction decelerates |

**Telemetry columns to monitor** (via MCP wifi://192.168.4.1):
- Col 47: shoot_mode (expect 0/1/2/3/4)
- Col 54: given_current (trigger motor)
- Col 57: speed (now in rpm, NOT rad/s — values ~0-4500 instead of ~0-15)
- Col 58: speed_set
- Col 59/60: trigger_ecd_fdb / trigger_ecd_set
- Col 61: local_heat
- Col 62: bullet_fired_count

**IMPORTANT telemetry change:** `speed` (col 57) and `speed_set` (col 58) are now in rpm instead of rad/s. Any analysis scripts or dashboards need to account for this unit change.

---

### Summary of all file changes

| File | Action |
|------|--------|
| `application/shoot.h` | Modify: PID params, macros, enum, struct |
| `application/shoot.c` | Modify: remove shoot_logic, rewrite control core |
| `application/shoot_logic.c` | Delete |
| `application/shoot_logic.h` | Delete |
| `MDK-ARM/standard_robot.uvprojx` | Modify: remove shoot_logic.c entry |
