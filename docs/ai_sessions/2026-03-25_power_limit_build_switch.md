# 2026-03-25 功率限制双版本开关

## 目标

- 执行 [2026-03-25_codex_task_prompt.md](/d:/RM资料/电控/Development-Board-C-Examples-master/Development-Board-C-Examples-master/20.standard_robot/.worktrees/power-limit-ui/docs/ai_sessions/2026-03-25_codex_task_prompt.md) 中定义的“底盘功率限制比赛双版本”任务。
- 仅修改：
  - `application/chassis_power_control.h`
  - `application/chassis_task.c`
- 明确不修改：
  - `application/chassis_power_control.c`
  - `quadratic_solve_current()`
  - `motor_power_estimate()`
  - emergency scale 逻辑
  - telemetry frame
  - `power_est/effective_limit`
  - `rm_ui.*`
  - `shoot.*`

## 本轮改动

### `application/chassis_power_control.h`

- 新增编译开关：

```c
#ifndef POWER_LIMIT_AGGRESSIVE
#define POWER_LIMIT_AGGRESSIVE 0
#endif
```

- `BUFFER_ENERGY_SETPOINT` 改为条件编译：
  - 保守版：`50.0f`
  - 激进版：`45.0f`

- 功率预测模型系数回退为文档指定值：

```c
#define MOTOR_TORQUE_COEFF 1.99688994e-6f
#define MOTOR_K2           1.453e-07f
#define MOTOR_A            1.23e-07f
#define MOTOR_CONSTANT     4.081f
```

### `application/chassis_task.c`

- `buffer_energy_pid` 改为条件编译：
  - 保守版：`{2.0f, 0.1f, 0.0f}`
  - 激进版：`{3.0f, 0.15f, 0.0f}`

- `PID_init(&chassis_move_init->buffer_pid, ...)` 改为条件编译：
  - 保守版：`40.0f, 20.0f`
  - 激进版：`50.0f, 25.0f`

## 验证

### 已完成

- `git diff --name-only -- application/chassis_power_control.h application/chassis_task.c`
  - 结果只包含：
    - `application/chassis_power_control.h`
    - `application/chassis_task.c`

- 本机最小语法探针验证：
  - 创建临时文件 `C:\Users\21886\AppData\Local\Temp\power_limit_config_probe.c`
  - 使用 `gcc -fsyntax-only` 分别验证：
    - `POWER_LIMIT_AGGRESSIVE=0`
    - `POWER_LIMIT_AGGRESSIVE=1`
  - 两个分支均通过

### 未完成

- Keil/ARM 工具链下的整工程双配置编译
- 板端 conservative / aggressive 实机对比

## 结果

- 代码已按文档要求落地。
- 本轮实际代码改动范围控制在 `application/chassis_power_control.h` 和 `application/chassis_task.c`。
- 由于当前机器没有可用的 Keil 或 ARM 编译器，无法完成文档要求的“整工程双配置编译”；已用最小语法探针替代验证条件编译分支本身。

## ⚠ 未验证假设

- 假设：
  - 文档中给出的原始模型系数和两套 buffer PID 就是本分支目标比赛策略。
  - 默认 `POWER_LIMIT_AGGRESSIVE=0` 符合当前固件期望行为。

- 未验证原因：
  - 当前环境没有可用的 Keil 命令行或 ARM 编译器。
  - 宿主 `gcc` 无法直接编译该仓库，因为 FreeRTOS RVDS 移植层和 CMSIS/ARM 专用头文件不兼容宿主工具链。

- 潜在影响：
  - 如果比赛前未在工程里显式固定 `POWER_LIMIT_AGGRESSIVE`，不同机器可能编出不同行为的固件。
  - 若 aggressive 版本未经板端验证，可能导致 buffer 介入点与手感偏离预期。

- 后续验证计划：
  - 在 Keil 中分别编译 `POWER_LIMIT_AGGRESSIVE=0/1`。
  - 板端记录 `buffer/chassis_power/current`，对比两版的 20J 红线保护和动力介入时机。
