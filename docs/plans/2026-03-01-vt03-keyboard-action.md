# VT03 图传链路键位配置 Implementation Plan

> **For Claude/Codex:** 本文档由 Claude 生成，供 Codex 逐 Task 执行。每个 Task 都包含精确的文件路径、行号、old/new 代码块。

**Goal:** 通过 VT03/VT13 图传链路实现完整的键鼠控制，新建集中式 keyboard_action 模块管理所有键位映射，同时保留原 DBUS 遥控器链路不受影响。

**Architecture:** 新建 `keyboard_action.c/h` 作为统一键位管理层，负责边沿检测和命令输出。各 task（chassis/gimbal/shoot）读取 keyboard_cmd 标志位执行动作。VT03 mode_sw 映射到 DBUS 兼容值。DBUS 原有逻辑完全保留。

**Tech Stack:** STM32F407 + FreeRTOS, Keil MDK (armcc), C99

**参考实现:** `HUST_Infantry_2023/F405_Gimbal/Task/ActionTask.c`

---

## 重要背景信息（Codex 必读）

### 代码结构
- `application/gimbal_task.c`: 云台主循环（1ms），也调用 `shoot_control_loop()`
- `application/gimbal_behaviour.c`: 云台行为模式选择与控制
- `application/chassis_task.c`: 底盘主循环（2ms）
- `application/chassis_behaviour.c`: 底盘行为模式选择与控制
- `application/shoot.c`: 发射控制（在 gimbal_task 中被调用，1ms 频率）
- `application/vt03_link.c`: VT03 帧解析，写入 `rc_ctrl` 和 `vt03_ext`
- `application/remote_control.c`: DBUS/SBUS 解析，写入 `rc_ctrl`

### RC 开关通道分配
- `s[0]` (GIMBAL_MODE_CHANNEL=0, CHASSIS_MODE_CHANNEL=0): 云台和底盘模式
- `s[1]` (SHOOT_RC_MODE_CHANNEL=1): **仅 DBUS 有效**，VT03 下永远 = RC_SW_DOWN

### 关键宏
- `RC_SW_UP=1, RC_SW_DOWN=2, RC_SW_MID=3`
- `GIMBAL_YAW_CONTINUOUS_TURN=1` (当前值): 180° 掉头代码已被条件编译排除
- `VT03_ENABLE`: 由 `uart_mode.h` 根据 `CURRENT_UART_MODE` 定义
- `VT03_TOE`: 在 `detect_task.h` 中注册的 VT03 在线检测

### 编译工具链
- Keil MDK (armcc), **不支持 C99 变量声明在代码中间**，所有变量必须在块开头声明
- 不能用 `<string.h>` 的 memset，用手动清零或 `= {0}`

---

## 键位映射表（最终确认版）

### 键盘键位（16-bit，VT13 键盘与 DBUS 键盘 bit 映射完全一致）
| bit | 按键 | 功能 | 类型 |
|-----|------|------|------|
| 0 | W | 前进 | Hold |
| 1 | S | 后退 | Hold |
| 2 | A | 左移 | Hold |
| 3 | D | 右移 | Hold |
| 4 | SHIFT | 超级电容（预留接口） | Hold |
| 5 | CTRL | 底盘模式切换（跟随 ↔ 小陀螺） | Toggle |
| 6 | Q | 发射系统开关 | Toggle |
| 7 | E | 预留（自瞄） | — |
| 8 | R | 连发模式 | Toggle |
| 9 | F | 摩擦轮 -100（SHIFT+F = +100） | Rising |
| 10 | G | 堵转反转 | Rising |
| 11 | Z | 无力模式（= DOWN → ZERO_FORCE） | Toggle |
| 12 | X | 预留 | — |
| 13 | C | 高频射击 | Toggle |
| 14 | V | 预留 | — |
| 15 | B | UI 刷新 | Rising |

### 鼠标键位
| 输入 | 功能 | 类型 |
|------|------|------|
| 鼠标移动 X/Y | 云台 yaw/pitch 瞄准 | 已有 |
| 鼠标左键 | 短按单发 / 长按连发 | Press |
| 鼠标右键 | 预留（自瞄） | — |
| 鼠标中键 | 预留 | — |
| 滚轮 Z | 预留 | — |

### VT13 遥控器物理按键（不接键盘时的独立操控）
| 输入 | 操作 | 功能 | 等同键盘 |
|------|------|------|---------|
| mode_sw | C(0)/N(1)/S(2) | 无力/跟随/小陀螺 | — |
| fn_1 短按 | <300ms 松开 | 发射系统开关 | Q |
| fn_1 长按 | ≥300ms | 高频模式 toggle | C |
| fn_2 短按 | <300ms 松开 | 连发模式 toggle | R |
| fn_2 长按 | ≥300ms | 堵转反转 | G |
| pause 短按 | <300ms 松开 | 超级电容 | SHIFT |
| pause 长按 | ≥300ms | 预留 | — |
| trigger | 按下 | 射击（等同鼠标左键） | 鼠标左键 |
| 拨轮 dial | 旋转 | 摩擦轮调速 | F/SHIFT+F |

