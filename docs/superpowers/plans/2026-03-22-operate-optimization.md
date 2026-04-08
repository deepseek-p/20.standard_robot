# 操作优化实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现射速两挡位切换、小陀螺方向锁定、遥测 bullet_speed 上报

**Architecture:** 修改 shoot 模块的摩擦轮调速逻辑为离散挡位数组；删除小陀螺方向判断改为固定正值；在遥测末尾追加裁判系统射速列

**Tech Stack:** STM32F407, FreeRTOS, Keil MDK (armcc), C99

**Spec:** `docs/superpowers/specs/2026-03-22-operate-optimization-design.md`

---

## File Map

| 文件 | 操作 | 职责 |
|------|------|------|
| `application/shoot.h` | 修改 | 删除旧速度宏，新增挡位宏 |
| `application/shoot.c` | 修改 | 挡位切换逻辑替换连续调速 |
| `application/chassis_behaviour.c` | 修改 | wz_set 固定正值 |
| `application/usb_task.c` | 修改 | 新增 bullet_spd 遥测列 |
| `D:/tools/stm32-telemetry-mcp/frame_parser.py` | 修改 | 新增列69定义 |

---

### Task 1: 射速挡位 — shoot.h 宏替换

**Files:**
- Modify: `application/shoot.h:69-73`

- [ ] **Step 1: 替换摩擦轮速度宏**

将 shoot.h 第 69-73 行：
```c
//摩擦轮目标转速（不变）
#define FRIC_SPEED_LOW              4900.0f
#define FRIC_SPEED_MID              5800.0f
#define FRIC_SPEED_HIGH             7400.0f
#define FRIC_SPEED_ADJUST_STEP      100.0f
```

替换为：
```c
//摩擦轮挡位（两挡，RPM 待标定）
#define FRIC_GEAR_COUNT             2
#define FRIC_SPEED_GEAR_0           4900.0f   // ~20m/s，待标定
#define FRIC_SPEED_GEAR_1           5800.0f   // ~24m/s，待标定
```

- [ ] **Step 2: Commit**

```bash
git add application/shoot.h
git commit -m "refactor(shoot): replace fric speed constants with 2-gear macros"
```

---

### Task 2: 射速挡位 — shoot.c 逻辑改写

**Files:**
- Modify: `application/shoot.c:112, 185, 336-372`

- [ ] **Step 1: 在文件顶部（shoot_control 定义附近）新增静态变量**

在 `shoot_init()` 函数之前（约第 30-40 行，其他 static 变量附近），添加：
```c
static uint8_t fric_gear = 0;
static const fp32 fric_speed_table[FRIC_GEAR_COUNT] = {
    FRIC_SPEED_GEAR_0, FRIC_SPEED_GEAR_1
};
```

- [ ] **Step 2: 修改初始化（第 112 行）**

将：
```c
    shoot_control.fric_speed_base = FRIC_SPEED_LOW;
```
改为：
```c
    fric_gear = 0;
    shoot_control.fric_speed_base = fric_speed_table[0];
```

- [ ] **Step 3: 修改 SHOOT_STOP 复位（第 185 行）**

将：
```c
        shoot_control.fric_speed_base = FRIC_SPEED_LOW;
```
改为：
```c
        fric_gear = 0;
        shoot_control.fric_speed_base = fric_speed_table[0];
```

- [ ] **Step 4: 替换键盘调速逻辑（第 336-372 行）**

将整个 `fric_speed_adj` + F 键 + 硬限幅段（第 336-372 行）：
```c
        if (kb_cmd->fric_speed_adj == 1u)
        {
            shoot_control.fric_speed_base += FRIC_SPEED_ADJUST_STEP;
        }
        else if (kb_cmd->fric_speed_adj == 2u)
        {
            shoot_control.fric_speed_base -= FRIC_SPEED_ADJUST_STEP;
        }
        else if (kb_cmd->fric_speed_adj == 3u && shoot_control.arm_enable)
        {
            shoot_control.fric_speed_base += FRIC_SPEED_ADJUST_STEP;
        }
        else if (kb_cmd->fric_speed_adj == 4u && shoot_control.arm_enable)
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

        /* 硬限幅 fric_speed_base */
        if (shoot_control.fric_speed_base < FRIC_SPEED_LOW)
        {
            shoot_control.fric_speed_base = FRIC_SPEED_LOW;
        }
        else if (shoot_control.fric_speed_base > FRIC_SPEED_HIGH)
        {
            shoot_control.fric_speed_base = FRIC_SPEED_HIGH;
        }
```

替换为：
```c
        /* 挡位切换：adj 1/3 升挡，adj 2/4 降挡 */
        if (kb_cmd->fric_speed_adj == 1u || (kb_cmd->fric_speed_adj == 3u && shoot_control.arm_enable))
        {
            if (fric_gear < FRIC_GEAR_COUNT - 1)
            {
                fric_gear++;
            }
        }
        else if (kb_cmd->fric_speed_adj == 2u || (kb_cmd->fric_speed_adj == 4u && shoot_control.arm_enable))
        {
            if (fric_gear > 0)
            {
                fric_gear--;
            }
        }
        else if (key_f && !shoot_control.last_key_f)
        {
            if (shoot_control.shoot_rc->key.v & SHOOT_FRIC_INC_KEYBOARD)
            {
                /* Shift+F: 升挡 */
                if (fric_gear < FRIC_GEAR_COUNT - 1)
                {
                    fric_gear++;
                }
            }
            else
            {
                /* F alone: 降挡 */
                if (fric_gear > 0)
                {
                    fric_gear--;
                }
            }
        }
        shoot_control.fric_speed_base = fric_speed_table[fric_gear];
```

