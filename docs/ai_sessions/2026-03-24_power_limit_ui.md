# 2026-03-24 功率限制与裁判客户端 UI

## 1. 目标

- 将 `main` 里的底盘功率预测限流思路移植到当前分支。
- 范围只保留“功率预测模型 + 功率限制”本体，不移植超级电容相关逻辑。
- 新增裁判客户端 UI，显示：
  - 底盘模式：`FOLLOW / SPIN`
  - 发射挡位：`LOW / HIGH`
  - 退弹成功绿灯闪烁
  - 位于屏幕高度约 `44%` 的固定准心

## 2. 改动文件列表

- `application/chassis_power_control.c`
- `application/chassis_power_control.h`
- `application/shoot.c`
- `application/shoot.h`
- `application/rm_ui.c`
- `application/rm_ui.h`
- `application/referee_usart_task.c`
- `MDK-ARM/standard_robot.uvprojx`
- `docs/INDEX.md`
- `docs/02_ARCHITECTURE.md`
- `docs/03_TASK_MAP.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`

## 3. 关键改动摘要

- `chassis_power_control`
  - 用模型功率估算替代旧的总电流直接缩放。
  - 保留裁判 `power/buffer` 读数、离线 fallback 和低 buffer 保护。
  - 不引入 supercap 读写、状态机或额外任务耦合。
- `shoot`
  - 增加 `shoot_get_ui_gear()`，只读导出 `LOW/HIGH`。
  - 增加 `shoot_consume_reverse_success_pulse()`，把退弹完成做成一次性 UI 事件。
- `rm_ui`
  - 新增客户端 UI 发送模块，负责 `0x0301` 组帧、CRC、初始化重建和低频刷新。
  - 初次在线时依次发送：delete all、reticle、mode text、gear text。
  - 退弹成功后闪烁绿色指示约 `800ms`。
- `referee_usart_task`
  - 不改接收解包职责，只在 10ms 循环里额外挂 `rm_ui_update()`。

## 4. 关键决策与理由

- 不新建 UI task。
  - 直接复用现有 `REFEREE` 任务，避免引入新的串口发送调度点。
- 不移植 supercap。
  - 用户明确限定只要功率预测模型和限流本体。
- UI 只做低频刷新。
  - 模式/挡位变化频率很低，退弹闪烁也不需要高频重绘；限到 `100ms` 可以减少阻塞发送对裁判接收的影响。
- 底盘模式折叠为两态。
  - 需求只保留 `FOLLOW/SPIN`，因此通过 `chassis_mode + wz_set` 做最小判定，不扩展更多 UI 状态。

## 5. 实时性影响

- 阻塞变化：
  - 新增 `HAL_UART_Transmit(&huart6, ...)` 阻塞发送路径。
  - 发送频率由 `rm_ui.c` 内部限到 `100ms`，每次最多发一个 UI 包。
- CPU 影响：
  - 新增少量协议组帧、字符串拷贝和状态机判断。
  - 未侵入 1ms `gimbal_task` 或 2ms `chassis_task` 主控制路径。
- 栈影响：
  - `rm_ui_send_interactive()` 使用本地 `frame[128]` 缓冲，实际栈占用上升发生在 `REFEREE` 任务。
  - 当前未做板端 stack watermark 复测。
- 周期影响：
  - 无任务周期、优先级、创建数量变化。

## 6. 风险点

- 裁判 UI 发送仍是阻塞 TX，若实机链路窗口紧张，可能影响 `REFEREE` 任务单次循环时间。
- 功率预测模型常数来自参考实现，尚未按本车实测重新标定。
- 客户端颜色枚举与图元表现目前按官方串口协议实现，仍需板端确认显示风格与坐标是否满足现场预期。

## ⚠ 未验证假设

- 假设：当前协议颜色枚举和图元打包方式与裁判系统实机版本兼容。
- 未验证原因：本次会话没有接入裁判系统客户端做实时绘制确认。
- 潜在影响：可能出现颜色不符、图元不显示或重建顺序异常。
- 后续验证计划：
  - 在板端联机观察初次上线、掉线重连和退弹闪烁三种场景。
  - 若出现异常，优先核对 `0x0100/0x0101/0x0103/0x0110` 的实机兼容性与颜色枚举。

## 7. 验证方式与结果

- 主机语法检查：
  - `gcc -fsyntax-only ... application/rm_ui.c`
  - 结果：通过。
- 主机整仓编译替代检查：
  - `gcc -fsyntax-only ... application/referee_usart_task.c`
  - 结果：失败，但失败点位于仓库现有 FreeRTOS ARM/RVDS 移植层和宿主机工具链不兼容，不是本次 UI 改动新引入的语法错误。
- 板端验证：
  - 尚未执行。

## 8. 回滚方式

- 代码回滚点：
  - 删除 `application/rm_ui.c/h`
  - 去掉 `application/referee_usart_task.c` 中的 `rm_ui_init()/rm_ui_update()`
  - 恢复 `MDK-ARM/standard_robot.uvprojx` 的文件列表
- 功率限流回滚点：
  - 恢复 `application/chassis_power_control.c/h` 到旧的电流缩放版本
- 运行时保护：
  - 若 UI 异常影响裁判链路，优先临时注释 `rm_ui_update()`，不动接收解包主链。

## 9. 附件/证据

- 设计文档：`docs/plans/2026-03-24-power-limit-ui-design.md`
- 实施计划：`docs/plans/2026-03-24-power-limit-ui-implementation.md`
- 风险日志：`docs/risk_log.md`
- 官方协议参考：
  - `RoboMaster 裁判系统串口协议附录 V1.5（2023-07-07）`

## v2: 功率上限按 main 策略回调到 120W

### 改动摘要

