# 2026-02-21_pitch_absolute_no_cmd_guard

## 1. 目标

- 本次要解决的问题：针对 `pitch_mode=1`（绝对角模式）下“无输入仍持续加力”的现象，增加防风up保护。
- 预期可观测结果：
  - 在 `rc_ch3=0` 且 `mouse_y=0` 时，`pitch_cur` 不再长期维持高位。
  - `pitch_gyro_set` 在小角误差场景下回落到接近 0，降低往复冲击风险。

## 2. 改动文件列表

- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `docs/INDEX.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/2026-02-21_pitch_absolute_no_cmd_guard.md`

## 3. 关键改动摘要

- `application/gimbal_task.h`
  - 新增宏：
  - `GIMBAL_PITCH_ABS_NO_CMD_GUARD_ENABLE`
  - `GIMBAL_PITCH_ABS_NO_CMD_ERR_LIMIT`
  - `GIMBAL_PITCH_ABS_NO_CMD_MOUSE_DEADBAND`
- `application/gimbal_task.c`
  - `gimbal_motor_absolute_angle_control` 增加 `is_yaw` 参数，调用点区分 yaw/pitch。
  - 新增 `gimbal_pitch_no_cmd_detect()`：判断 pitch 通道与鼠标 y 是否为无输入。
  - 在 pitch 绝对角控制中加入保护：
  - 无输入且 `abs_err` 小于阈值时，置 `motor_gyro_set = 0`；
  - 同时 `PID_clear(&gimbal_motor_gyro_pid)`，防止速度环积分持续累积。

## 4. 关键决策与理由

- 决策 1：先抑制“无输入积分累积”，而不是立即改整套 PID 参数。
  - 理由：你的日志已显示无输入下持续高电流，先做安全止血比全量调参更快更稳。
- 决策 2：仅对 pitch 且仅在小角误差区间生效。
  - 理由：避免影响 yaw 控制，也避免在大偏差需要纠偏时误抑制控制输出。
- 备选方案与放弃理由：
  - 直接全局降 `PITCH_SPEED_PID_KI`：会影响正常控制响应，且无法区分“有输入/无输入”场景。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增 `osDelay`/`vTaskDelay`/忙等。
- CPU 影响：每周期新增少量条件判断与一次可选 `PID_clear`，开销可忽略。
- 栈影响：仅新增少量局部变量。
- 帧率/周期：未修改任务周期与优先级。

## 6. 风险点

- 风险 1：阈值过大可能使小角误差下稳态保持能力下降。
- 风险 2：若主根因为机械耦合/传感映射异常，现象可能仅部分缓解。

## ⚠ 未验证假设

- 假设：
  - 当前主要异常链路是绝对角模式速度环积分累积，而非相对角限幅分支。
  - 在小误差区间置零 `motor_gyro_set` 不会显著破坏正常瞄准体验。
- 未验证原因：
  - 本轮刚完成代码修改，尚未拿到你的新一轮实机数据闭环验证。
- 潜在影响：
  - 若阈值设置不当，可能出现“稳态偏差变大”或“缓慢漂移”。
- 后续验证计划：
  - 复测 GPS 静止/左右摇杆；
  - 重点观察列 `24/33/34/38/39`；
  - 若 `pitch_cur` 仍高位，转入机构耦合与传感映射联合排查。

## 7. 验证方式与结果

- 串口：本轮未新增串口埋点。
- USB：待你回传新一轮 FireWater 数据。
- 示波器：待执行。
- 上位机：待执行 VOFA+ 对比曲线。
- 长跑：待执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 将 `GIMBAL_PITCH_ABS_NO_CMD_GUARD_ENABLE` 置 `0`；
  - 或回退 `gimbal_motor_absolute_angle_control` 新增分支。
- 参数回滚点：
  - 恢复本次新增 3 个宏为旧值/移除。
- 运行时保护：
  - 回滚后先零输入上电，确认无突发大电流再进行动态动作。

## 9. 附件/证据

- 代码差异：`git diff -- application/gimbal_task.h application/gimbal_task.c`
- 文档差异：`git diff -- docs/INDEX.md docs/04_MODULE_MAP.md docs/risk_log.md`
