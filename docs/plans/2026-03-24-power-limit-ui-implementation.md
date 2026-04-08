# Power Limit UI Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port the chassis power prediction limit algorithm from `main` without any supercap code, and add a referee client UI that shows chassis mode, fire-speed gear, reverse-success feedback, and a reticle.

**Architecture:** Keep the existing chassis and shoot control loops intact and replace only the limiter logic in `application/chassis_power_control.c`. Add a separate `application/rm_ui.c/.h` module for referee client drawing and refresh, and feed it with readonly state from chassis and shoot instead of mixing UI protocol code into `referee.c`.

**Tech Stack:** STM32F4 bare-metal/CMSIS-RTOS v1, existing referee protocol stack, CAN motor control, Keil MDK project, no unit-test framework.

---

### Task 1: Port the non-supercap power model limiter

**Files:**
- Modify: `application/chassis_power_control.c`
- Modify: `application/chassis_power_control.h`
- Modify: `application/chassis_task.h`
- Modify: `application/chassis_task.c`
- Reference: `docs/plans/2026-03-24-power-limit-ui-design.md`
- Reference: `main:application/chassis_power_control.c`

**Step 1: Record the current limiter behavior boundary**

Run: `git diff -- application/chassis_power_control.c application/chassis_power_control.h application/chassis_task.c application/chassis_task.h`

Expected: no local edits in those files before implementation begins.

**Step 2: Copy the power model math but remove all supercap branches**

Implement in `application/chassis_power_control.c`:
- per-motor power estimate helper
- quadratic current back-solve helper
- total positive-power accumulation
- over-limit redistribution/back-solve path

Do not include:
- `#include "supercap.h"`
- `supercap_is_available()`
- `get_supercap_measure_point()`
- `supercap_send_cmd(...)`

**Step 3: Keep the current project's limiter inputs unchanged**

In `application/chassis_power_control.c`, continue using the current branch's referee power/buffer accessors and project constants as the limiter input boundary. Do not port `main`'s broader power-limit sourcing changes.

**Step 4: Add minimal runtime observability fields to the chassis control struct**

In `application/chassis_task.h`, add only the fields needed by the new limiter:
- `power_est`
- `effective_limit`

Do not add buffer PID or any supercap-related fields.

**Step 5: Remove any accidental supercap integration from the chassis task**

In `application/chassis_task.c`, verify there is no new transmit path or include for supercap. The task must still end with the existing `CAN_cmd_chassis(...)` flow only.

**Step 6: Build-check the power files for obvious integration mistakes**

Run: `rg -n "supercap|super cap|get_supercap|supercap_send" application/chassis_power_control.c application/chassis_power_control.h application/chassis_task.c application/chassis_task.h`

Expected: no matches.

**Step 7: Commit**

```bash
git add application/chassis_power_control.c application/chassis_power_control.h application/chassis_task.c application/chassis_task.h
git commit -m "feat(power): port non-supercap power prediction limiter"
```

### Task 2: Add shoot-side readonly UI state and reverse-success event

**Files:**
- Modify: `application/shoot.h`
- Modify: `application/shoot.c`
- Reference: `docs/plans/2026-03-24-power-limit-ui-design.md`

**Step 1: Define the readonly APIs the UI module will consume**

Add declarations in `application/shoot.h` for:
- current fire-speed gear getter
- reverse-success pulse getter or consume-once API

Keep the API readonly from the UI module's point of view.

**Step 2: Normalize the gear into `LOW/HIGH`**

In `application/shoot.c`, derive a stable two-state gear API from the current project's two gear settings. Do not expose raw internal static variables directly across modules.

**Step 3: Emit a reverse-success pulse only when reverse reaches the loaded-one-round position**

Use the existing reverse path completion point:
- reverse path is active
- one reverse `TRIGGER_ONEGRID` step has been commanded
- `abs(trigger_ecd_set - trigger_ecd_fdb) < TRIGGER_POS_THRESHOLD`