- `application/chassis_power_control.c/h`
  - 去掉此前固定 `100W` 和“裁判离线时总电流硬限幅”的临时策略。
  - 改为读取 `get_chassis_power_limit()`，当前按 `main` 返回 `120W`。
  - 有效功率上限改为：
    `effective_limit = power_limit - PID(buffer, 50J setpoint)`，
    并夹紧到 `[20W, 120W]`。
  - 保留功率模型估算 + 二次方程反解电流。
  - 当 `buffer < 20J` 时，不直接清零，改为按 `buffer / 20` 线性缩放四轮输出。
- `application/chassis_task.c/h`
  - 新增 `buffer_pid` 状态。
  - 在 `chassis_init()` 中按 `main` 参数初始化：
    `Kp=2.0, Ki=0.1, Kd=0.0, max_out=40, max_iout=20`。
- `application/referee.c/h`
  - 新增 `get_chassis_power_limit()`，当前返回 `120u`。

### 关键决策与理由

- 本轮向 `main` 对齐的是“120W 上限 + buffer PID 保 buffer + 20J 红线保护”这套策略，不再保留我前一版 `100W` 的保守上限。
- 仍然不移植 `supercap` 分支，因为本分支范围被限定为“功率预测模型 + 功率限制”本体。
- `buffer < 20J` 采用线性缩放而不是直接断底盘，原因是 `main` 也是用应急降额而不是硬清零；这样更接近比赛实际控制手感，同时仍守住 20J 红线。

## ⚠ 未验证假设

- 假设内容：
  - 当前分支直接返回 `120W` 与本车当前组别/裁判配置一致。
  - `main` 中的 `buffer_pid` 参数 `{2.0, 0.1, 0.0}` 迁移到本车后不会导致限功率介入过早或过晚。
- 当前无法验证原因：
  - 本次仅完成代码级对齐，尚未做 Keil 全工程编译和板端带裁判系统实测。
- 潜在影响：
  - 若实际裁判功率配置不是 `120W`，会导致限功率点偏差。
  - 若 buffer PID 过强，可能在 `50J` 附近提前压动力；过弱则可能守不住 `20J` 红线。
- 后续验证计划：
  - 板端记录 `chassis_power / chassis_power_buffer / wheel current`，覆盖急加速、撞停、连续机动三种工况。
  - 观察 `buffer` 是否稳定守在 `20J` 以上，以及 `effective_limit` 介入时机是否符合预期。

## v3: 功率模型系数按用户确认值覆盖

### 改动摘要

- `application/chassis_power_control.h`
  - 功率预测模型参数改为：
    - `MOTOR_TORQUE_COEFF = 2.16e-6f`
    - `MOTOR_K2 = 2.09e-7f`
    - `MOTOR_A_COEFF = 1.83e-7f`
    - `MOTOR_CONST_TERM = 2.21f`

### 关键决策与理由

- 我检查了当前工作区可见的本地 `main`，其中仍是旧系数；这说明本地 `main` 与你所说的“最新 main”不一致。
- 本轮以你提供的 4 个参数为准覆盖当前分支，因为这比本地旧分支值更符合你的目标来源。

## ⚠ 未验证假设

- 假设内容：
  - 你提供的 4 个模型参数对应的就是目标 `main` 最新版本，而不是另一条实验分支。
- 当前无法验证原因：
  - 当前仓库本地 `main` 并未包含这组新参数，缺少可直接对照的本地提交证据。
- 潜在影响：
  - 若该组参数来源不是最终目标分支，预测功率会与主线期望不一致。
- 后续验证计划：
  - 板端对比新旧参数下的 `power/buffer/current` 轨迹，确认这组系数的介入点和动力手感。

## v4: 补遥测字段与 back-solve 防御

### 改动摘要

- `application/chassis_task.h`
  - 在 `chassis_move_t` 末尾新增：
    - `power_est`
    - `effective_limit`
- `application/chassis_power_control.c`
  - 在限功率判定前写入：
    - `chassis_power_control->power_est = total_power;`
    - `chassis_power_control->effective_limit = effective_limit;`
  - 在 quadratic back-solve 循环中补上：
    - `if (target_power < 0.0f) continue;`
- `application/chassis_power_control.h`
  - 宏名对齐 `main`：
    - `MOTOR_A_COEFF -> MOTOR_A`
    - `MOTOR_CONST_TERM -> MOTOR_CONSTANT`
  - 系数值保持不变：
    - `2.16e-6 / 2.09e-7 / 1.83e-7 / 2.21`

### 关键决策与理由

- 本轮只做你指定的 3 件事，不调整 PID、不动 buffer 阈值、不引入 `supercap`。
- `power_est/effective_limit` 放进 `chassis_move_t`，是为了直接复用现有底盘调试快照对象，后续加 USB 遥测最省改动面。
- `target_power < 0` 的防御按 `main` 补齐，避免在极端比例缩放下把不该反解的目标功率继续送进二次方程。

## ⚠ 未验证假设

- 假设内容：
  - 现有 USB/调试链路后续会消费 `chassis_move_t.power_est` 与 `effective_limit`，且不会因为结构体扩展引入额外兼容问题。
- 当前无法验证原因：
  - 本轮只做了结构和限流路径补丁，没有把两个新字段真正接到 USB 输出，也没有做板端采样。
- 潜在影响：
  - 字段虽已写入，但如果后续遥测侧没有读取，就只能停留在内部观测量。
  - 若二次方程极端工况此前依赖负值穿透逻辑，本轮行为会更保守。
- 后续验证计划：
  - 下一轮把 `power_est/effective_limit` 接到 USB 遥测，并在急加速和低 buffer 场景下看曲线是否连续、是否与限功率介入点一致。
