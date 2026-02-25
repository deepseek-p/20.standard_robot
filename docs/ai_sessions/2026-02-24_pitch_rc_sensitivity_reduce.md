# 2026-02-24 pitch rc sensitivity reduce

## 1. 目标

- 解决 Pitch 连杆机构下“轻推遥控器就打到物理限位”的操控问题。
- 仅降低遥控器到 Pitch 目标角增量的灵敏度，不改 PID 与控制流程。

## 2. 改动文件列表

- `application/gimbal_task.h`
- `docs/ai_sessions/2026-02-24_pitch_rc_sensitivity_reduce.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`

## 3. 关键改动摘要

- `application/gimbal_task.h`
- 仅修改 1 处宏：
- `PITCH_RC_SEN: -0.000004f -> -0.000001f`
- 未修改任何 `.c` 文件，未修改任何 PID 参数宏。

## 4. 关键决策与理由

- 决策：本轮只降 `PITCH_RC_SEN`，不动 PID。
- 理由：上轮验收已确认 GYRO 模式下速度环/角度环稳定，当前问题是“输入增量过大导致快速顶限位”，属于输入映射问题而非闭环稳定性问题。

### 4.1 改码前三问（对应 handoff 6.1）

1. 当前故障执行路径是什么？
- `gimbal_behaviour` 的遥控通道通过 `PITCH_RC_SEN` 累加到 Pitch 设定值，在 1ms 周期下累计过快，导致目标角迅速逼近/超过连杆可用行程。

2. 本次修改命中哪条路径？
- 直接命中输入映射增量宏 `PITCH_RC_SEN`，不改变任何闭环控制路径。

3. 如何用数据证明生效？
- 对比 `col43(rc_ch3)`、`col34(pitch_abs_set)`、`col33(pitch_abs)`：同等摇杆推量下，`pitch_abs_set` 的斜率应下降到原来的约 1/4，且不再快速顶限位。

## 5. 实时性影响（必须写）

- 阻塞变化：无。
- CPU 影响：无。
- 栈影响：无。
- 帧率/周期：无改动。

## 6. 风险点

- 风险 1：灵敏度降低后可能出现“手感偏慢”，快速瞄准效率下降。
- 风险 2：不同连杆几何/摩擦工况下最佳值可能不同，仍需实机复测确认。

## ⚠ 未验证假设

- 假设：`PITCH_RC_SEN=-0.000001f` 可在当前连杆机构上兼顾“不过冲限位”和“可用响应速度”。
- 未验证原因：本轮仅完成参数落地，未获取新 VOFA+ 与实机口述验收。
- 潜在影响：若仍偏快会继续触发限位；若偏慢会影响跟手性。
- 后续验证计划：采集 MID/UP 下慢推、快推、微调三组数据，重点看 `col43/col34/col33/col39` 与是否触限；负责人 `@Codex`。

## 7. 验证方式与结果

- 静态验证：
- 已确认 `application/gimbal_task.h` 中 `PITCH_RC_SEN` 仅此一处变更。
- 已确认未修改任何 `.c` 文件。

- 实机验证：
- 本轮未执行，状态为待验收。

### 7.1 上轮验收结论（本轮先做）

- 上轮会话：`docs/ai_sessions/2026-02-24_pitch_speed_loop_limit_cycle_tune.md`
- 数据来源：用户本轮口述验收结果
- 关键帧/时间段：第二版 GYRO PID 验收
- 状态：`Passed`（映射 `Mitigated（待长跑）`）
- 差异：PID 稳定性问题已基本解决；当前新增问题为遥控输入灵敏度过高导致限位触发
- 下一步：仅调整 `PITCH_RC_SEN` 并复测输入-设定斜率与触限行为

## 8. 回滚方式（如何恢复）

- 回滚点：将 `application/gimbal_task.h` 中 `PITCH_RC_SEN` 改回 `-0.000004f`。
- 运行时保护：先架空再落地复测，避免直接上负载触发机械冲击。

## 9. 附件/证据

- 变更文件：`application/gimbal_task.h`
- 相关历史：`docs/ai_sessions/2026-02-24_pitch_speed_loop_limit_cycle_tune.md`
- 风险联动：`docs/risk_log.md`