> **长按判定:** 按下持续 ≥300ms 判定为长按，松开时不触发短按。短按 = 按下后 <300ms 内松开。

---

## Task 1: 创建 keyboard_action.h

**Files:**
- Create: `application/keyboard_action.h`

**完整文件内容:**

```c
#ifndef KEYBOARD_ACTION_H
#define KEYBOARD_ACTION_H

#include "struct_typedef.h"
#include "remote_control.h"
#include "uart_mode.h"

/* ---- 长按检测阈值 (ms，keyboard_action_update 在 gimbal_task 1ms 调用) ---- */
#define BTN_LONG_PRESS_MS  300u

/* ---- VT13 按键长按状态 ---- */
typedef struct
{
    uint16_t hold_cnt;    /* 按住计时 (ms) */
    uint8_t  long_fired;  /* 1 = 本次按住已触发长按，松开前不再触发短按 */
} btn_lp_t;

/* ---- 边沿检测状态 ---- */
typedef struct
{
    uint16_t rising;    /* 本周期刚按下的键 bitmask (0->1) */
    uint16_t falling;   /* 本周期刚松开的键 bitmask (1->0) */
    uint16_t held;      /* 当前按住的键 (= rc_ctrl.key.v) */
    uint16_t prev;      /* 上周期键值（内部用） */

    /* VT03 扩展键当前值与上周期值 */
    uint8_t fn1_cur;
    uint8_t fn2_cur;
    uint8_t trigger_cur;
    uint8_t pause_cur;
    uint8_t fn1_prev;
    uint8_t fn2_prev;
    uint8_t trigger_prev;
    uint8_t pause_prev;

    /* VT13 按键长按状态 */
    btn_lp_t fn1_lp;
    btn_lp_t fn2_lp;
    btn_lp_t pause_lp;
} key_state_t;

/* ---- 命令输出（每周期更新，1=本周期触发） ---- */
typedef struct
{
    /* 底盘 */
    uint8_t chassis_mode_toggle;  /* CTRL rising: 切换底盘模式 */
    uint8_t cap_enable;           /* SHIFT held 或 pause 短按 toggle: 超级电容 */

    /* 发射 */
    uint8_t shoot_toggle;         /* Q rising 或 fn_1 短按: 发射系统开关 */
    uint8_t high_freq_toggle;     /* C rising 或 fn_1 长按: 高频模式切换 */
    uint8_t fric_speed_adj;       /* 0=无, 1=SHIFT+F(+100), 2=F alone(-100), 3=拨轮+ 4=拨轮- */
    uint8_t reverse_trigger;      /* G rising 或 fn_2 长按: 反转拨弹 */
    uint8_t burst_toggle;         /* R rising 或 fn_2 短按: 连发模式 */
    uint8_t ui_refresh;           /* B rising: UI 刷新 */

    /* VT03 特有 */
    uint8_t vt03_trigger;         /* VT03 trigger rising: 等同鼠标左键 */

    /* 安全 */
    uint8_t zero_force_toggle;    /* Z rising: 无力模式 toggle */
} keyboard_cmd_t;

/* ---- 底盘键盘模式（CTRL toggle 状态） ---- */
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
```

**Commit:**
```bash
git add application/keyboard_action.h
git commit -m "feat: add keyboard_action.h header for centralized key management"
```

---

## Task 2: 实现 keyboard_action.c

**Files:**
- Create: `application/keyboard_action.c`

**注意:**
- armcc 不支持 `<string.h>` 的 memset，用手动清零或 `= {0}`
- 所有变量必须在块开头声明
- VT13 按键长按检测：按住 ≥300ms 触发长按，松开时 <300ms 触发短按

**完整文件内容:**

