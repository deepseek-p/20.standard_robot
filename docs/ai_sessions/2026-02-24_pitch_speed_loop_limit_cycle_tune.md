# 2026-02-24 pitch speed loop limit cycle tune

## 1. 目标

- 本次要解决的问题：在 Pitch 已切到 GYRO 后，速度环仍出现静止大幅正负摆动（limit cycle）。
- 预期可观测结果：静止电流振荡显著下降，慢推/快推回弹电流峰值下降，同时保持角度跟踪精度。

## 2. 改动文件列表

- `application/gimbal_task.h`
- `docs/ai_sessions/2026-02-24_pitch_speed_loop_limit_cycle_tune.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`

## 3. 关键改动摘要

- `application/gimbal_task.h`
- 仅调整 Pitch 速度环 5 个宏（不改角度环、不改 Yaw、不改控制流）：
- `PITCH_SPEED_PID_KP: 1500.0f -> 800.0f`
- `PITCH_SPEED_PID_KI: 30.0f -> 10.0f`
- `PITCH_SPEED_PID_KD: 0.0f -> 0.0f`（保持）
- `PITCH_SPEED_PID_MAX_OUT: 20000.0f -> 15000.0f`
- `PITCH_SPEED_PID_MAX_IOUT: 5000.0f -> 3000.0f`

## 4. 关键决策与理由

- 决策 1：本轮仅改速度环，不触碰角度环和 RC 灵敏度。
- 理由：上轮数据已证明角度环跟踪误差 < 1°，主问题定位在速度环振荡。
- 决策 2：Kp/ Ki 与输出上限同步下调。
- 理由：抑制静止段正负电流翻转，避免积分在振荡环境中放大。

### 4.1 改码前三问（对应 `docs/00_AI_HANDOFF_RULES.md` 6.1）

1. 当前故障的实际执行路径是什么？  
   `data/pitch_data.md`（“陀螺仪模式第一版”后，约 lines 528-799）显示无输入时 `col39` 在 `+5000` 与 `-6600` 周期翻转，命中 Pitch 速度环参数路径。
2. 本次修改命中的是哪条路径？  
   仅命中 `gimbal_task.h` 的 `PITCH_SPEED_PID_*` 宏，不改模式切换与控制函数路径。
3. 如何用数据证明本次修改生效？  
   复测静止/慢推/快推：重点看 `col39` 波动幅度、慢推回弹峰值、`pitch_absolute` 稳态误差。

## 5. 实时性影响（必须写）

- 阻塞变化：无。
- CPU 影响：无新增计算。
- 栈影响：无。
- 帧率/周期：无任务周期变化。

## 6. 风险点

- 风险 1：Kp 降低后可能出现“稳但慢”，回位速度不足。
- 风险 2：若连杆摩擦区间强非线性，仍可能在特定角度出现局部振荡。

## ⚠ 未验证假设

- 假设：速度环降增益后，静止段极限环可被显著压制且不牺牲太多响应速度。
- 未验证原因：虽已回填第二版 VOFA+，但缺长跑与全姿态覆盖。
- 潜在影响：若假设不成立，可能出现“振荡改善但响应过慢”或“仍有局部翻转”。
- 后续验证计划：按静止、慢推、快推三段回传 `col24/33/34/37/39`，负责人 `@Codex`。

## 7. 验证方式与结果

- 串口：已回填 `data/pitch_data.md` 第 1136-1492 行（“第二版”）。
- USB：已有 VOFA+ 帧，覆盖静止/慢推/快推。
- 示波器：未执行。
- 上位机：已完成二调数据初算，振荡幅度明显下降但未完全达标。
- 长跑：未执行。

### 7.1 上轮验收结论回填（本轮先做）

- 上轮会话：`docs/ai_sessions/2026-02-24_pitch_linkage_gyro_mode_pid_tune.md`
- 数据来源：`data/pitch_data.md`（“陀螺仪模式第一版”后数据段）
- 关键帧/时间段：
- `pitch_mode=1`（GYRO）确认切模成功。
- 静止稳态误差由旧 ENCONDE 约 `5.3°` 降至约 `0.34°`。
- 第二次静止段 `col39` 仍有 `+5000 ~ -6600` 周期振荡。
- 状态：`Partial`（映射为 `In Progress`）
- 差异：角度环效果明显改善，但速度环存在极限环振荡。
- 下一步：执行本轮速度环降增益参数并采集改后对比数据。

### 7.2 本轮验收状态

- 状态：`Partial`（映射 `In Progress`）
- 目标标准：
- 静止 `col39` 波动 < `±500`
- 慢推回弹峰值 < `±4000`
- 稳态角误差 < `1°`
- 实测（`data/pitch_data.md` 第二版，按当前列定义对应 `pitch_given_current`）：
- 静止段电流约 `3824~3836`，波动约 `12`（已明显改善）。
- 慢推段 `|pitch_current|` 峰值约 `4413`（略高于目标 `4000`）。
- 快推段 `|pitch_current|` 峰值约 `4967`（未达目标）。
- 结论：本轮已脱离“无数据”状态，当前是“改善明显但未完全收敛”，需继续微调。

## 8. 回滚方式（如何恢复）

- 代码回滚点：`application/gimbal_task.h` 的 5 个 `PITCH_SPEED_PID_*` 宏恢复到上一版（1500/30/0/20000/5000）。
- 参数回滚点：保持 Pitch 角度环与 `PITCH_RC_SEN` 不变，仅回滚速度环。
- 运行时保护：先架空后落地，避免直接机械冲击。

## 9. 附件/证据

- 数据：`data/pitch_data.md`
- 上轮会话：`docs/ai_sessions/2026-02-24_pitch_linkage_gyro_mode_pid_tune.md`
- 相关风险条目：`docs/risk_log.md`（2026-02-24 `gimbal_task`）
