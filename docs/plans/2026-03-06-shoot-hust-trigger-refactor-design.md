# Shoot HUST Trigger Refactor Design

**Date:** 2026-03-06

**Status:** Approved for planning

## Background

Current `application/shoot.c` is only partially aligned with `HUST_Infantry_2023`. Parameters such as one-grid size and position-loop gains are similar, but the control structure is different.

Board telemetry on 2026-03-06 showed that after one empty single-fire, the trigger stayed in `SHOOT_READY_BULLET`, `trigger_ecd_set` remained fixed, and `trigger_ecd_fdb` oscillated around that fixed target with sustained saturated current. This means the main issue is not periodic target rewrite in READY, but persistent full-authority hold around an old target.

Reference behavior in `HUST_Infantry_2023` differs in two important ways:

- The upper layer generates fire/continue/reverse commands.
- The lower layer executes those commands and, after finishing, re-attaches the hold target to the current trigger position instead of holding an old firing target indefinitely.

## Goals

- Refactor shoot control to be structurally closer to `HUST_Infantry_2023`.
- Remove microswitch-driven main state transitions.
- Keep the current project's public shoot interface and telemetry compatibility.
- Fix the empty-load self-oscillation seen after a single fire.
- Preserve current heat-limit, fric-ready, and CAN integration behavior where possible.

## Non-Goals

- Do not fully port `HUST_Infantry_2023` task topology into this project.
- Do not redesign unrelated gimbal, VT03, referee, or CAN modules.
- Do not change the user-facing fire controls beyond what is necessary for HUST-aligned behavior.

## Options Considered

### Option 1: Keep current API, refactor internals into HUST-like command layer + executor layer

Recommended.

- Pros:
  - Closest to HUST behavior without rewriting the whole project architecture.
  - Keeps `shoot_init()`, `shoot_control_loop()`, and telemetry fields stable.
  - Separates input interpretation from trigger execution, which matches the observed failure mode.
- Cons:
  - Requires careful mapping between current state names and HUST behavior.

### Option 2: Full HUST-style port of shoot task structure

- Pros:
  - Highest semantic similarity with the reference project.
- Cons:
  - Too invasive for the current codebase.
  - Would disrupt current task integration, telemetry, and call sites.

### Option 3: Patch current 7-state machine in place

- Pros:
  - Smallest edit volume.
- Cons:
  - Leaves structural coupling in place.
  - Higher chance of future regressions and harder reasoning during board acceptance.

## Chosen Design

Use Option 1.

The current public module shape remains intact, but internal control is split into:

- `shoot_command_layer`
- `trigger_executor_layer`

The upper layer only decides intent. The lower layer is the only code allowed to modify trigger execution targets and execution sub-states.

## Architecture

### Public Interface

Keep the existing public functions:

- `shoot_init()`
- `shoot_control_loop()`
- `shoot_get_fric_current()`

This avoids changes to existing task scheduling, CAN call sites, and telemetry consumers.

### Internal Layer 1: Command Layer

The command layer converts current inputs and readiness conditions into four executor commands:

- `arm_enable`
- `single_fire_req`
- `continue_req`
- `reverse_req`

Inputs considered by this layer:

- RC switch and mouse/keyboard fire actions
- VT03 or keyboard-trigger fire commands if already supported
- fric ready state
- local/referee heat limit
- manual reverse command
- shoot stop request from higher-level gimbal behavior

The command layer does not directly write `trigger_ecd_set`.

### Internal Layer 2: Trigger Executor Layer

The executor layer owns trigger execution. It contains four execution states:

- `idle_hold`
- `step_one_grid`
- `continuous_run`
- `reverse_recover`

Only this layer may:

- advance `trigger_ecd_set`
- back off one grid during reverse recovery
- decide when a shot is complete
- re-attach hold target to current feedback

## State Model

### External Compatibility States

For telemetry and compatibility, keep exposing current `shoot_mode` values:

- `SHOOT_STOP`
- `SHOOT_READY_FRIC`
- `SHOOT_READY_BULLET`
- `SHOOT_BULLET`
- `SHOOT_CONTINUE_BULLET`
- `SHOOT_DONE`

`SHOOT_READY` will no longer be used as a microswitch-driven steady state in the main flow.

### Internal Transition Rules

