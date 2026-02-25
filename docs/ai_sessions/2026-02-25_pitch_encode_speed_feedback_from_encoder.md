# 2026-02-25 pitch encode speed feedback from encoder

## 1. 目标

- 在 pitch 轴 `GIMBAL_MOTOR_ENCONDE` 模式下，将速度环反馈从 IMU pitch gyro 切换为电机编码器转速换算角速度。
- 调整 pitch ENCONDE 角度环参数，避免连杆机构下 ENCONDE 外环过激。

## 2. 上轮验收结论（先回填）

- 上轮会话：`docs/ai_sessions/2026-02-25_tune_pitch_gyro_mode_diagnosis.md`
- 数据来源：`MCP capture（data/tune_2026-02-24_001.csv）`
- 关键结论：AHRS pitch 解算失真，pitch 轴 GYRO 反馈不可用于闭环；yaw GYRO 正常。
- 状态映射：`Partial -> In Progress`
- 下一步：改为 pitch ENCONDE 链路后，继续验证速度反馈一致性与电流稳定性。

## 3. 改动文件列表

- `application/gimbal_task.c`
- `application/gimbal_task.h`
- `docs/ai_sessions/2026-02-25_pitch_encode_speed_feedback_from_encoder.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`

## 4. 关键改动摘要

- `application/gimbal_task.c`
- `gimbal_feedback_update()` 中 pitch `motor_gyro` 更新改为：
- 当 `gimbal_pitch_motor.gimbal_motor_mode == GIMBAL_MOTOR_ENCONDE` 时，使用 `speed_rpm * 0.104720f`（按 `PITCH_TURN` 修正符号）；
- 其他模式仍使用 IMU `INS_GYRO_Y_ADDRESS_OFFSET`。

- `application/gimbal_task.h`
- `PITCH_ENCODE_RELATIVE_PID_KP: 8.0f -> 6.0f`
- `PITCH_ENCODE_RELATIVE_PID_KI: 0.2f -> 0.1f`
- `PITCH_ENCODE_RELATIVE_PID_KD: 0.1f -> 0.0f`
- `PITCH_ENCODE_RELATIVE_PID_MAX_OUT: 5.0f -> 10.0f`
- `PITCH_ENCODE_RELATIVE_PID_MAX_IOUT: 1.0f -> 2.0f`
- 保持不变：`PITCH_SPEED_PID_* = 800/10/0/15000/3000`

## 5. 改码前三问（6.1）

1. 当前故障的实际执行路径是什么？
- pitch 在 ENCONDE 模式下，速度环反馈仍来自 IMU gyro，反馈源与执行器（电机）不一致，连杆工况下易引入额外扰动。

2. 本次修改命中的是哪条路径？
- 直接命中 `gimbal_feedback_update()` 的 pitch 速度反馈路径，仅在 ENCONDE 模式切到编码器速度反馈。

3. 如何用数据证明本次修改生效？
- 观察 `pitch_mode/pitch_gyro/pitch_gyro_set/pitch_given_current/pitch_relative/pitch_relative_set`：
- ENCONDE 模式下 `pitch_gyro` 应与电机转速变化同相，`pitch_given_current` 峰值与波动下降，`pitch_relative` 收敛更平滑。

## 6. 关键决策与理由

- 决策：ENCONDE 模式速度环反馈使用编码器速度，而非 IMU gyro。
- 理由：当前已确认 AHRS pitch 失真，且 ENCONDE 链路本质是“电机相对角+电机速度”闭环，反馈源一致性更高。

## 7. 实时性影响

- 无新增任务、无阻塞调用。
- 新增分支与一次 `speed_rpm` 到 rad/s 换算，CPU 开销可忽略。
- 任务周期与帧率不变。

## 8. 风险点

- 编码器速度噪声可能增大速度环抖动，需靠后续参数和限幅验证。
- 若 `PITCH_TURN` 符号与机械方向不一致，可能出现反向阻尼。

## ⚠ 未验证假设

- 假设：ENCONDE 模式下编码器速度反馈比 IMU pitch gyro 反馈更稳定、更一致。
- 未验证原因：本轮仅完成代码切换，尚未完成烧录后 MCP 实机闭环验证。
- 潜在影响：若符号或量纲不匹配，可能导致 pitch 轴抖动或响应变慢。
- 验证计划：烧录后进行静止/轻推/快推三场景 MCP 采集，负责人 `@Codex`。

## 9. 验证方式

- 本轮为静态代码验证：编译前检查宏与反馈路径均已命中指定位置。
- 待实机：用 MCP/VOFA+ 采集 `pitch_mode(ENCONDE)`、`pitch_gyro`、`pitch_given_current`、`pitch_relative` 收敛曲线。

## 10. 回滚方式

- 回滚 `application/gimbal_task.c` 中 ENCONDE 分支，恢复 pitch `motor_gyro` 始终读取 IMU。
- 回滚 `application/gimbal_task.h` 的 `PITCH_ENCODE_RELATIVE_PID_*` 到前一组参数。