Trigger the UI pulse here, not on reverse request edge.

**Step 4: Make the pulse consume-once**

Ensure the UI can read the success event once and then clear it, so a stale success does not keep retriggering the blink.

**Step 5: Build-check the shoot API surface**

Run: `rg -n "get_.*gear|reverse_success|ui" application/shoot.c application/shoot.h`

Expected: the new API names exist only in the intended declarations and implementations.

**Step 6: Commit**

```bash
git add application/shoot.c application/shoot.h
git commit -m "feat(shoot): expose UI gear state and reverse success pulse"
```

### Task 3: Add the referee client UI module

**Files:**
- Create: `application/rm_ui.h`
- Create: `application/rm_ui.c`
- Modify: `application/referee.h`
- Modify: `application/referee.c`
- Reference: `docs/plans/2026-03-24-power-limit-ui-design.md`
- Reference: `docs/05_BOARD_PHYSICAL_MAP.md`

**Step 1: Define the minimal referee client UI protocol types**

In `application/rm_ui.h`, add only the protocol structs/constants needed for:
- line graphics
- text graphics if used
- delete/refresh commands if needed
- sender/receiver IDs

Do not broaden this into a generic full-protocol framework yet.

**Step 2: Implement a packet builder in `application/rm_ui.c`**

Implement helpers to:
- build frame header
- append cmd id and interactive payload
- compute CRC8/CRC16 using existing support code
- send through the existing referee transmit path

**Step 3: Draw the static reticle**

Implement one-time draw helpers for the reticle:
- one vertical main line
- several horizontal long division lines
- Y position near screen height `44%`

Do not center it geometrically at `50%`.

**Step 4: Add dynamic UI items**

Implement refresh helpers for:
- chassis mode text: `FOLLOW` / `SPIN`
- fire-speed gear text: `LOW` / `HIGH`
- reverse-success green indicator

**Step 5: Add blink timing inside the UI module**

In `application/rm_ui.c`, maintain:
- blink active flag
- blink start time
- current blink phase

Behavior:
- start on reverse-success pulse
- blink green for `500~1000ms`
- auto-return to default state after timeout

**Step 6: Keep refresh bandwidth low**

Implement separate paths for:
- static draw once
- dynamic refresh on change
- faster refresh only during the blink window

Do not refresh every 1ms or 2ms.

**Step 7: Commit**

```bash
git add application/rm_ui.c application/rm_ui.h application/referee.c application/referee.h
git commit -m "feat(ui): add referee client reticle and state overlay"
```

### Task 4: Integrate the UI module into the existing runtime

**Files:**
- Modify: `application/referee_usart_task.c`
- Modify: `application/gimbal_task.c`
- Modify: `application/chassis_task.c`
- Modify: `MDK-ARM/standard_robot.uvprojx`
- Reference: `application/referee_usart_task.c`
- Reference: `application/gimbal_task.c`

**Step 1: Choose one runtime update point and keep it single-owned**

Pick one existing low-frequency-safe path for `rm_ui_update()`:
- prefer the referee task or another non-critical periodic path
- avoid doing full packet assembly inside the tightest control loop if not necessary

Document the chosen hook with a brief code comment.

**Step 2: Initialize the UI module once**

Call `rm_ui_init()` from an existing initialization path after the referee stack is usable.

**Step 3: Feed the UI with readonly state**

Pass into the module only:
- folded chassis mode (`FOLLOW` or `SPIN`)
- current `LOW/HIGH` gear state
- reverse-success consume-once event

Do not let the UI module inspect internal shoot static variables directly.

**Step 4: Add new files to Keil**

Update `MDK-ARM/standard_robot.uvprojx` so the new `rm_ui.c` compiles in Keil.

**Step 5: Sanity-check project references**

Run: `rg -n "rm_ui" application MDK-ARM/standard_robot.uvprojx`

Expected: references appear only in the new module, intended call sites, and the Keil project file.

**Step 6: Commit**

