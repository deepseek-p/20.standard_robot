# Shoot HUST Trigger Refactor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Refactor the current shoot control into a HUST-like "command generation + trigger executor" structure, remove microswitch-driven main state transitions, and recover stable empty-load single-fire behavior.

**Architecture:** Keep the public `shoot` module API and existing telemetry field names, but split internal logic into an upper command layer and a lower trigger executor layer. The executor becomes the only owner of trigger target changes, shot completion, reverse recovery, and post-shot hold reattachment. Because this project has no unit-test harness for shoot, verification relies on Keil compile checks plus board telemetry acceptance.

**Tech Stack:** STM32F407 HAL, FreeRTOS (CMSIS-RTOS v1), C, Keil uVision, USB debug telemetry, `stm32-telemetry-mcp`

---

### Task 1: Add Internal Command/Executor State Model

**Files:**
- Modify: `application/shoot.h`
- Modify: `application/shoot.c`
- Reference only: `HUST_Infantry_2023/F405_Gimbal/Task/ActionTask.c`
- Reference only: `HUST_Infantry_2023/F405_Gimbal/Task/ShootTask.c`

**Step 1: Add executor-local enums and command flags to `shoot.h`**

Add internal state carriers without changing the public API:

```c
typedef enum
{
    SHOOT_ARM_STOP = 0,
    SHOOT_ARM_WAIT_FRIC,
    SHOOT_ARM_READY,
} shoot_arm_state_e;

typedef enum
{
    TRIGGER_EXEC_IDLE_HOLD = 0,
    TRIGGER_EXEC_STEP_ONE_GRID,
    TRIGGER_EXEC_CONTINUOUS_RUN,
    TRIGGER_EXEC_REVERSE_RECOVER,
} trigger_exec_state_e;
```

Extend `shoot_control_t` with fields such as:

```c
shoot_arm_state_e arm_state;
trigger_exec_state_e trigger_exec_state;

bool_t arm_enable;
bool_t single_fire_req;
bool_t continue_req;
bool_t reverse_req;
bool_t microswitch_state;
bool_t shot_finish_latch;
```

Keep `shoot_mode_e`, `shoot_init()`, `shoot_control_loop()`, and `shoot_get_fric_current()` unchanged.

**Step 2: Initialize the new fields in `shoot_init()`**

Set all new state fields to safe defaults:

- `arm_state = SHOOT_ARM_STOP`
- `trigger_exec_state = TRIGGER_EXEC_IDLE_HOLD`
- request flags cleared
- `microswitch_state` sampled from current GPIO

**Step 3: Run a compile check**

Run: open `MDK-ARM/standard_robot.uvprojx` in Keil and press `F7`

Expected:
- no new enum/type/field compile errors
- no missing initializer errors in `shoot_init()`

**Step 4: Commit**

```bash
git add application/shoot.h application/shoot.c
git commit -m "refactor: add shoot command and executor state model"
```

---

### Task 2: Split Input Interpretation Into a Command Layer

**Files:**
- Modify: `application/shoot.c`
- Reference only: `HUST_Infantry_2023/F405_Gimbal/Task/ActionTask.c`

**Step 1: Replace direct mode mutation in `shoot_set_mode()` with command generation**

Introduce a helper such as:

```c
static void shoot_update_command_layer(void)
{
    // update arm_enable, single_fire_req, continue_req, reverse_req
    // do not write trigger_ecd_set here
}
```

Responsibilities:

- determine whether shooting is enabled
- compute fire edge from mouse/RC/VT03/keyboard inputs
- compute long-press continue request
- compute manual reverse request
- compute arm readiness based on fric ready and stop conditions

**Step 2: Remove microswitch-driven main state transitions**

Delete logic equivalent to:

```c
else if (shoot_control.shoot_mode == SHOOT_READY_BULLET && shoot_control.key == SWITCH_TRIGGER_ON)
{
    shoot_control.shoot_mode = SHOOT_READY;
}
else if (shoot_control.shoot_mode == SHOOT_READY && shoot_control.key == SWITCH_TRIGGER_OFF)
{
    shoot_control.shoot_mode = SHOOT_READY_BULLET;
}
```

Microswitch must stay sampled, but no longer drives public READY state transitions.

**Step 3: Map arm status to public compatibility mode**

At the end of the command layer:

- stop -> `SHOOT_STOP`
- enabled but fric not ready -> `SHOOT_READY_FRIC`
- armed idle -> `SHOOT_READY_BULLET`

Do not enter `SHOOT_BULLET` or `SHOOT_CONTINUE_BULLET` here; that is executor-owned.

**Step 4: Run a compile check**

Run: Keil `F7`

Expected:
- build succeeds
- no dead references to removed `SHOOT_READY` transition paths

**Step 5: Commit**

```bash
git add application/shoot.c
git commit -m "refactor: split shoot command layer from input handling"
```

---

### Task 3: Add Trigger Executor Skeleton and External Mode Mapping

**Files:**
- Modify: `application/shoot.c`
- Reference only: `HUST_Infantry_2023/F405_Gimbal/Task/ShootTask.c`

