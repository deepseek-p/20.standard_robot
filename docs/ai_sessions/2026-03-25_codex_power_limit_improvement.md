# Codex 修改建议：底盘功率限制 —— 比赛双版本方案

> 交接日期: 2026-03-25
> 分支: `codex/power-limit-ui`
> 基于: `2026-03-25_power_model_coefficient_analysis.md` 的分析结论
> 紧急度: **后天比赛**，需要两版可切换的固件

## 背景

当前功率模型在高负载下低估（拟合标签 scap_power 高负载低报 ~31%）。
裁判系统**不再实时反馈 chassis_power**，所以在线校正方案（方案 A）不可行。

需要两版固件：
- **V1 保守版**：原参数 + main 的功率限制策略，已验证不会 buffer 归零
- **V2 加固版**：原参数 + 更激进的 buffer PID，应对比赛场地高摩擦

## 策略说明

两版**共用原始 Motor-modeling 系数**（不用新拟合系数），原因：
1. 原参数在实测中从未出现 buffer 归零
2. 原参数常数项偏高（4.081W/电机），导致模型整体偏保守，**限流偏早**
3. 偏保守 = 更安全，比赛中宁可少一点动力也不能超功率罚电
4. 新拟合系数（分支当前值）虽然低负载更准，但高负载有低估风险

V2 在 V1 基础上只改 buffer PID 参数，不改模型系数和限流逻辑。

---

## V1 保守版（默认比赛固件）

### 改动：回退系数到原参数

文件: `application/chassis_power_control.h`

```c
// 修改前 (分支当前):
#define MOTOR_TORQUE_COEFF   2.16e-6f
#define MOTOR_K2             2.09e-07f
#define MOTOR_A              1.83e-07f
#define MOTOR_CONSTANT       2.21f

// 修改后 (回退到 main 原参):
#define MOTOR_TORQUE_COEFF   1.99688994e-6f
#define MOTOR_K2             1.453e-07f
#define MOTOR_A              1.23e-07f
#define MOTOR_CONSTANT       4.081f
```

### 改动：保持 main 的 buffer PID 参数不变

文件: `application/chassis_task.c` — 确认当前值为：

```c
const static fp32 buffer_energy_pid[3] = {2.0f, 0.1f, 0.0f};
PID_init(&chassis_move_init->buffer_pid, PID_POSITION, buffer_energy_pid, 40.0f, 20.0f);
```

文件: `application/chassis_power_control.h` — 确认当前值为：

```c
#define BUFFER_ENERGY_SETPOINT          50.0f
#define BUFFER_EMERGENCY_THRESHOLD      20.0f
#define EFFECTIVE_POWER_LIMIT_MIN       20.0f
```

### V1 预期行为

- 空载低估（预测 16W，实际 ~4W）→ 不影响，因为远低于限制
- 中高负载偏保守（常数项高导致模型整体预测偏高）→ 限流偏早，动力略弱但安全
- 与之前实测一致：buffer 不会归零

---

## V2 加固版（备用，高摩擦场地）

在 V1 基础上，**只改 buffer PID 参数**，让安全网更激进。

### 改动：Buffer PID 提高响应

文件: `application/chassis_task.c`

```c
// V1:
const static fp32 buffer_energy_pid[3] = {2.0f, 0.1f, 0.0f};
PID_init(&chassis_move_init->buffer_pid, PID_POSITION, buffer_energy_pid, 40.0f, 20.0f);

// V2 改为:
const static fp32 buffer_energy_pid[3] = {3.0f, 0.15f, 0.0f};
PID_init(&chassis_move_init->buffer_pid, PID_POSITION, buffer_energy_pid, 50.0f, 25.0f);
```

### 改动：Buffer setpoint 降低，提前介入

文件: `application/chassis_power_control.h`

```c
// V1:
#define BUFFER_ENERGY_SETPOINT  50.0f

// V2 改为:
#define BUFFER_ENERGY_SETPOINT  45.0f
```

### V2 与 V1 的差异汇总

| 参数 | V1 (保守) | V2 (加固) | 效果 |
|---|---|---|---|
| 模型系数 | 原参 (main) | 原参 (main) | 相同 |
| buffer PID Kp | 2.0 | **3.0** | 更快响应 buffer 下降 |
| buffer PID Ki | 0.1 | **0.15** | 更快积累修正 |
| PID max_out | 40W | **50W** | 更大限流修正幅度 |
| PID max_iout | 20 | **25** | 积分饱和上限提高 |
| setpoint | 50J | **45J** | 在 buffer=50J 时就开始压限 |
| emergency | 20J | 20J | 相同 |