```c
#include "keyboard_action.h"
#include "detect_task.h"

#if VT03_ENABLE
#include "vt03_link.h"
#endif

static key_state_t    key_state = {0};
static keyboard_cmd_t kb_cmd    = {0};
static kb_chassis_mode_e kb_chassis_mode = KB_CHASSIS_FOLLOW;
static uint8_t kb_zero_force = 0;   /* Z 键 toggle 状态 */
static uint8_t kb_cap_on     = 0;   /* pause 短按 toggle 状态 */

void keyboard_action_init(void)
{
    uint8_t *p;
    uint16_t i;

    p = (uint8_t *)&key_state;
    for (i = 0; i < sizeof(key_state); i++) p[i] = 0;

    p = (uint8_t *)&kb_cmd;
    for (i = 0; i < sizeof(kb_cmd); i++) p[i] = 0;

    kb_chassis_mode = KB_CHASSIS_FOLLOW;
    kb_zero_force = 0;
    kb_cap_on = 0;
}

void keyboard_action_update(void)
{
    const RC_ctrl_t *rc;
    uint16_t cur;

    rc  = get_remote_control_point();
    cur = rc->key.v;

    /* ---- 16 键统一边沿检测 ---- */
    key_state.rising  = cur & ~key_state.prev;   /* 0->1 */
    key_state.falling = ~cur & key_state.prev;    /* 1->0 */
    key_state.held    = cur;
    key_state.prev    = cur;

    /* ---- VT03 扩展键采样 ---- */
#if VT03_ENABLE
    if (!toe_is_error(VT03_TOE))
    {
        key_state.fn1_cur     = vt03_ext.fn_1;
        key_state.fn2_cur     = vt03_ext.fn_2;
        key_state.trigger_cur = vt03_ext.trigger;
        key_state.pause_cur   = vt03_ext.pause;
    }
    else
#endif
    {
        key_state.fn1_cur     = 0;
        key_state.fn2_cur     = 0;
        key_state.trigger_cur = 0;
        key_state.pause_cur   = 0;
    }

    /* ---- 清零命令（每周期重新生成） ---- */
    {
        uint8_t *p;
        uint16_t i;
        p = (uint8_t *)&kb_cmd;
        for (i = 0; i < sizeof(kb_cmd); i++) p[i] = 0;
    }

    /* ======== 键盘键位 ======== */

    /* CTRL -> 底盘模式循环 */
    if (key_state.rising & KEY_PRESSED_OFFSET_CTRL)
    {
        kb_cmd.chassis_mode_toggle = 1;
        if (kb_chassis_mode == KB_CHASSIS_FOLLOW)
            kb_chassis_mode = KB_CHASSIS_SPIN;
        else
            kb_chassis_mode = KB_CHASSIS_FOLLOW;
    }

    /* SHIFT -> 超级电容 (hold) */
    /* pause 短按 toggle 也会置 kb_cap_on，见下方 VT13 段 */
    kb_cmd.cap_enable = ((cur & KEY_PRESSED_OFFSET_SHIFT) || kb_cap_on) ? 1u : 0u;

    /* Q -> 发射系统开关 */
    if (key_state.rising & KEY_PRESSED_OFFSET_Q)
        kb_cmd.shoot_toggle = 1;

    /* C -> 高频 toggle */
    if (key_state.rising & KEY_PRESSED_OFFSET_C)
        kb_cmd.high_freq_toggle = 1;

    /* R -> 连发 toggle */
    if (key_state.rising & KEY_PRESSED_OFFSET_R)
        kb_cmd.burst_toggle = 1;

    /* G -> 反转拨弹 */
    if (key_state.rising & KEY_PRESSED_OFFSET_G)
        kb_cmd.reverse_trigger = 1;

    /* F -> 摩擦轮调速 */
    if (key_state.rising & KEY_PRESSED_OFFSET_F)
    {
        if (cur & KEY_PRESSED_OFFSET_SHIFT)
            kb_cmd.fric_speed_adj = 1;   /* SHIFT+F = +100 */
        else
            kb_cmd.fric_speed_adj = 2;   /* F alone = -100 */
    }

    /* Z -> 无力模式 toggle */
    if (key_state.rising & KEY_PRESSED_OFFSET_Z)
    {
        kb_cmd.zero_force_toggle = 1;
        kb_zero_force = !kb_zero_force;
    }

    /* B -> UI 刷新 */
    if (key_state.rising & KEY_PRESSED_OFFSET_B)
        kb_cmd.ui_refresh = 1;

    /* ======== VT03 trigger ======== */
    if (key_state.trigger_cur && !key_state.trigger_prev)
        kb_cmd.vt03_trigger = 1;
    key_state.trigger_prev = key_state.trigger_cur;

    /* ======== VT13 按键长按检测 (fn_1 / fn_2 / pause) ======== */

    /* --- fn_1: 短按=发射开关(Q), 长按=高频(C) --- */
    if (key_state.fn1_cur)
    {
        key_state.fn1_lp.hold_cnt++;
        if (key_state.fn1_lp.hold_cnt >= BTN_LONG_PRESS_MS && !key_state.fn1_lp.long_fired)
        {
            key_state.fn1_lp.long_fired = 1;
            kb_cmd.high_freq_toggle = 1;
        }
    }
    else
    {
        if (key_state.fn1_prev && !key_state.fn1_lp.long_fired)
        {
            kb_cmd.shoot_toggle = 1;  /* 短按 */
        }
        key_state.fn1_lp.hold_cnt = 0;
        key_state.fn1_lp.long_fired = 0;
    }
    key_state.fn1_prev = key_state.fn1_cur;

    /* --- fn_2: 短按=连发(R), 长按=堵转反转(G) --- */
    if (key_state.fn2_cur)
    {
        key_state.fn2_lp.hold_cnt++;
        if (key_state.fn2_lp.hold_cnt >= BTN_LONG_PRESS_MS && !key_state.fn2_lp.long_fired)
        {
            key_state.fn2_lp.long_fired = 1;
            kb_cmd.reverse_trigger = 1;
        }
    }
    else
    {
        if (key_state.fn2_prev && !key_state.fn2_lp.long_fired)
        {
            kb_cmd.burst_toggle = 1;  /* 短按 */
        }
        key_state.fn2_lp.hold_cnt = 0;
        key_state.fn2_lp.long_fired = 0;
    }
    key_state.fn2_prev = key_state.fn2_cur;

    /* --- pause: 短按=超电toggle, 长按=预留 --- */
    if (key_state.pause_cur)
    {
        key_state.pause_lp.hold_cnt++;
        if (key_state.pause_lp.hold_cnt >= BTN_LONG_PRESS_MS && !key_state.pause_lp.long_fired)
        {
            key_state.pause_lp.long_fired = 1;
            /* 长按预留，暂不执行动作 */
        }
    }
    else
    {
        if (key_state.pause_prev && !key_state.pause_lp.long_fired)
        {
            kb_cap_on = !kb_cap_on;   /* 短按 toggle 超电 */
        }
        key_state.pause_lp.hold_cnt = 0;
        key_state.pause_lp.long_fired = 0;
    }
    key_state.pause_prev = key_state.pause_cur;

    /* ======== 拨轮 → 摩擦轮调速 ======== */
#if VT03_ENABLE
    if (!toe_is_error(VT03_TOE))
    {
        int16_t dial = rc->rc.ch[4];
        if (dial > 200)
            kb_cmd.fric_speed_adj = 3;   /* 拨轮正向 = +100 */
        else if (dial < -200)
            kb_cmd.fric_speed_adj = 4;   /* 拨轮反向 = -100 */
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
```

