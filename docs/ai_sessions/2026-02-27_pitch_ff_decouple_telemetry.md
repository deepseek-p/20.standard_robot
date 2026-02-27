# AI 会话记录：Pitch 前馈解耦 + 遥测观测通道（7c）
日期：2026-02-27

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-27_pitch_adaptive_ff_deadlock_fix.md`
- 数据来源：口述验收结果（本轮用户提供的满仓下拉迟钝关键帧）
- 关键帧/时间段：`pitch_rel=0.11`、`pitch_rel_set=0.19`、`pitch_cur≈10721`，稳态误差约 `0.08 rad`
- 状态：`Partial -> In Progress`
- 差异：自适应前馈在静态保持可用，但满仓逆重力方向动态到位能力不足；本轮引入总电流硬限幅框架与前馈参数遥测可视化
- 下一步：实机验证 FF 收敛速度与满仓上下拉满到位能力

## 1. 目标

- 在 pitch ENCONDE 控制链路中明确“PID输出 + 前馈输出”的合成与硬限幅边界。
- 将自适应参数 `K_hat/b_hat` 暴露给 USB FireWater，支撑在线收敛观察。
- 保持现有 PID 参数与 LMS 更新逻辑不变，仅做前馈合成与观测增强。

## 2. 改动文件列表

- `application/gimbal_task.c`
- `application/gimbal_task.h`
- `application/usb_task.c`
- `docs/02_ARCHITECTURE.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-27_pitch_ff_decouple_telemetry.md`

## 3. 关键改动摘要

- `application/gimbal_task.c`
  - 新增文件级状态：`pitch_ff_K_hat`、`pitch_ff_b_hat`。
  - 新增 getter：`get_pitch_ff_K_hat()`、`get_pitch_ff_b_hat()`。
  - 在 `gimbal_motor_relative_angle_control()` 的 pitch 分支中：
    - 合成 `I_total = I_pid ± I_ff`
    - 对 `I_total` 做 `[-16000, 16000]` 硬限幅后再写 `current_set`
    - LMS 更新改为直接读写文件级 `pitch_ff_K_hat/pitch_ff_b_hat`
- `application/gimbal_task.h`
  - 增加 FF getter 声明，供跨模块（USB 遥测）调用。
- `application/usb_task.c`
  - FireWater 字段注释新增：
    - `55:ff_k_hat`（milli-scale）
    - `56:ff_b_hat`（raw）
  - `usb_emit_firewater_frame()` 追加 2 个输出字段：
    - `usb_debug_fp32_to_milli(get_pitch_ff_K_hat())`
    - `(int32_t)(get_pitch_ff_b_hat())`

## 4. 关键决策与理由

- 决策 1：总电流硬限幅放在 FF+PID 合成后。
  理由：保护边界应作用于最终下发电流，不改变各子环算法结构。
- 决策 2：`K_hat/b_hat` 采用 getter 暴露，而非直接 extern 变量。
  理由：保持模块边界，避免后续修改存储位置时影响调用方。
- 决策 3：不改 LMS 超参与 PID 参数。
  理由：本轮目标是“解耦与可观测性增强”，避免引入多变量混叠。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增阻塞调用。
- CPU 影响：新增常数级计算（限幅判断 + 2 个 getter 调用），可忽略。
- 栈影响：无新增大对象，仅少量局部标量。
- 帧率/周期：`gimbal_task` 1ms、`usb_task` 周期不变。

## 6. 风险点

- 硬限幅后若长期贴近 `±16000`，可能掩盖“参数未收敛”与“环路增益不足”的边界问题。
- 新增遥测字段后，上位机解析脚本若未同步字段数，可能出现列错位。
- `b_hat` 输出为 raw 整数，若后续上位机统一按 milli 处理会导致误读。

## ⚠ 未验证假设

- 假设：`I_total` 硬限幅不会在当前参数下频繁触发并造成动态迟滞。
  - 未验证原因：本轮未执行实机/VOFA+ 采集。
  - 潜在影响：若频繁触发，可能导致动作段响应“顶住不走”。
  - 后续验证计划：采集 `pitch_cur` 与新增 `ff_k_hat/ff_b_hat`，统计限幅触发占比。
- 假设：`ff_k_hat`（milli）与 `ff_b_hat`（raw）的混合尺度不会被上位机误解析。
  - 未验证原因：`frame_parser.py` 本轮按需求未改。
  - 潜在影响：联调时错误判断前馈收敛方向和幅值。
  - 后续验证计划：MCP 端完成 parser 更新后做字段对齐验收。

## 7. 验证方式与结果

- 串口：未执行。
- USB：已完成代码侧拼帧字段追加；未执行板端收包验收。
- 示波器：未执行。
- 上位机：未执行。
- 长跑：未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 回退 `gimbal_motor_relative_angle_control()` 中 `I_total` 硬限幅段。
  - 回退 `pitch_ff_K_hat/pitch_ff_b_hat` 文件级状态与 getter。
  - 回退 `usb_task.c` 新增的 55/56 字段。
- 参数回滚点：
  - 不涉及 PID/学习率参数改动，无额外参数回滚。
- 运行时保护：
  - 回滚后先在空仓工况验证 `pitch_cur` 与到位误差，再进行装弹测试。

## 9. 附件/证据

- 用户问题描述（满仓逆重力方向迟钝）作为本轮触发依据。
- 关联会话：
  - `docs/ai_sessions/2026-02-27_pitch_adaptive_gravity_feedforward.md`
  - `docs/ai_sessions/2026-02-27_pitch_adaptive_ff_deadlock_fix.md`

## 10. 控制链路三问（规则 6.1）

1. 当前故障的实际执行路径是什么？  
   `gimbal_motor_relative_angle_control()` 的 pitch ENCONDE 路径，`I_pid` 与 `I_ff` 合成后电流不足以快速克服满仓逆重力方向负载，出现稳态残余误差。
2. 本次修改命中的是哪条路径？  
   直接命中同一函数同一分支：在总电流合成点增加硬限幅边界，并导出 FF 参数到遥测链路便于在线诊断。
3. 如何用数据证明本次修改生效？  
   观察 `pitch_rel/pitch_rel_set/pitch_cur` 与新增 `ff_k_hat/ff_b_hat`：预期满仓下拉阶段误差收敛更快，且可看到 FF 参数变化与电流变化的对应关系。