### V2 预期行为

- buffer 在 45J 就开始压限（比 V1 提前 5J 介入）
- 同等 buffer 偏差下，修正幅度更大（Kp 3.0 × 5J 偏差 = 15W 修正，V1 只有 10W）
- 高摩擦场地实际功率更大时，PID 有更大空间兜底
- 代价：正常操控时动力可能比 V1 略弱（因为 setpoint=45J 意味着 buffer 更早触发压限）

---

## 约束

- **不要动** `quadratic_solve_current()` 逻辑
- **不要动** emergency_scale 逻辑（buffer < 20J 线性降额）
- **不要动** positive-only 累加策略
- **不要** 新增 .h 文件
- **不要** 修改遥测帧的已有列顺序
- 两版固件**只有 3 行代码不同**（PID 参数 + setpoint），方便快速切换
- `chassis_power_control_set_enabled()` 功能保留，但比赛时应始终 enabled

## 切换方式

V1 → V2 只需改两个文件共 3 处：
1. `chassis_task.c`: buffer_energy_pid 数组 `{2.0, 0.1, 0.0}` → `{3.0, 0.15, 0.0}`
2. `chassis_task.c`: PID_init max_out/max_iout `40.0/20.0` → `50.0/25.0`
3. `chassis_power_control.h`: BUFFER_ENERGY_SETPOINT `50.0` → `45.0`

建议用 `#if` 宏控制：

```c
// chassis_power_control.h
#ifndef POWER_LIMIT_AGGRESSIVE
#define POWER_LIMIT_AGGRESSIVE  0   /* 0=V1保守, 1=V2加固 */
#endif

#if POWER_LIMIT_AGGRESSIVE
#define BUFFER_ENERGY_SETPOINT  45.0f
#else
#define BUFFER_ENERGY_SETPOINT  50.0f
#endif
```

```c
// chassis_task.c
#if POWER_LIMIT_AGGRESSIVE
    const static fp32 buffer_energy_pid[3] = {3.0f, 0.15f, 0.0f};
    PID_init(&chassis_move_init->buffer_pid, PID_POSITION, buffer_energy_pid, 50.0f, 25.0f);
#else
    const static fp32 buffer_energy_pid[3] = {2.0f, 0.1f, 0.0f};
    PID_init(&chassis_move_init->buffer_pid, PID_POSITION, buffer_energy_pid, 40.0f, 20.0f);
#endif
```

这样切换只需改一个宏定义，编译两次即可。

## 验证标准

### V1
- 各种操控下 buffer ≥ 40J（与之前实测一致）
- 急加急停 buffer 不低于 30J

### V2
- 各种操控下 buffer ≥ 35J
- 急加急停 buffer 不低于 20J（emergency threshold）
- 日常操控动力手感与 V1 差异可接受

### 两版都需要
- 遥控器 s0=DOWN 时底盘零力输出，不触发功率限制
- 比赛前场地实测确认 buffer 走势

## 文件改动汇总

| 文件 | V1 改动 | V2 额外改动 |
|---|---|---|
| `chassis_power_control.h` | 系数回退到原参 | + setpoint 50→45 |
| `chassis_task.c` | 确认 PID 参数不变 | PID 参数 2.0/0.1/40/20 → 3.0/0.15/50/25 |

如果用 `#if` 宏方案，还需：
| `chassis_power_control.h` | 新增 `POWER_LIMIT_AGGRESSIVE` 宏 + 条件编译 |
| `chassis_task.c` | buffer PID 初始化改为条件编译 |

## ⚠ 关键提醒

1. **裁判系统不再实时反馈 chassis_power**，不要依赖 `get_chassis_power_and_buffer()` 返回的 `chassis_power` 值做在线校正
2. `get_chassis_power_and_buffer()` 返回的 `chassis_power_buffer` (buffer 能量) **仍然可用**，buffer PID 依赖这个值
3. 分支当前的 `(void)chassis_power;` 不需要改，这一行就是因为 chassis_power 不可用才加的
4. 比赛场地摩擦系数大 → 同样遥控器输入下电机需要更大电流 → 实际功率更高 → V2 的激进 PID 更有必要
