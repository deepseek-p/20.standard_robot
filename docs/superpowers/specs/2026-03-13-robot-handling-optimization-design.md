# Robot Handling Optimization Design

**Date**: 2026-03-13
**Branch**: `codex/optimize-robot-handling`
**Status**: Draft

## Problem Statement

Three operational issues impacting robot handling quality:

1. **Chassis forward/backward reversal** — Pushing forward (RC stick or W key) causes the robot to move backward. Both input sources are affected, indicating a control logic issue rather than input mapping.
2. **Pitch oscillation** — Pitch axis exhibits mixed oscillation behavior (small high-frequency jitter + larger low-frequency swings) across all scenarios (idle, after input, during chassis motion).
3. **Chassis follow steady-state error** — After gimbal rotation in MID mode (FOLLOW_GIMBAL_YAW), chassis follows but never fully aligns, leaving a persistent angle offset that makes first-person control feel like walking at an angle.

## Design Decisions

### Approach chosen: Conservative (observe-then-tune)

- Definitive bugs (direction reversal) are fixed immediately in code
- Parameter-tuning issues (pitch oscillation, chassis follow) are diagnosed via MCP telemetry before any changes

This minimizes risk and follows "one variable at a time" debugging discipline.

---

## Fix 1: Chassis vx Direction Reversal (Immediate)

### Root cause

File: `application/chassis_task.c`, line 445-446:

```c
vx_set_channel = vx_channel * CHASSIS_VX_RC_SEN;       // no negation
vy_set_channel = vy_channel * -CHASSIS_VY_RC_SEN;      // negated
```

The vy axis already has sign correction (`-`), but vx does not. Under the current motor mounting direction, positive vx feeds into the mecanum formula (`chassis_task.c:603-606`) and produces front-wheel speeds that physically correspond to backward motion.

Both RC (ch[1]) and keyboard (W/S via vx_max/min_speed) converge at the same output point (`chassis_task.c:484`), so the reversal affects all input methods uniformly.

### Change

File: `application/chassis_task.c`, line 484:

```c
// Before:
*vx_set = chassis_move_rc_to_vector->chassis_cmd_slow_set_vx.out;

// After:
*vx_set = -chassis_move_rc_to_vector->chassis_cmd_slow_set_vx.out;
```

### Impact scope

All behaviour modes calling `chassis_rc_to_control_vector()`:
- `chassis_infantry_follow_gimbal_yaw_control()` — MID mode
- `chassis_hust_self_protect_control()` — UP mode (HUST spin)
- Any other mode using this function

The downstream rotation matrix (line 517-520) and mecanum formula (line 597-607) are unaffected — they operate on the already-corrected vx_set.

### Risk

Low. Both MID mode and UP mode (HUST self-protect) call `chassis_rc_to_control_vector()` at line 441/492, so the fix affects both uniformly. Verify both modes on-robot.

**Known pre-existing issue in HUST self-protect rotation** (`chassis_behaviour.c:444-445`): The rotation matrix uses `sin(+relative_angle)` but `cos(-relative_angle)`. While `cos(-x)=cos(x)` makes the cos term correct, the sin sign is opposite to the FOLLOW mode's rotation (which uses `sin(-relative_angle)`). This means the two modes rotate translation commands in opposite directions. This may surface as a separate direction issue in UP mode — out of scope for this fix but noted for awareness.

---

## Fix 2: Pitch Oscillation (Diagnose via Telemetry First)

### Analysis

Multiple potential causes identified in code review:

| Factor | Location | Current Value | Concern |
|--------|----------|---------------|---------|
| Encoder speed LPF | `gimbal_task.c:901` | alpha=0.05 (~8Hz cutoff) | ~20ms phase lag with Kp=800 speed loop |
| FF learning rate base | `gimbal_task.h:77` | GAMMA_BASE=0.005 | May cause feedforward overshoot |
| FF max step | `gimbal_task.h:83` | ALPHA_DOT_MAX=500 (0.5A/ms) | Aggressive — 5A change in 10ms |
| Angle loop Ki | `gimbal_task.h:70` | Ki=0 | Steady-state error relies entirely on feedforward |
| Speed loop Ki | `gimbal_task.h:37` | Ki=10 | Integrator wind-up when feedforward is unsettled |