- If shoot is disabled, external mode is `SHOOT_STOP`, executor is `idle_hold`, and the hold target is reattached to current position.
- If shoot is enabled but fric is not ready, external mode is `SHOOT_READY_FRIC`, executor remains `idle_hold`, and the hold target is reattached to current position.
- If shoot is armed and no fire command is active, external mode is `SHOOT_READY_BULLET`, executor remains `idle_hold`.
- On `single_fire_req`, executor enters `step_one_grid` once and increments the target by one grid once.
- On long press, executor enters `continuous_run` and uses speed control instead of repeated position-target stepping.
- On manual or automatic reverse request, executor enters `reverse_recover`.
- When a shot finishes, external mode briefly enters `SHOOT_DONE`, then returns to `SHOOT_READY_BULLET` next cycle.
- When executor exits `step_one_grid` or `continuous_run`, it re-attaches `trigger_ecd_set` to `trigger_ecd_fdb`.

## Microswitch Handling

The microswitch will no longer drive `READY_BULLET <-> READY` main state transitions.

It will be retained only for:

- telemetry/observation
- optional protection logic
- future homing or mechanical diagnosis if needed

This is intentionally closer to HUST behavior, where the main firing path is driven by command generation and trigger execution, not by a microswitch-driven public state split.

## Detailed Execution Behavior

### Idle Hold

Idle hold must behave like HUST's post-release behavior:

- do not keep holding an old shot target
- align `trigger_ecd_set` to the current `trigger_ecd_fdb`
- then apply hold control around that current position

This directly addresses the observed empty-load oscillation after a completed shot.

### Single Fire

- Enter `step_one_grid` on a fire edge.
- Increase `trigger_ecd_set` by exactly one grid once.
- Use the existing position loop to drive toward the target.
- When within `TRIGGER_POS_THRESHOLD`, count the shot once, update heat once, enter `SHOOT_DONE`, and then re-attach hold target to current position.

### Continue Fire

Continue fire should align with HUST semantics:

- use speed-command execution while long press is active
- do not keep stacking old position-hold targets in the background
- when continuous fire ends, re-attach hold target to current position before returning to idle hold

### Reverse Recovery

Reverse recovery remains available, but becomes executor-internal behavior.

- Manual reverse should emulate HUST's one-grid reverse action.
- Automatic jam recovery can continue using the current block timer / reverse timer / reverse count logic.
- Reverse recovery only affects the current shot attempt and must not leave the idle hold target stuck on an obsolete target afterward.

## Protection and Current Limiting

The refactor includes two protection requirements:

- Add final trigger current limiting before CAN transmit, aligned with HUST's last-stage clamp behavior.
- Keep automatic jam recovery bounded by `MAX_REVERSE_COUNT` and return to idle hold if recovery fails.

This is required because current telemetry showed prolonged saturation at the trigger output during the failure case.

## Telemetry Compatibility

Keep existing acceptance channels available:

- `shoot_mode`
- `trigger_spd`
- `trigger_spd_set`
- `trigger_ecd_fdb`
- `trigger_ecd_set`
- `bullet_cnt`

Additional channels to retain or add where available:

- `trigger_cur`
- microswitch state
- `reverse_flag`
- `local_heat`
- `referee_heat`

Telemetry compatibility is part of the design because board acceptance is the only effective validation path for this module.

## Acceptance Criteria

### Empty-Load Gate

- Perform 20 empty single shots first.
- `bullet_cnt` must increase by exactly 1 each time.
- `SHOOT_DONE` must appear only briefly and return to `SHOOT_READY_BULLET`.
- `trigger_ecd_set` must not be periodically rewritten in READY/READY_BULLET.
- Empty-load overshoot must not be worse than `data/tune_2026-03-06_003.csv`.
- No sustained high current or hard pushing when the microswitch is ON.

### Live-Fire Gate

- Only enter live-fire if empty-load passes.
- Validate single-shot count correctness, no repeated count, no accidental continuous fire, and no worsening overshoot.

## Risks

- The current project still uses DJI-style public state exposure, so internal/external state mapping must stay coherent.
- Existing telemetry and downstream tools may assume `SHOOT_READY` appears; those assumptions must be checked before removing its steady-state role.
- The sign conventions for trigger position, speed, and reverse direction must be revalidated on hardware after refactor.

## Rollout Strategy

1. Refactor internal structure without changing telemetry field names.
2. Keep current PID constants initially, except for required output-clamp alignment.
3. Re-run empty-load acceptance first.
4. Only if empty-load passes, continue to live-fire verification.

## ⚠ Unverified Assumptions

- The main empty-load oscillation is caused by stale-target persistent hold plus insufficient final current clamping, rather than a hardware polarity/configuration error.
- Removing microswitch-driven READY transitions will not break any higher-level caller assumptions outside current telemetry usage.
- HUST's effective stability does not depend on another hidden mechanical or wiring difference not yet mirrored in this project.
