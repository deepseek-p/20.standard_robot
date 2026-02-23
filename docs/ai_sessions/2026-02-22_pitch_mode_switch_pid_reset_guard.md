# 2026-02-22_pitch_mode_switch_pid_reset_guard

## 1. 目标

- 本次要解决的问题：你反馈“切换到 GPS 就马上往下打到底”，怀疑为 pitch 模式切换瞬间状态残留导致的瞬时冲击。
- 预期可观测结果：
  - `pitch_mode` 从 `2 -> 1` 切换时不再出现瞬时大电流下冲。
  - 无输入状态下（`rc_ch3=0`、`mouse_y=0`）`pitch_cur` 保持低位，`pitch_gyro_set` 贴近 0。

## 2. 改动文件列表

- `application/gimbal_task.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-22_pitch_mode_switch_pid_reset_guard.md`

## 3. 关键改动摘要

- `application/gimbal_task.c`
  - 在 `gimbal_mode_change_control_transit()` 中，pitch 从 `ENCODE -> GYRO` 或 `GYRO -> ENCODE` 切换时：
  - 同步目标值到当前测量值；
  - 将 `motor_gyro_set/current_set/given_current` 归零；
  - 清理对应角度环 PID 与速度环 PID，消除切模瞬间历史状态残留。
  - 在 `gimbal_motor_absolute_angle_control()` 的“无输入+小误差”分支中，同时清理 pitch 绝对角 PID 和速度环 PID，避免仅清速度环时的残余效应。

## 4. 关键决策与理由

- 决策 1：不先改全局 PID 参数，而是先做“切模瞬间状态清理”。
  - 理由：该问题具有明显“切换瞬间触发”特征，优先处理状态机边界条件更直接。
- 决策 2：只对 pitch 切模路径强化，先不扩大到 yaw。
  - 理由：当前故障集中在 pitch 下冲，避免扩大改动面。
- 备选方案与放弃理由：
  - 直接降低 `PITCH_SPEED_PID_KI`：会影响所有场景动态响应，且不能针对“切模瞬态”精准生效。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增 `osDelay`/`vTaskDelay`/等待机制。
- CPU 影响：仅在模式切换分支新增少量赋值与 PID clear，常态路径开销几乎不变。
- 栈影响：无新增大对象，栈变化可忽略。
- 帧率/周期：未修改任务周期与链路时序。

## 6. 风险点

- 风险 1：若根因包含机械耦合/传感器安装偏置，软件切模清理只能部分缓解。
- 风险 2：切模时清空 PID 可能带来极短暂“力矩空窗”，需确认不影响操控体验。

## ⚠ 未验证假设

- 假设：本次“GPS 一切换就下冲”主要由切模瞬间 PID 状态继承触发。
- 未验证原因：当前尚未拿到切模瞬间的新一轮实机对比数据。
- 潜在影响：若该假设不成立，仍可能在强耦合工况出现下冲或缓慢漂移。
- 后续验证计划：复测 ATTI->GPS 切换瞬间并抓取切换前后 1 秒完整帧，重点看 `pitch_mode/pitch_abs_set/pitch_gyro_set/pitch_cur`。

## 7. 验证方式与结果

- 串口：待执行。
- USB：你提供的新日志显示在稳态无输入时 `pitch_gyro_set` 接近 0、`pitch_cur` 低位，表明前一版“无输入防风up”在稳态已生效。
- 示波器：待执行。
- 上位机：待执行（VOFA+ 切模瞬间曲线）。
- 长跑：待执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：撤销 `gimbal_mode_change_control_transit()` 中 pitch 切模 PID 清理分支。
- 参数回滚点：保留前一版 `GIMBAL_PITCH_ABS_NO_CMD_GUARD_ENABLE` 配置不变。
- 运行时保护（例如先发零电流）：回滚后先在零输入下切换模式，确认无瞬时大电流再做动态动作。

## 9. 附件/证据

- 代码差异：`git diff -- application/gimbal_task.c`
- 文档差异：`git diff -- docs/INDEX.md docs/04_MODULE_MAP.md docs/risk_log.md docs/ai_sessions/2026-02-22_pitch_mode_switch_pid_reset_guard.md`
- 相关风险条目：`docs/risk_log.md`