```bash
git add application/referee_usart_task.c application/gimbal_task.c application/chassis_task.c MDK-ARM/standard_robot.uvprojx
git add application/rm_ui.c application/rm_ui.h
git commit -m "feat(ui): integrate referee client UI into runtime"
```

### Task 5: Update required documentation and handoff artifacts

**Files:**
- Modify: `docs/02_ARCHITECTURE.md`
- Modify: `docs/04_MODULE_MAP.md`
- Modify: `docs/risk_log.md`
- Modify: `docs/ai_sessions/CURRENT_STATE.md`
- Modify: `docs/INDEX.md`
- Create: `docs/ai_sessions/2026-03-24_power_limit_ui.md`
- Reference: `docs/00_AI_HANDOFF_RULES.md`
- Reference: `docs/ai_sessions/TEMPLATE.md`

**Step 1: Add the AI session record**

Create `docs/ai_sessions/2026-03-24_power_limit_ui.md` and include all required sections from the repo rules:
- 目标
- 改动文件列表
- 关键改动摘要
- 关键决策与理由
- 实时性影响
- 风险点
- `## ⚠ 未验证假设`
- 验证方式
- 回滚方式

**Step 2: Update architecture/module docs**

In `docs/02_ARCHITECTURE.md` and `docs/04_MODULE_MAP.md`, document:
- non-supercap chassis power model limiter
- new `rm_ui` module responsibility
- chosen runtime hook for UI refresh

**Step 3: Update status aggregation**

Update:
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`

Keep one current-state entry for this feature.

**Step 4: Commit**

```bash
git add docs/02_ARCHITECTURE.md docs/04_MODULE_MAP.md docs/risk_log.md docs/ai_sessions/CURRENT_STATE.md docs/INDEX.md docs/ai_sessions/2026-03-24_power_limit_ui.md
git commit -m "docs: record power limiter and referee ui migration"
```

### Task 6: Verify build and hardware-facing behavior

**Files:**
- Verify: `MDK-ARM/standard_robot.uvprojx`
- Observe: chassis power limit behavior on board
- Observe: referee client UI on board

**Step 1: Run project-level build verification**

Use Keil MDK to rebuild `MDK-ARM/standard_robot.uvprojx`.

Expected: `0 Error`.

If Keil is unavailable in the current environment, record that explicitly in the session log instead of claiming build success.

**Step 2: Verify the power limiter behavior on board**

Check:
- normal chassis motion is not obviously over-limited
- high-load chassis motion is visibly constrained
- no abrupt current jump caused by bad quadratic solve behavior

Recommended observed values:
- motor currents
- predicted power if temporarily instrumented
- chassis response under aggressive stick input

**Step 3: Verify UI geometry and semantics on referee client**

Check:
- reticle is at about screen height `44%`
- reticle is one vertical line plus several horizontal long lines
- chassis mode toggles only between `FOLLOW` and `SPIN`
- gear toggles only between `LOW` and `HIGH`
- reverse-success indicator blinks green for about `0.5~1s` only after the reverse step finishes at the loaded-one-round position

**Step 4: Request code review before closure**

Use `@superpowers:requesting-code-review` after local verification and before claiming completion.

**Step 5: Commit any verification-only follow-up fixes**

```bash
git add -A
git commit -m "fix: resolve verification findings for power limiter and referee ui"
```

### Task 7: Final completion pass

**Files:**
- Review: modified implementation files
- Review: updated docs

**Step 1: Run `@superpowers:verification-before-completion`**

Confirm the final response is backed by actual verification evidence, not assumption.

**Step 2: Summarize any remaining unverified assumptions**

Carry unresolved board-side assumptions into:
- `docs/ai_sessions/2026-03-24_power_limit_ui.md`
- `docs/risk_log.md`

**Step 3: Prepare branch for handoff**

Leave the branch with:
- clean working tree
- documented risks
- updated current state

**Step 4: Final commit**

```bash
git add -A
git commit -m "feat: finish power limiter port and referee ui overlay"
```