**Commit:**
```bash
git add application/keyboard_action.c
git commit -m "feat: implement keyboard_action module with edge detection and command output"
```

---

## Task 3: VT03 mode_sw 映射

**Files:**
- Modify: `application/vt03_link.c`

**精确修改位置:** `vt03_frame_decode()` 函数内，第 109-111 行

**OLD (第 109-111 行):**
```c
    // Keep same semantic container as DBUS; mapping table can be tuned on hardware.
    rc_ctrl.rc.s[0] = (int8_t)vt03_ext.mode_sw;
    rc_ctrl.rc.s[1] = RC_SW_DOWN;
```

**NEW:**
```c
    /* VT03 mode_sw -> DBUS s[0] 兼容值
     * 说明书确认只有 3 个值 (最大值=2):
     *   C=0 -> RC_SW_DOWN(2)  安全/无力
     *   N=1 -> RC_SW_MID(3)   正常操控
     *   S=2 -> RC_SW_UP(1)    小陀螺
     *   3 不会出现，防御性填 RC_SW_DOWN */
    {
        static const int8_t mode_map[4] = {
            RC_SW_DOWN, RC_SW_MID, RC_SW_UP, RC_SW_DOWN
        };
        uint8_t idx = vt03_ext.mode_sw & 0x03u;
        rc_ctrl.rc.s[0] = mode_map[idx];
        rc_ctrl.rc.s[1] = RC_SW_DOWN;
    }
```

**Commit:**
```bash
git add application/vt03_link.c
git commit -m "feat: add VT03 mode_sw to DBUS switch mapping table"
```

---

## Task 4: gimbal_task 集成 keyboard_action

**Files:**
- Modify: `application/gimbal_task.c`

### Step 1: 添加 include（第 42 行之后）

**OLD (第 42 行):**
```c
#include "pid.h"
```

**NEW:**
```c
#include "pid.h"
#include "keyboard_action.h"
```

### Step 2: 添加 init 调用（第 408 行之后）

**OLD (第 405-408 行):**
```c
    gimbal_init(&gimbal_control);
    //shoot init
    //射击初始化
    shoot_init();
```

**NEW:**
```c
    gimbal_init(&gimbal_control);
    //shoot init
    //射击初始化
    shoot_init();
    //keyboard action init
    keyboard_action_init();
```

### Step 3: 在 while(1) 循环开头调用 update（第 422 行之前）

**OLD (第 417-422 行):**
```c
    while (1)
    {
        int16_t fric1_current = 0;
        int16_t fric2_current = 0;

        gimbal_set_mode(&gimbal_control);                    //设置云台控制模式
```

**NEW:**
```c
    while (1)
    {
        int16_t fric1_current = 0;
        int16_t fric2_current = 0;

        keyboard_action_update();
        gimbal_set_mode(&gimbal_control);                    //设置云台控制模式
```

### Step 4: 修改 CAN 离线检查（第 441 行）

**OLD (第 441 行):**
```c
        if (toe_is_error(DBUS_TOE) || toe_is_error(YAW_GIMBAL_MOTOR_TOE) || toe_is_error(PITCH_GIMBAL_MOTOR_TOE) || toe_is_error(TRIGGER_MOTOR_TOE))
```

**NEW:**
```c
        if ((toe_is_error(DBUS_TOE)
#if VT03_ENABLE
            && toe_is_error(VT03_TOE)
#endif
            ) || toe_is_error(YAW_GIMBAL_MOTOR_TOE) || toe_is_error(PITCH_GIMBAL_MOTOR_TOE) || toe_is_error(TRIGGER_MOTOR_TOE))
```