**Step 1: Add executor helper functions**

Create helpers similar to:

```c
static void trigger_executor_update(void);
static void trigger_executor_idle_hold(void);
static void trigger_executor_step_one_grid(void);
static void trigger_executor_continuous_run(void);
static void trigger_executor_reverse_recover(void);
```

**Step 2: Move `trigger_ecd_set` ownership into the executor**

Rules:

- only executor functions may change `trigger_ecd_set`
- command layer may only set request flags
- `shoot_bullet_control()` and `trigger_motor_turn_back()` should be absorbed into executor functions or wrapped by them

**Step 3: Map executor state back to public `shoot_mode`**

Executor should drive:

- idle armed -> `SHOOT_READY_BULLET`
- stepping one grid -> `SHOOT_BULLET`
- continuous run -> `SHOOT_CONTINUE_BULLET`
- shot complete pulse -> `SHOOT_DONE`

`SHOOT_DONE` must only last one control cycle before returning to idle armed.

**Step 4: Run a compile check**

Run: Keil `F7`

Expected:
- build succeeds
- no duplicate ownership of `trigger_ecd_set`

**Step 5: Commit**

```bash
git add application/shoot.c
git commit -m "refactor: add trigger executor and compatibility mode mapping"
```

---

### Task 4: Implement HUST-Style Idle Hold and Single-Fire Completion

**Files:**
- Modify: `application/shoot.c`
- Modify: `application/shoot.h`
- Reference only: `HUST_Infantry_2023/F405_Gimbal/Task/ActionTask.c`

**Step 1: Rework idle hold so it reattaches the hold target**

Implement idle hold as:

```c
static void trigger_executor_idle_hold(void)
{
    shoot_control.trigger_ecd_set = shoot_control.trigger_ecd_fdb;
    shoot_control.speed_set = PID_enhanced_calc(&shoot_control.trigger_pos_pid,
                                                shoot_control.trigger_ecd_fdb,
                                                shoot_control.trigger_ecd_set);
}
```

This must replace any behavior that keeps holding an old one-grid target after a completed shot.

**Step 2: Rework single-fire completion**

In `TRIGGER_EXEC_STEP_ONE_GRID`:

- increment `trigger_ecd_set` by one grid once on entry
- drive toward target with the position loop
- when `fabs(trigger_ecd_set - trigger_ecd_fdb) < TRIGGER_POS_THRESHOLD`
  - increment `bullet_fired_count` exactly once
  - add `HEAT_PER_BULLET` exactly once
  - pulse `SHOOT_DONE`
  - clear one-shot latches
  - reattach `trigger_ecd_set` to current feedback before returning to idle hold

**Step 3: Keep READY hold authority bounded**

If `TRIGGER_POS_MAX_OUT_FIRE` remains in use, ensure it only applies during active shot execution, not during stale-target hold.

If needed, add a dedicated hold clamp:

```c
#define TRIGGER_POS_MAX_OUT_HOLD 8000.0f
```

and switch to it in idle hold.

**Step 4: Run a compile check**

Run: Keil `F7`

Expected:
- build succeeds
- no duplicate bullet counting

**Step 5: Commit**

```bash
git add application/shoot.c application/shoot.h
git commit -m "fix: reattach trigger hold target after single fire"
```

---

### Task 5: Align Continue-Fire and Reverse Recovery With HUST Semantics

**Files:**
- Modify: `application/shoot.c`
- Modify: `application/shoot.h`
- Reference only: `HUST_Infantry_2023/F405_Gimbal/Task/ActionTask.c`
- Reference only: `HUST_Infantry_2023/F405_Gimbal/Task/ShootTask.c`

**Step 1: Implement continuous fire as speed-mode execution**

Behavior:

- while `continue_req` is active, executor uses speed control
- do not keep stacking persistent position targets
- on release, clear continue state and reattach `trigger_ecd_set` to `trigger_ecd_fdb`

**Step 2: Keep manual reverse as an executor action**

Manual reverse should:

- enter `TRIGGER_EXEC_REVERSE_RECOVER`
- back off one grid once
- exit cleanly to idle hold

This should be closer to HUST's one-grid reverse behavior than the current mixed READY-state approach.

**Step 3: Preserve bounded automatic jam recovery**

Retain current timer-based jam logic, but keep it executor-local:

- `block_time`
- `reverse_time`
- `reverse_count`
- `MAX_REVERSE_COUNT`

If recovery fails:

- abandon current shot
- clear active execution latches
- return to idle hold with hold target reattached to current feedback

**Step 4: Run a compile check**

Run: Keil `F7`

Expected:
- build succeeds
- no unreachable reverse/continue paths

**Step 5: Commit**

```bash
git add application/shoot.c application/shoot.h
git commit -m "refactor: align trigger continue and reverse execution with hust"
```

---

### Task 6: Add Final Trigger Current Clamp and Telemetry Fields

**Files:**
- Modify: `application/shoot.c`
- Modify: `application/shoot.h`
- Modify: `application/usb_task.c`
- Reference only: `HUST_Infantry_2023/F405_Gimbal/Task/DataSendTask.c`

