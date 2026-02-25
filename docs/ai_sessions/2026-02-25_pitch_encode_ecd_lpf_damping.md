# 2026-02-25 pitch encode ecd lpf damping

## 1. 目标

- 在 `pitch ENCONDE` 模式下，对 `ecd差分角速度` 增加一阶低通，降低 1ms 量化阶梯对速度环阻尼的破坏，抑制残余振荡。

## 2. 上轮验收结论（先回填）

- 上轮会话：`docs/ai_sessions/2026-02-25_pitch_encode_speed_feedback_from_encoder.md`
- 数据来源：`口述`
- 关键现象：`ecd差分` 方向正确后可跟踪目标，但速度值在 `0`/`0.767rad/s` 跳变，pitch 仍有约 `±0.15rad` 振荡（周期约 `0.8s`）。
- 状态：`Partial -> In Progress`
- 下一步：在 ENCONDE 速度反馈链路加入低通后复测阻尼与收敛。

## 3. 改动文件列表

- `application/gimbal_task.c`
- `docs/ai_sessions/2026-02-25_pitch_encode_ecd_lpf_damping.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`

## 4. 关键改动摘要

- `application/gimbal_task.c`
- 在 `gimbal_feedback_update()` 的 `pitch ENCONDE` 分支中：
- 保留 `ecd` 差分速度计算；
- 新增一阶低通状态量 `pitch_ecd_speed_filtered`；
- 使用 `alpha=0.05f` 执行 `filtered += alpha * (raw - filtered)`；
- 最终 `motor_gyro` 输出改为滤波后的角速度（并按 `PITCH_TURN` 处理符号）。

## 5. 改码前三问（6.1）

1. 当前故障的实际执行路径是什么？
- 故障在 `pitch_mode=ENCONDE` 的速度反馈链路：1ms 周期下 `ecd` 变化离散，导致 `raw_speed` 量化跳变，速度环阻尼不连续。

2. 本次修改命中的是哪条路径？
- 直接命中 `gimbal_feedback_update()` 的 `ENCONDE -> motor_gyro` 更新分支，不改变其他模式反馈。

3. 如何用数据证明本次修改生效？
- 观察 `pitch_gyro/pitch_gyro_set/pitch_given_current/pitch_relative`：
- `pitch_gyro` 高频阶梯应显著减小；
- `pitch_given_current` 反复反向冲击减少；
- `pitch_relative` 振荡幅值和收敛时间下降。

## 6. 关键决策与理由

- 决策：采用一阶低通（`alpha=0.05`）而不是继续提高速度环阻尼参数。
- 理由：当前主矛盾是反馈量量化阶梯，不先平滑反馈，单纯改 PID 会被离散噪声驱动，收益有限且更容易引入副作用。

## 7. 实时性影响

- 新增常量与若干标量运算，无阻塞调用。
- CPU 与栈增量可忽略，任务周期保持 1ms。

## 8. 风险点

- `alpha` 过小会带来相位滞后，可能在快速动作下跟随变慢。
- 该滤波状态为 `static`，切模态瞬间可能保留历史值，需要实测确认切模态过渡是否平顺。

## ⚠ 未验证假设

- 假设：`alpha=0.05` 在当前机构与采样周期下能同时兼顾抑振和响应速度。
- 未验证原因：本轮仅完成代码修改，尚未烧录实机闭环测试。
- 潜在影响：若滤波过重，会出现轻推迟滞或回位慢；若滤波不足，振荡仍残留。
- 后续验证计划：实机采集静止/轻推/快推三场景，优先比对 `pitch_gyro` 平滑度与 `pitch_given_current` 峰值，负责人 `@Codex`。

## 9. 验证方式

- 静态验证：确认仅修改 `gimbal_feedback_update()` 中目标分支，`else` 的 IMU 路径保持不变。
- 待实机验证：
- 场景1 静止 5s：确认 `pitch_given_current` 波动下降；
- 场景2 轻推后松手：确认振荡幅值小于上轮；
- 场景3 快推阶跃：确认不过冲放大、可回位。

## 10. 回滚方式

- 将 `application/gimbal_task.c` 中该分支恢复为不带低通的 `ecd差分` 版本（上轮基线）。
