# 2026-02-25 pitch absolute mode encode fallback

## 1. 目标

- 基于 MCP 实测结论（AHRS pitch 解算严重失真），将 `GIMBAL_ABSOLUTE_ANGLE` 行为下的 pitch 轴从 `GYRO` 切换为 `ENCONDE`。
- 保持 yaw 轴 `GYRO` 不变。
- 重设 pitch `ENCONDE` 角度环参数以适配连杆机构，速度环参数保持当前稳定版本。

## 2. 改动文件列表

- `application/gimbal_behaviour.c`
- `application/gimbal_task.h`
- `docs/ai_sessions/2026-02-25_pitch_absolute_mode_encode_fallback.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/04_MODULE_MAP.md`
- `docs/INDEX.md`

## 3. 关键改动摘要

- `application/gimbal_behaviour.c`
- 在 `GIMBAL_ABSOLUTE_ANGLE` 分支中：
- yaw 维持 `GIMBAL_MOTOR_GYRO`
- pitch 从 `GIMBAL_MOTOR_GYRO` 改为 `GIMBAL_MOTOR_ENCONDE`

- `application/gimbal_task.h`
- 仅修改 pitch 编码器角度环 PID 宏：
- `PITCH_ENCODE_RELATIVE_PID_KP: 15.0f -> 8.0f`
- `PITCH_ENCODE_RELATIVE_PID_KI: 0.00f -> 0.2f`
- `PITCH_ENCODE_RELATIVE_PID_KD: 0.0f -> 0.1f`
- `PITCH_ENCODE_RELATIVE_PID_MAX_OUT: 10.0f -> 5.0f`
- `PITCH_ENCODE_RELATIVE_PID_MAX_IOUT: 0.0f -> 1.0f`

- 保持不变（按要求）：
- `PITCH_SPEED_PID_*`（`800/10/0/15000/3000`）
- `PITCH_ENCODE_SEN`（`0.01f`）
- yaw 全部参数
- AHRS/INS/`usb_task.c`

## 4. 关键决策与理由

- 决策 1：`GIMBAL_ABSOLUTE_ANGLE` 下 pitch 不再走 `GYRO`，改走 `ENCONDE`。
- 理由：MCP 采集已证明 pitch 绝对角反馈不可用（90° 旋转仅变化约 2.6°），继续使用 GYRO 反馈会导致控制链路失真。

- 决策 2：保留 pitch GYRO 参数，不删除。
- 理由：后续若替换或修复 AHRS 解算，可快速回切验证，不丢历史参数。

### 4.1 改码前三问（对应 handoff 6.1）

1. 当前故障的实际执行路径是什么？
- 口述 + MCP 结论显示故障发生于 pitch 轴 `GIMBAL_MOTOR_GYRO` 反馈链路，`absolute_angle` 对真实姿态变化响应失真。

2. 本次修改命中的是哪条路径？
- 直接命中 `gimbal_behaviour` 的模式映射与 pitch `ENCONDE` 角度环参数，不改 yaw 路径。

3. 如何用数据证明本次修改生效？
- 复测 `pitch_mode`、`pitch_rel/pitch_rel_set`、`pitch_cur`：确认绝对角行为下 pitch 模式切到编码器链路并实现稳定跟踪。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增阻塞。
- CPU 影响：无新增计算路径，仅模式分支与参数常量改变。
- 栈影响：无。
- 帧率/周期：任务周期不变（`gimbal_task` 1ms）。

## 6. 风险点

- 风险 1：`ENCONDE` 链路在连杆机构下虽可控，但相对角参考可能受装配零位和机械间隙影响。
- 风险 2：若后续 AHRS 修复，当前 fallback 语义需明确避免与新链路混用。

## ⚠ 未验证假设

- 假设：当前连杆机构下，pitch 使用 `ENCONDE` 可显著优于失真的 `GYRO` 反馈，并满足实机操控要求。
- 未验证原因：本轮只完成代码切换，尚未完成烧录后的 MCP 闭环验收。
- 潜在影响：若编码器链路参数仍偏激，可能出现响应慢/稳态偏差/限位附近抖动。
- 后续验证计划：烧录后执行 MCP 连续采集（静止、轻推、松手回位、限位附近微调），负责人 `@Codex`。

## 7. 验证方式与结果

- 静态验证：
- 已确认 `GIMBAL_ABSOLUTE_ANGLE` 分支下 pitch 模式为 `GIMBAL_MOTOR_ENCONDE`。
- 已确认 pitch `ENCONDE` PID 宏更新为目标值。
- 已确认 `PITCH_SPEED_PID_*` 与 `PITCH_ENCODE_SEN` 未改。

- 实机验证：
- 本轮未执行，待你烧录后按 MCP 验收。

### 7.1 上轮验收结论（本轮先做）

- 上轮会话：`docs/ai_sessions/2026-02-25_tune_pitch_gyro_mode_diagnosis.md`
- 数据来源：`MCP capture（附 CSV 路径）`
- 关键帧/时间段：`data/tune_2026-02-24_001.csv`（诊断阶段）
- 状态：`Passed`（映射 `Mitigated（待长跑）`，针对“根因识别”）
- 差异：根因已从“PID不稳”切换为“pitch GYRO反馈失真”，本轮改为 ENCONDE fallback
- 下一步：上板后验证控制效果是否满足验收标准

## 8. 回滚方式（如何恢复）

- 行为回滚：`application/gimbal_behaviour.c` 将 pitch 模式切回 `GIMBAL_MOTOR_GYRO`。
- 参数回滚：`application/gimbal_task.h` 恢复 `PITCH_ENCODE_RELATIVE_PID_*` 为旧值（`15/0/0/10/0`）。
- 运行保护：先架空后落地，防止限位冲击。

## 9. 附件/证据

- 诊断会话：`docs/ai_sessions/2026-02-25_tune_pitch_gyro_mode_diagnosis.md`
- 原始采集：`data/tune_2026-02-24_001.csv`