### Action plan

1. **Observe** — Connect via MCP telemetry (`wifi://192.168.4.1`) and monitor:
   - Channel 56: `ff_b_hat` — feedforward convergence curve (raw, no scale)
   - Channel 32: `pitch_rel` — pitch relative angle (milli-scale)
   - Channel 33: `pitch_rel_set` — pitch relative setpoint (milli-scale)
   - Channel 34: `pitch_cur` — pitch given current

   **Note**: Pitch speed feedback (`motor_gyro`) is NOT in the current telemetry format. If speed waveform observation is needed, a new telemetry channel must be added first.

2. **Diagnose** — Determine if oscillation is:
   - Feedforward hunting (ff_b_hat oscillates around target)
   - Speed loop instability (high-frequency in pitch_gyro)
   - Both coupled

3. **Prescribe** — Based on data, candidate parameter adjustments:
   - LPF alpha: 0.05 → 0.10~0.15
   - GAMMA_BASE: 0.005 → 0.002
   - ALPHA_DOT_MAX: 500 → 150~250
   - Angle loop Ki: 0 → 0.1~0.3 (small, if needed)

---

## Fix 3: Chassis Follow Steady-State Error (Diagnose via Telemetry First)

### Analysis

Current PID: Kp=8.0, Ki=0.5, Kd=0, max_out=3.0 rad/s, max_iout=1.0 rad/s

At small angle errors (e.g., 2° = 0.035 rad), with 2ms control period:
- P output: 0.035 × 8.0 = 0.28 rad/s
- I accumulation rate: 0.035 × 0.5 = 0.0175 rad/s per 2ms cycle → reaches max_iout=1.0 in ~0.11s
- Total max output at 2° error: P+I_max = 0.28 + 1.0 = 1.28 rad/s

If chassis friction requires >1.28 rad/s to overcome at small angles, the PID cannot close the gap.

### Action plan

1. **Observe** — Monitor via MCP telemetry:
   - Channel 14: `rel` — chassis relative angle (milli-scale), steady-state offset magnitude
   - Channel 15: `rel_set` — chassis relative angle setpoint (milli-scale), target (should be 0)
   - Channel 13: `wz_set` — chassis wz setpoint (milli-scale), rotation command after follow PID

2. **Diagnose** — Determine if:
   - I term is hitting max_iout ceiling (if so, raise ceiling)
   - Motor dead zone prevents small wz_set from producing motion
   - Brake logic (line 531-540) is interfering at normal angles

3. **Prescribe** — Based on data, candidate adjustments:
   - max_iout: 1.0 → 1.5~2.5
   - Add Kd: 0 → 0.1~0.3 (if oscillation appears after raising max_iout)
   - Motor dead zone compensation (if identified)

---

## Verification Checklist

- [ ] Flash firmware with vx fix
- [ ] Verify forward/backward direction correct in MID mode (both RC and keyboard)
- [ ] Verify forward/backward direction correct in UP mode (HUST spin)
- [ ] Verify left/right direction unchanged
- [ ] (Optional) Add pitch speed (`motor_gyro`) telemetry channel if speed waveform needed
- [ ] Connect MCP telemetry and record pitch oscillation baseline
- [ ] Connect MCP telemetry and record chassis follow angle convergence
- [ ] Based on telemetry, decide on pitch parameter changes
- [ ] Based on telemetry, decide on chassis follow parameter changes

## Files Modified

| File | Change | Type |
|------|--------|------|
| `application/chassis_task.c:484` | Negate vx output | Bug fix |

## Files Potentially Modified (After Telemetry Diagnosis)

| File | Potential Change | Type |
|------|-----------------|------|
| `application/gimbal_task.c:901` | LPF alpha | Tuning |
| `application/gimbal_task.h:77-83` | FF learning rate parameters | Tuning |
| `application/gimbal_task.h:70` | Pitch angle loop Ki | Tuning |
| `application/chassis_task.h:133` | Follow PID max_iout | Tuning |
| `application/chassis_task.h:131` | Follow PID Kd | Tuning |