**Commit:**
```bash
git add application/gimbal_task.c
git commit -m "feat: integrate keyboard_action into gimbal_task, fix VT03 offline check"
```

---

## Task 5: 清理 180° 掉头死代码（可选）

**说明:** `GIMBAL_YAW_CONTINUOUS_TURN` 当前为 1（gimbal_task.h:129），所以 `gimbal_behaviour.c` 第 715-750 行的 `#if !GIMBAL_YAW_CONTINUOUS_TURN` 块**已经不参与编译**。这段代码是死代码。

**Files:**
- Modify: `application/gimbal_behaviour.c`

**删除第 715-750 行的整个 `#if !GIMBAL_YAW_CONTINUOUS_TURN ... #endif` 块:**

```c
// 删除以下代码（第 715-750 行）:
#if !GIMBAL_YAW_CONTINUOUS_TURN
    {
        static uint16_t last_turn_keyboard = 0;
        static uint8_t gimbal_turn_flag = 0;
        static fp32 gimbal_end_angle = 0.0f;

        if ((gimbal_control_set->gimbal_rc_ctrl->key.v & TURN_KEYBOARD) && !(last_turn_keyboard & TURN_KEYBOARD))
        {
            if (gimbal_turn_flag == 0)
            {
                gimbal_turn_flag = 1;
                gimbal_end_angle = rad_format(gimbal_control_set->gimbal_yaw_motor.absolute_angle + PI);
            }
        }
        last_turn_keyboard = gimbal_control_set->gimbal_rc_ctrl->key.v;

        if (gimbal_turn_flag)
        {
            if (rad_format(gimbal_end_angle - gimbal_control_set->gimbal_yaw_motor.absolute_angle) > 0.0f)
            {
                *yaw += TURN_SPEED;
            }
            else
            {
                *yaw -= TURN_SPEED;
            }
        }
        if (gimbal_turn_flag && fabs(rad_format(gimbal_end_angle - gimbal_control_set->gimbal_yaw_motor.absolute_angle)) < 0.01f)
        {
            gimbal_turn_flag = 0;
        }
    }
#endif
```

**Commit:**
```bash
git add application/gimbal_behaviour.c
git commit -m "refactor: remove dead 180-degree turn code block"
```

---

## Task 6: gimbal_behaviour 离线判断 + Z 键无力

**Files:**
- Modify: `application/gimbal_behaviour.c`

### Step 1: 添加 include（第 82-83 行之间）

在 `#include "gimbal_behaviour.h"` 之后添加:
```c
#include "keyboard_action.h"
#include "detect_task.h"
```

注意: `detect_task.h` 可能已通过其他头文件间接包含，但显式包含更安全。

### Step 2: 修改离线判断（第 516-519 行）

**OLD:**
```c
    if( toe_is_error(DBUS_TOE))
    {
        gimbal_behaviour = GIMBAL_ZERO_FORCE;
    }
```

**NEW:**
```c
    if (toe_is_error(DBUS_TOE)
#if VT03_ENABLE
        && toe_is_error(VT03_TOE)
#endif
    )
    {
        gimbal_behaviour = GIMBAL_ZERO_FORCE;
    }

    /* Z 键 toggle 无力模式 */
    if (get_kb_zero_force())
    {
        gimbal_behaviour = GIMBAL_ZERO_FORCE;
    }
```

### Step 3: 修改 init 退出条件（第 490-491 行）

**OLD:**
```c
        if (init_time < GIMBAL_INIT_TIME && init_stop_time < GIMBAL_INIT_STOP_TIME &&
            !switch_is_down(gimbal_mode_set->gimbal_rc_ctrl->rc.s[GIMBAL_MODE_CHANNEL]) && !toe_is_error(DBUS_TOE))
```

**NEW:**
```c
        if (init_time < GIMBAL_INIT_TIME && init_stop_time < GIMBAL_INIT_STOP_TIME &&
            !switch_is_down(gimbal_mode_set->gimbal_rc_ctrl->rc.s[GIMBAL_MODE_CHANNEL]) &&
            (  !toe_is_error(DBUS_TOE)
#if VT03_ENABLE
            || !toe_is_error(VT03_TOE)
#endif
            ))
```

**Commit:**
```bash
git add application/gimbal_behaviour.c
git commit -m "feat: VT03 offline fallback + pause emergency stop in gimbal_behaviour"
```

---

## Task 7: 底盘集成 — CTRL 切换 + 离线兼容

**Files:**
- Modify: `application/chassis_behaviour.c`
- Modify: `application/chassis_task.c`

### chassis_behaviour.c

#### Step 1: 添加 include（第 83-84 行之间）

在 `#include "chassis_behaviour.h"` 之后添加:
```c
#include "keyboard_action.h"
```

#### Step 2: 在 chassis_behaviour_mode_set 中添加键盘覆盖（第 258-259 行之间）

在现有三个 `else if` 判断（第 245-258 行）之后，`gimbal_cmd_to_chassis_stop()` 检查（第 262 行）之前，插入:

**在第 258 行 `}` 之后插入:**
```c

    /* CTRL 键盘覆盖底盘模式（仅 s[0]!=DOWN 时生效） */
    if (!switch_is_down(chassis_move_mode->chassis_RC->rc.s[CHASSIS_MODE_CHANNEL]))
    {
        kb_chassis_mode_e kb_mode = get_kb_chassis_mode();
        if (kb_mode == KB_CHASSIS_SPIN)
        {
            chassis_behaviour_mode = CHASSIS_HUST_SELF_PROTECT;
        }
        /* kb_mode == KB_CHASSIS_FOLLOW 时保持 RC 开关的原始选择 */
    }
```

#### Step 3: 删除 CTRL 摇摆逻辑（第 494-531 行）

在 `chassis_infantry_follow_gimbal_yaw_control` 函数中，删除 SWING_KEY 相关的整个代码块。

**删除以下代码（第 484-537 行的 swing 相关部分）:**

**OLD (第 484-537 行):**
```c
    static fp32 swing_time = 0.0f;

    static fp32 swing_angle = 0.0f;
    static fp32 max_angle = SWING_NO_MOVE_ANGLE;
    static fp32 const add_time = PI * 0.5f * configTICK_RATE_HZ / CHASSIS_CONTROL_TIME_MS;

    static uint8_t swing_flag = 0;

    if (chassis_move_rc_to_vector->chassis_RC->key.v & SWING_KEY)
    {
        if (swing_flag == 0)
        {
            swing_flag = 1;
            swing_time = 0.0f;
        }
    }
    else
    {
        swing_flag = 0;
    }

    if (chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_FRONT_KEY || chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_BACK_KEY ||
        chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_LEFT_KEY || chassis_move_rc_to_vector->chassis_RC->key.v & CHASSIS_RIGHT_KEY)
    {
        max_angle = SWING_MOVE_ANGLE;
    }
    else
    {
        max_angle = SWING_NO_MOVE_ANGLE;
    }

    if (swing_flag)
    {
        swing_angle = max_angle * arm_sin_f32(swing_time);
        swing_time += add_time;
    }
    else
    {
        swing_angle = 0.0f;
    }
    if (swing_time > 2 * PI)
    {
        swing_time -= 2 * PI;
    }


    *angle_set = swing_angle;
```

**NEW:**
```c
    *angle_set = 0.0f;
```

### chassis_task.c

#### Step 4: 修改启动等待（第 176 行）

**OLD:**
```c
    while (toe_is_error(CHASSIS_MOTOR1_TOE) || toe_is_error(CHASSIS_MOTOR2_TOE) || toe_is_error(CHASSIS_MOTOR3_TOE) || toe_is_error(CHASSIS_MOTOR4_TOE) || toe_is_error(DBUS_TOE))
```

**NEW:**
```c
    while (toe_is_error(CHASSIS_MOTOR1_TOE) || toe_is_error(CHASSIS_MOTOR2_TOE) || toe_is_error(CHASSIS_MOTOR3_TOE) || toe_is_error(CHASSIS_MOTOR4_TOE) || (toe_is_error(DBUS_TOE)
#if VT03_ENABLE
        && toe_is_error(VT03_TOE)
#endif
    ))
```

#### Step 5: 修改离线零电流（第 205 行）

**OLD:**
```c
            if (toe_is_error(DBUS_TOE))
```

**NEW:**
```c
            if (toe_is_error(DBUS_TOE)
#if VT03_ENABLE
                && toe_is_error(VT03_TOE)
#endif
            )
```

#### Step 6: 在 chassis_task.c 添加 include

在 include 区域末尾添加:
```c
#include "uart_mode.h"
#include "detect_task.h"
```

注意: `detect_task.h` 可能已通过其他头文件包含（检查 chassis_task.c 的现有 include），如果已有则不需重复。根据代码已有 `#include "detect_task.h"`，只需添加 `uart_mode.h`（如果尚未包含的话）。实际上 `VT03_ENABLE` 和 `VT03_TOE` 需要这两个头文件。检查现有 include 列表后决定是否需要补充。

**Commit:**
```bash
git add application/chassis_behaviour.c application/chassis_task.c
git commit -m "feat: CTRL toggles chassis mode, fix VT03 offline checks in chassis"
```

---

## Task 8: 发射系统改造 — VT03 兼容

**Files:**
- Modify: `application/shoot.c`
- Modify: `application/shoot.h`

### 关键问题说明

**`SHOOT_RC_MODE_CHANNEL = 1`**，即发射系统用 `s[1]` 控制。而 VT03 的 `s[1]` 永远是 `RC_SW_DOWN`。这意味着：
- `switch_is_up(s[1])` → VT03 下永远 false（UP toggle 不工作）
- `switch_is_mid(s[1])` → VT03 下永远 false（键盘模式不激活）
- `switch_is_down(s[1])` → VT03 下永远 true