- [ ] **Step 5: Commit**

```bash
git add application/shoot.c
git commit -m "feat(shoot): implement 2-gear fric speed switching"
```

---

### Task 3: 小陀螺方向锁定

**Files:**
- Modify: `application/chassis_behaviour.c:454-461`

- [ ] **Step 1: 替换方向判断为固定正值**

将第 454-461 行：
```c
    if (chassis_move_rc_to_vector->chassis_RC->rc.ch[CHASSIS_WZ_CHANNEL] < -CHASSIS_RC_DEADLINE)
    {
        *wz_set = -CHASSIS_HUST_SELF_PROTECT_WZ;
    }
    else
    {
        *wz_set = CHASSIS_HUST_SELF_PROTECT_WZ;
    }
```

替换为：
```c
    *wz_set = CHASSIS_HUST_SELF_PROTECT_WZ;
```

- [ ] **Step 2: Commit**

```bash
git add application/chassis_behaviour.c
git commit -m "feat(chassis): lock spin direction to positive in self-protect mode"
```

---

### Task 4: 遥测新增 bullet_speed

**Files:**
- Modify: `application/usb_task.c:45附近, 1284, 1357`

- [ ] **Step 1: 添加 `#include "referee.h"` 和 extern 声明**

在 usb_task.c 的 `#include` 区域添加：
```c
#include "referee.h"
```

在已有的 `extern shoot_control_t shoot_control;`（第 45 行）附近添加：
```c
extern ext_shoot_data_t shoot_data_t;
```

- [ ] **Step 2: 在 snprintf 格式串末尾追加一个 `%ld`**

在第 1284 行的格式串中，最后一个 `%ld` 后、`\r\n` 前，追加 `,%ld`。

即将：
```c
...%ld,%ld\r\n",
```
改为：
```c
...%ld,%ld,%ld\r\n",
```

- [ ] **Step 3: 在 snprintf 参数列表末尾追加 bullet_speed 值**

在第 1357 行 `pitch_gyro_set` 参数后，闭合括号前，追加：
```c
                    usb_debug_masked_fp32_milli(USB_DBG_CH_SHOOT, shoot_data_t.bullet_speed));
```

注意：最后一个原参数的 `));` 改为 `),`，新行末尾为 `));`。

> **缩放约定：** 使用 `usb_debug_masked_fp32_milli`（×1000），与遥测中其他 shoot 列（fric_set, local_heat 等）一致。MCP parser 中对应列标记 `True`（milli-scaled）。规格中的 centi-scaled 描述已被此计划覆盖。

- [ ] **Step 4: Commit**

```bash
git add application/usb_task.c
git commit -m "feat(telemetry): add bullet_speed column (index 69)"
```

---

### Task 5: MCP 遥测解析器更新

**Files:**
- Modify: `D:/tools/stm32-telemetry-mcp/frame_parser.py:83-84, 125`

- [ ] **Step 1: 在 COLUMNS 字典中插入 bullet_spd**

在第 83 行 `68: ("pitch_gyro_set", ...)` 后，现有的 `69: ("supercap_online", ...)` 前，需要将 supercap 及后续列全部后移一位（69→70, 70→71, ..., 83→84）。

> **注意：** MCP parser 中 69-83 列是预留定义，当前固件只发送 69 列（0-68）。插入到索引 69 不会破坏任何现有数据流。

在原 68 行后插入：
```python
    69: ("bullet_spd",    "shoot",   True),
```

然后将原 69-83 的索引全部 +1（变为 70-84）。

- [ ] **Step 2: 更新 TOTAL_COLUMNS**

将：
```python
TOTAL_COLUMNS = 84
```
改为：
```python
TOTAL_COLUMNS = 85
```

- [ ] **Step 3: 更新 test_standalone.py 中的期望列数**

将 `EXPECTED_TOTAL_COLUMNS = 84` 改为 `EXPECTED_TOTAL_COLUMNS = 85`。

- [ ] **Step 4: 运行 MCP 测试验证**

```bash
cd D:/tools/stm32-telemetry-mcp && python test_standalone.py
```

Expected: 所有测试通过，列数 = 85。

- [ ] **Step 5: Commit**

```bash
cd D:/tools/stm32-telemetry-mcp && git add frame_parser.py test_standalone.py && git commit -m "feat: add bullet_spd column (index 69), shift supercap/power columns"
```

---

### Task 6: 编译验证

- [ ] **Step 1: Keil 编译**

用 Keil 编译 `MDK-ARM/standard_robot.uvprojx`，确认零 error。warning 可接受。

- [ ] **Step 2: 上车功能验证清单**

以下项目上车后逐一验证：
1. 遥控器 MID 模式：F 键降挡（无反应，已在最低挡），Shift+F 升挡到 24m/s 档，再按无反应
2. 切 STOP 再切回 MID：挡位复位到 gear 0
3. 遥控器 UP 模式：底盘始终同一方向旋转，转动云台不影响底盘旋转方向
4. 遥测中 bullet_spd 列有值（发射弹丸后更新）
5. VT03 拨盘调速：拨盘上/下 → 升/降挡（如有 VT03 可测）
