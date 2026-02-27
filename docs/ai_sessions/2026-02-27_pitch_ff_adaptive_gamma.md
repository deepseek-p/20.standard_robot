# AI 会话记录：Pitch 前馈误差自适应GAMMA（7d）
日期：2026-02-27

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-27_pitch_ff_decouple_telemetry.md`
- 数据来源：口述 + MCP观测（用户描述）
- 关键帧/时间段：
  - 满仓逆重力方向追目标约 `1~1.5s`
  - 顺重力方向约 `0.5s`
  - `pitch_cur` 已可到 `14836`（硬限幅后）
- 状态：`Partial -> In Progress`
- 差异：电流能力已释放，但固定学习率 `GAMMA=0.005` 收敛慢，导致方向不对称仍明显
- 下一步：改为误差自适应GAMMA并复测追踪时间

## 1. 目标

- 将 LMS 学习率从固定值改为随 `|angle_err|` 自适应变化。
- 在大误差工况加快前馈收敛，在小误差工况降低学习激进度。
- 保持硬限幅、PID 参数、遥测输出与 rate limit 机制不变。

## 2. 改动文件列表

- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `docs/02_ARCHITECTURE.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-27_pitch_ff_adaptive_gamma.md`

## 3. 关键改动摘要

- `application/gimbal_task.h`
  - 删除固定学习率宏：
    - `PITCH_FF_GAMMA_K`
    - `PITCH_FF_GAMMA_B`
  - 新增自适应学习率宏：
    - `PITCH_FF_GAMMA_BASE 0.005f`
    - `PITCH_FF_GAMMA_ERR_GAIN 0.5f`
    - `PITCH_FF_GAMMA_MAX 0.05f`
- `application/gimbal_task.c`
  - 在准静态学习块中，`error` 后新增：
    - `angle_err = |relative_angle - relative_angle_set|`
    - `gamma = BASE + ERR_GAIN * angle_err`
    - `gamma` 上限 `GAMMA_MAX`
  - LMS 更新替换为：
    - `dk = gamma * error * cos_theta`
    - `db = gamma * error`
  - `PITCH_FF_ALPHA_DOT_MAX` 限步逻辑保持不变。

## 4. 关键决策与理由

- 决策 1：统一用单一 `gamma` 驱动 `dk/db`。
  理由：保持 `K_hat` 与 `b_hat` 更新节奏一致，降低额外调参维度。
- 决策 2：保留 `ALPHA_DOT_MAX` 限步作为安全网。
  理由：避免大误差下自适应 `gamma` 过快引入单周期跳变。
- 决策 3：不改 PID 与 FF 合成限幅。
  理由：本轮只收敛“学习速度”问题，避免多变量耦合导致结论不清。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增阻塞。
- CPU 影响：每周期新增少量标量计算（绝对值、乘加、限幅），常数级开销。
- 栈影响：新增少量局部 `fp32` 变量（`angle_err`、`gamma`）。
- 帧率/周期：任务周期保持不变（`gimbal_task` 1ms）。

## 6. 风险点

- `GAMMA_ERR_GAIN` 偏大时可能在干扰下加剧参数抖动。
- 若长期大误差且门控持续满足，`gamma` 常驻上限会提高噪声敏感性。
- 仍需和 `ALPHA_DOT_MAX` 联合验证，避免“慢振荡型误学习”。

## ⚠ 未验证假设

- 假设：`gamma` 上限 `0.05` 在满仓逆重力工况可显著缩短收敛时间且不引入明显振荡。
  - 未验证原因：本轮仅完成代码改动，未执行实机采集。
  - 潜在影响：若过激，可能出现 `pitch_cur` 波动增大或到位后抖动。
  - 后续验证计划：采集 `pitch_rel/pitch_rel_set/pitch_cur/ff_k_hat/ff_b_hat`，对比改前改后追踪时间与稳态波动。
- 假设：`angle_err` 作为学习率调度量在当前机构全行程内单调有效。
  - 未验证原因：尚未覆盖全工况（空仓/半仓/满仓、上下限位附近）。
  - 潜在影响：局部工况可能出现学习率过高或过低。
  - 后续验证计划：按空仓→满仓、顺重力→逆重力分别记录收敛速度与稳态误差。

## 7. 验证方式与结果

- 串口：未执行。
- USB/VOFA+：待执行。
- 示波器：未执行。
- 上位机：待执行。
- 长跑：未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 恢复 `PITCH_FF_GAMMA_K/B` 固定学习率宏。
  - 恢复 `dk/db` 到固定学习率公式。
- 参数回滚点：
  - 若出现抖动，优先下调 `PITCH_FF_GAMMA_ERR_GAIN` 或 `PITCH_FF_GAMMA_MAX`。
- 运行时保护：
  - 回滚后先空仓验证，再转满仓验证。

## 9. 附件/证据

- 关联会话：
  - `docs/ai_sessions/2026-02-27_pitch_adaptive_ff_deadlock_fix.md`
  - `docs/ai_sessions/2026-02-27_pitch_ff_decouple_telemetry.md`

## 10. 控制链路三问（规则 6.1）

1. 当前故障的实际执行路径是什么？  
   `gimbal_motor_relative_angle_control()` 的 pitch LMS 学习路径，固定 `GAMMA=0.005` 导致前馈参数收敛慢，满仓逆重力追踪明显慢于顺重力方向。
2. 本次修改命中的是哪条路径？  
   直接命中同一函数同一学习分支，仅替换学习率计算方式（固定 → 误差自适应）。
3. 如何用数据证明本次修改生效？  
   对比改前改后的 `pitch_rel` 追踪时间、`pitch_rel_set` 误差衰减速度，并联看 `ff_k_hat/ff_b_hat` 变化速率与 `pitch_cur` 波动。