因此 VT03 需要独立的发射激活路径，使用 `s[0]` 代替 `s[1]`。

### shoot.h 修改

无需修改 `SHOOT_RC_MODE_CHANNEL`（保持 DBUS 兼容）。

### shoot.c 修改

#### Step 1: 添加 include（第 18 行之后）

**OLD (第 18 行):**
```c
#include "shoot.h"
```

**NEW:**
```c
#include "shoot.h"
#include "keyboard_action.h"
#if VT03_ENABLE
#include "vt03_link.h"
#endif
```

#### Step 2: 重写 shoot_set_mode 函数（第 293-431 行）

**OLD（完整 shoot_set_mode 函数，第 293-431 行）:**
```c
static void shoot_set_mode(void)
{
    static int8_t last_s = RC_SW_UP;
    bool_t key_q;
    bool_t key_c;
    bool_t key_f;
    bool_t key_r;

    //上拨判断， 一次开启，再次关闭
    if ((switch_is_up(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]) && !switch_is_up(last_s) && shoot_control.shoot_mode == SHOOT_STOP))
    {
        shoot_control.shoot_mode = SHOOT_READY_FRIC;
    }
    else if ((switch_is_up(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]) && !switch_is_up(last_s) && shoot_control.shoot_mode != SHOOT_STOP))
    {
        shoot_control.shoot_mode = SHOOT_STOP;
    }

    key_q = (shoot_control.shoot_rc->key.v & SHOOT_TOGGLE_KEYBOARD) ? 1 : 0;
    key_c = (shoot_control.shoot_rc->key.v & SHOOT_HIGH_FREQ_KEYBOARD) ? 1 : 0;
    key_f = (shoot_control.shoot_rc->key.v & SHOOT_FRIC_DEC_KEYBOARD) ? 1 : 0;
    key_r = (shoot_control.shoot_rc->key.v & SHOOT_BURST_KEYBOARD) ? 1 : 0;

    if (key_r && !shoot_control.last_key_r)
    {
        shoot_control.burst_mode = !shoot_control.burst_mode;
    }

    if (switch_is_mid(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]))
    {
        if (key_q && !shoot_control.last_key_q)
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

        if (key_c && !shoot_control.last_key_c)
        {
            shoot_control.high_freq_flag = !shoot_control.high_freq_flag;
        }

        if (key_f && !shoot_control.last_key_f)
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
    }

    shoot_control.last_key_q = key_q;
    shoot_control.last_key_c = key_c;
    shoot_control.last_key_f = key_f;
    shoot_control.last_key_r = key_r;
```

**NEW:**
```c
static void shoot_set_mode(void)
{
    static int8_t last_s = RC_SW_UP;
    bool_t key_q;
    bool_t key_c;
    bool_t key_f;
    bool_t key_r;
    const keyboard_cmd_t *kb_cmd;

    kb_cmd = get_keyboard_cmd();

    /* ---- DBUS 路径: s[1] 上拨 toggle（VT03 下 s[1]=DOWN，此段不触发） ---- */
    if ((switch_is_up(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]) && !switch_is_up(last_s) && shoot_control.shoot_mode == SHOOT_STOP))
    {
        shoot_control.shoot_mode = SHOOT_READY_FRIC;
    }
    else if ((switch_is_up(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL]) && !switch_is_up(last_s) && shoot_control.shoot_mode != SHOOT_STOP))
    {
        shoot_control.shoot_mode = SHOOT_STOP;
    }

    /* ---- Q 键 / fn_1 短按: 发射系统 toggle（通过 keyboard_cmd） ---- */
    if (kb_cmd->shoot_toggle)
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

    if (key_r && !shoot_control.last_key_r)
    {
        shoot_control.burst_mode = !shoot_control.burst_mode;
    }

    /* 键盘辅助功能: C(高频), F(摩擦轮调速)
     * DBUS 路径: 仅 s[1]=MID 时生效
     * VT03 路径: s[0]!=DOWN 时生效 */
    if (switch_is_mid(shoot_control.shoot_rc->rc.s[SHOOT_RC_MODE_CHANNEL])
#if VT03_ENABLE
        || (!toe_is_error(VT03_TOE) && !switch_is_down(shoot_control.shoot_rc->rc.s[0]))
#endif
    )
    {
        if (key_c && !shoot_control.last_key_c)
        {
            shoot_control.high_freq_flag = !shoot_control.high_freq_flag;
        }

        if (key_f && !shoot_control.last_key_f)
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
    }

    shoot_control.last_key_q = key_q;
    shoot_control.last_key_c = key_c;
    shoot_control.last_key_f = key_f;
    shoot_control.last_key_r = key_r;
```

**注意:** Q 键 toggle 现在通过 `keyboard_cmd.shoot_toggle` 统一处理（合并了 Q 键和 fn_1 短按），原有 `key_q` / `last_key_q` 保留用于 DBUS s[1]=MID 路径的兼容。

#### Step 3: VT03 trigger 支持

在 `shoot_feedback_update` 函数中，第 486 行之后插入:

**在第 486 行 `shoot_control.press_r = shoot_control.shoot_rc->mouse.press_r;` 之后添加:**
```c

    /* VT03 trigger 等同鼠标左键 */
#if VT03_ENABLE
    if (!toe_is_error(VT03_TOE))
    {
        const keyboard_cmd_t *cmd = get_keyboard_cmd();
        if (cmd->vt03_trigger)
        {
            shoot_control.press_l = 1;
        }
    }
#endif
```

#### Step 4: shoot.h 无需修改

`last_key_q` 保留在 `shoot_control_t` 中，不做删除。`shoot_init()` 中的 `shoot_control.last_key_q = 0;` 也保留。

**Commit:**
```bash
git add application/shoot.c application/shoot.h
git commit -m "feat: VT03 shoot auto-enable via s[0], remove Q key toggle, add trigger support"
```

---

## Task 9: 将 keyboard_action.c 加入 Keil 工程

**Files:**
- Modify: `MDK-ARM/standard_robot.uvprojx`

**精确位置:** 在 `vt03_link.c` 文件条目之后（XML 约第 917 行），`</Files>` 之前。

**在以下 XML 之后:**
```xml
            <File>
              <FileName>vt03_link.c</FileName>
              <FileType>1</FileType>
              <FilePath>..\application\vt03_link.c</FilePath>
            </File>
```

**插入:**
```xml
            <File>
              <FileName>keyboard_action.c</FileName>
              <FileType>1</FileType>
              <FilePath>..\application\keyboard_action.c</FilePath>
            </File>
```

**验证:** 用 Keil 打开工程，确认 application group 中有 `keyboard_action.c`，Build (F7) 应该 0 errors。

**Commit:**
```bash
git add MDK-ARM/standard_robot.uvprojx
git commit -m "build: add keyboard_action.c to Keil project"
```

---

## Task 10: 编译验证

**操作:** 打开 Keil MDK，Build (F7) 整个工程。

**期望结果:**
- 0 errors
- 可能有 warnings: `last_key_q` 未使用（如果清理不干净）、预留接口未使用
- .hex / .bin 正常生成

如果有编译错误，根据错误信息修复（常见问题：头文件循环包含、armcc 变量声明位置）。

---

## Task 11: 硬件验证清单

烧录后用 VT13 遥控器逐项测试:

**基础功能:**
1. **mode_sw 映射**: 拨动模式开关，确认 C(0)=无力, N(1)=跟随, S(2)=小陀螺
2. **WASD 移动**: 键盘控制底盘前后左右
3. **鼠标瞄准**: 鼠标控制云台 yaw/pitch
4. **DBUS 回退**: 断开 VT03，接上 DT7，确认原有全部功能正常

**键盘键位（需接键盘到 VT13 端电脑）:**
5. **Q 键**: 发射系统开关 toggle
6. **CTRL 键**: 底盘模式切换（跟随↔小陀螺）
7. **Z 键**: 无力模式 toggle（进入/退出 ZERO_FORCE）
8. **鼠标左键**: 短按单发，长按连发
9. **C/R/F/G 键**: 高频 toggle、连发 toggle、摩擦轮调速、堵转反转
10. **SHIFT 键**: 超级电容标志位（确认 cap_enable=1）

**VT13 物理按键（不接键盘纯遥控器操作）:**
11. **fn_1 短按**: 发射系统开关（等同 Q）
12. **fn_1 长按**: 高频模式 toggle（等同 C）
13. **fn_2 短按**: 连发模式 toggle（等同 R）
14. **fn_2 长按**: 堵转反转（等同 G）
15. **pause 短按**: 超级电容 toggle
16. **trigger**: 射击（等同鼠标左键）
17. **拨轮**: 摩擦轮调速（正向+100，反向-100）

---

## 风险与注意事项

1. **mode_sw 映射已参考说明书**: C=0→DOWN, N=1→MID, S=2→UP，索引3防御填DOWN。仍需实物确认拨杆物理位置
2. **s[1] 永远 DOWN**: VT03 下 SHOOT_RC_MODE_CHANNEL(s[1]) 无效，已通过 keyboard_cmd.shoot_toggle 解决
3. **mouse.press_l 位宽差异**: VT03 是 2-bit (0-3)，DBUS 是 1-bit (0/1)。shoot.c 用非零判断，兼容
4. **armcc 变量声明**: 所有变量必须在块开头声明，不能在代码中间
5. **超级电容 cap_enable**: 标志位已实现（SHIFT hold + pause 短按 toggle），实际硬件控制逻辑需后续接入
6. **DBUS/VT03 切换状态残留**: `kb_chassis_mode`、`kb_zero_force`、`kb_cap_on` 等 static 变量在设备切换时不会重置。如需要可在 detect 回调中清零
7. **长按阈值 300ms**: 可根据手感调整 `BTN_LONG_PRESS_MS`，过短容易误触长按，过长操作不灵敏
8. **拨轮死区 ±200**: 防止拨轮漂移误触发摩擦轮调速，可根据实测调整
