# Codex Task: 底盘功率限制比赛双版本

## 上下文

分支 `codex/power-limit-ui`，后天比赛，需要两版固件。
详细分析见 `docs/ai_sessions/2026-03-25_power_model_coefficient_analysis.md`。

裁判系统**不再实时反馈 chassis_power**，只有 buffer 能量可用。

## 要做的事

### Step 1: 回退模型系数到原参数

文件 `application/chassis_power_control.h`，把第 22-25 行：

```c
#define MOTOR_TORQUE_COEFF              2.16e-6f
#define MOTOR_A                         1.83e-07f
#define MOTOR_K2                        2.09e-07f
#define MOTOR_CONSTANT                  2.21f
```

改为：

```c
#define MOTOR_TORQUE_COEFF              1.99688994e-6f
#define MOTOR_K2                        1.453e-07f
#define MOTOR_A                         1.23e-07f
#define MOTOR_CONSTANT                  4.081f
```

注意 K2 和 A 的声明顺序：原版 K2 在 A 前面，当前分支是反的，请一并修正顺序。

### Step 2: 加 `POWER_LIMIT_AGGRESSIVE` 宏开关

同一个文件 `application/chassis_power_control.h`，在系数定义上方加：

```c
#ifndef POWER_LIMIT_AGGRESSIVE
#define POWER_LIMIT_AGGRESSIVE  0   /* 0=V1 conservative, 1=V2 aggressive */
#endif
```

把 `BUFFER_ENERGY_SETPOINT` 改为条件编译：

```c
#if POWER_LIMIT_AGGRESSIVE
#define BUFFER_ENERGY_SETPOINT          45.0f
#else
#define BUFFER_ENERGY_SETPOINT          50.0f
#endif
```

`BUFFER_EMERGENCY_THRESHOLD` 和 `EFFECTIVE_POWER_LIMIT_MIN` 不动。

### Step 3: buffer PID 初始化改为条件编译

文件 `application/chassis_task.c`，找到第 258 行附近：

```c
const static fp32 buffer_energy_pid[3] = {2.0f, 0.1f, 0.0f};
```

和第 288 行：

```c
PID_init(&chassis_move_init->buffer_pid, PID_POSITION, buffer_energy_pid, 40.0f, 20.0f);
```

改为：

```c
#if POWER_LIMIT_AGGRESSIVE
    const static fp32 buffer_energy_pid[3] = {3.0f, 0.15f, 0.0f};
#else
    const static fp32 buffer_energy_pid[3] = {2.0f, 0.1f, 0.0f};
#endif
```

```c
#if POWER_LIMIT_AGGRESSIVE
    PID_init(&chassis_move_init->buffer_pid, PID_POSITION, buffer_energy_pid, 50.0f, 25.0f);
#else
    PID_init(&chassis_move_init->buffer_pid, PID_POSITION, buffer_energy_pid, 40.0f, 20.0f);
#endif
```

### Step 4: 不要动的东西

- `chassis_power_control.c` 整个文件不动
- `quadratic_solve_current()` 不动
- `motor_power_estimate()` 不动
- emergency scale 逻辑不动
- positive-only 累加不动
- `chassis_power_control_set_enabled()` / `get_enabled()` 不动
- 遥测帧不动
- `power_est` / `effective_limit` 字段不动
- `(void)chassis_power;` 这行不动（chassis_power 确实不可用）

## 改动总结

只改 2 个文件，共约 15 行：

| 文件 | 改什么 |
|---|---|
| `application/chassis_power_control.h` | 系数回退 + POWER_LIMIT_AGGRESSIVE 宏 + BUFFER_ENERGY_SETPOINT 条件编译 |
| `application/chassis_task.c` | buffer_energy_pid 数组 + PID_init 参数 条件编译 |

## 两版参数对照

| | V1 (`AGGRESSIVE=0`) | V2 (`AGGRESSIVE=1`) |
|---|---|---|
| kt | 1.99688994e-6 | 1.99688994e-6 |
| k2 | 1.453e-7 | 1.453e-7 |
| a | 1.23e-7 | 1.23e-7 |
| c | 4.081 | 4.081 |
| Kp | 2.0 | 3.0 |
| Ki | 0.1 | 0.15 |
| max_out | 40 | 50 |
| max_iout | 20 | 25 |
| setpoint | 50J | 45J |

## 验证

编译两次（`AGGRESSIVE=0` 和 `=1`），确认无编译错误即可。
板端验证由操作手在赛前场地完成。