**Step 1: Add a final trigger current clamp in the shoot path**

Add a constant:

```c
#define TRIGGER_CAN_CURRENT_LIMIT 8000
```

Clamp the final trigger current before `shoot_control_loop()` returns:

```c
shoot_control.given_current = (int16_t)fp32_constrain((fp32)shoot_control.given_current,
                                                      -TRIGGER_CAN_CURRENT_LIMIT,
                                                      TRIGGER_CAN_CURRENT_LIMIT);
```

Do not rely on CAN send helpers to perform shoot-specific protection.

**Step 2: Export missing telemetry values**

Extend the shoot telemetry payload to include:

- microswitch state
- `reverse_flag`
- `referee_heat`

Append them near existing shoot debug fields in `application/usb_task.c`.

**Step 3: Update channel documentation comments if needed**

If `usb_task.c` or related docs list shoot channels, extend the comment block so FireWater mapping stays readable.

**Step 4: Run a compile check**

Run: Keil `F7`

Expected:
- build succeeds
- no USB debug frame length overrun

**Step 5: Commit**

```bash
git add application/shoot.c application/shoot.h application/usb_task.c
git commit -m "fix: clamp trigger output and export shoot protection telemetry"
```

---

### Task 7: Run Empty-Load Acceptance First

**Files:**
- No source edits
- Output evidence: `data/tune_2026-03-06_shoot_hust_acceptance_empty_*.csv`
- Reference only: `docs/ai_sessions/2026-03-06_shoot_state_machine_align_hust.md`

**Step 1: Build and flash the board**

Run:

- Keil `F7`
- Keil Download

Expected:
- flash succeeds
- board boots normally

**Step 2: Verify telemetry link on COM22**

Run:

```powershell
@'
import sys
sys.path.insert(0, r"D:\tools\stm32-telemetry-mcp")
from serial_manager import TelemetrySerialManager
mgr = TelemetrySerialManager()
print(mgr.list_serial_ports())
'@ | D:\python\python.exe -
```

Expected:
- `COM22` appears when the board is powered

**Step 3: Capture empty-load single-fire telemetry**

Run:

```powershell
@'
import sys
sys.path.insert(0, r"D:\tools\stm32-telemetry-mcp")
from serial_manager import TelemetrySerialManager
mgr = TelemetrySerialManager()
cols = [
    "shoot_mode",
    "trigger_spd",
    "trigger_spd_set",
    "trigger_ecd_fdb",
    "trigger_ecd_set",
    "bullet_cnt",
    "trigger_cur",
    "switch_trigger",
    "reverse_flag",
    "local_heat",
    "referee_heat",
]
print(mgr.capture("COM22", 20.0, cols))
'@ | D:\python\python.exe -
```

Expected:
- capture succeeds
- CSV is written under `D:\tools\stm32-telemetry-mcp\captures\`

**Step 4: Evaluate acceptance**

Required pass conditions:

- `bullet_cnt` increases by exactly 1 per single fire
- `SHOOT_DONE` is transient
- executor returns to `SHOOT_READY_BULLET`
- no sustained current saturation in ready hold
- no empty-load oscillation worse than `data/tune_2026-03-06_003.csv`

**Step 5: Commit docs-only evidence update**

```bash
git add data/tune_2026-03-06_shoot_hust_acceptance_empty_*.csv docs/ai_sessions/2026-03-06_shoot_state_machine_align_hust.md docs/risk_log.md docs/ai_sessions/CURRENT_STATE.md
git commit -m "test: record empty-load shoot hust refactor acceptance"
```

---

### Task 8: Only If Empty-Load Passes, Run Live-Fire Acceptance

**Files:**
- No source edits
- Output evidence: `data/tune_2026-03-06_shoot_hust_acceptance_live_*.csv`
- Modify: `docs/ai_sessions/2026-03-06_shoot_state_machine_align_hust.md`
- Modify: `docs/risk_log.md`
- Modify: `docs/ai_sessions/CURRENT_STATE.md`
- Modify: `docs/INDEX.md` if a new session or plan entry is added

**Step 1: Stop if empty-load failed**

Do not proceed to live-fire unless Task 7 passed.

Expected:
- if empty-load failed, mark live-fire blocked and end execution

**Step 2: Capture live-fire single-shot telemetry**

Reuse the same telemetry command as Task 7, but store to a live-fire evidence file and run only a small gated batch first.

Expected:
- single-fire only
- no repeated count
- no accidental continue fire

**Step 3: Update handoff docs**

Backfill:

- `docs/ai_sessions/2026-03-06_shoot_state_machine_align_hust.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md` if needed

Every session record must include:

```markdown
## ⚠ 未验证假设
```

**Step 4: Commit final acceptance docs**

```bash
git add data/tune_2026-03-06_shoot_hust_acceptance_live_*.csv docs/ai_sessions/2026-03-06_shoot_state_machine_align_hust.md docs/risk_log.md docs/ai_sessions/CURRENT_STATE.md docs/INDEX.md
git commit -m "docs: update shoot hust refactor acceptance results"
```
