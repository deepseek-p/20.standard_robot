# Shoot HUST Control Core Replacement Design

Date: 2026-03-06
Branch: shoot-state-machine-align-hust
Status: Approved

## Goal

Replace the current shoot control core with HUST_Infantry_2023's proven implementation while keeping the existing operation interface (RC, VT13/VT03, keyboard) unchanged.

## Problem

Current shoot module has comprehensive reliability issues:
- Single fire unreliable (position/speed loop unit mismatch affects damping)
- Continuous fire stuttering / jam handling issues
- Speed loop PID parameters calibrated in rad/s while position loop outputs rpm-scale values

## Approach: Graft HUST Core into Current Framework (Option A)

Keep `shoot.c` file structure and input handling functions, replace only the control core.

## Changes

### 1. Speed Feedback Unit: rad/s → rpm

In `shoot_feedback_update()`, remove `MOTOR_RPM_TO_SPEED` conversion:
```c
// Before: speed_rpm * MOTOR_RPM_TO_SPEED
// After:  speed_rpm (raw)
```
Keep Butterworth filter (HUST uses raw, but filter is beneficial for noise).

### 2. Speed Loop PID Parameters → HUST ID=3/4/14/46

| Parameter | Before (rad/s) | After (rpm) |
|-----------|----------------|-------------|
| Kp | 200.0 | 6.2 |
| Ki | 3.0 | 3.2 |
| DeadZone | 0.0 | 50.0 |
| IMax | 5000.0 | 1000.0 |
| OutMax | 10000.0 | 10000.0 |
| I_L / I_U | 100 / 200 | 100 / 200 |

Position loop parameters unchanged (already identical to HUST).

### 3. Mode System Simplification

Remove `SHOOT_DONE`. Final modes:
- SHOOT_STOP: zero current, lock position
- SHOOT_READY_FRIC: zero current, lock position, friction spinning up
- SHOOT_READY_BULLET: cascade PID hold, armed
- SHOOT_BULLET: position step one grid (single fire + reverse)
- SHOOT_CONTINUE_BULLET: bypass position loop, direct speed (HUST ShootContinue)

Mode transitions handled directly in `shoot_set_mode()`, replacing `shoot_logic.c`.

### 4. Single Fire (HUST Shoot_Fire_Cal)

On fire command:
- `trigger_ecd_set += one_grid`
- Position PID → Speed PID cascade drives motor
- When `|pos_err| < threshold`: done, return to READY_BULLET, increment bullet_count

### 5. Continuous Fire (HUST ShootContinue)

Bypass position loop entirely:
- `spd_pid.SetPoint = PullerSpeed` (3500 rpm normal, 4500 rpm high-freq)
- Bullet counting via encoder (more reliable than HUST's friction speed detection)
- On release: lock current position, return to READY_BULLET

### 6. Reverse (HUST Simple Flag)

On reverse command:
- `trigger_ecd_set -= one_grid`
- `reverse_flag = 1`
- When position arrives: clear flag, return to READY_BULLET

Remove: block_time, reverse_time, reverse_count, MAX_REVERSE_COUNT, jam_abandon.

### 7. READY Hold (HUST Approach)

Use full cascade PID with same parameters (no separate hold limits).
Remove `TRIGGER_POS_MAX_OUT_HOLD` and `TRIGGER_POS_MAX_IOUT_HOLD`.

## Files

### Delete
- `application/shoot_logic.c`
- `application/shoot_logic.h`

### Modify
- `application/shoot.h`: PID params, remove DONE/hold macros, clean struct
- `application/shoot.c`: rewrite control core, keep input handling
- `MDK-ARM/standard_robot.uvprojx`: remove shoot_logic.c

### Preserve Unchanged
- `shoot_feedback_update()`: encoder, microswitch, mouse/VT03 timing
- `shoot_set_mode()` input section: arm_enable, key mapping, fric adjust, heat gating
- Friction wheel PID calculation
- Heat management (local_heat + referee sync)

## Operation Interface (Unchanged)

| Operation | Mode | Motor Action |
|-----------|------|-------------|
| VT13/RC enable toggle | READY_FRIC → READY_BULLET | Friction spins up |
| Single click / RC down edge | BULLET | Step one grid |
| Long press / RC down hold | CONTINUE_BULLET | Continuous speed |
| Release | → READY_BULLET | Lock position |
| G key / reverse | BULLET (reverse) | Step back one grid |
| Disable toggle | STOP | All zero |
| C key | Toggle high-freq | PullerSpeed 3500↔4500 |
| F/Shift keys | Friction speed +-100 | Friction target change |
| R key | Burst mode toggle | Heat limit 80↔180 |

## Continuous Fire Speed (HUST Reference)

| Condition | PullerSpeed (rpm) |
|-----------|-------------------|
| Normal | 3500 |
| High-freq (C key) | 4500 |
