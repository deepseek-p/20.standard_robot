# AI 会话记录：Pitch 自适应前馈死锁修复（7b）

日期：2026-02-27

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-27_pitch_adaptive_gravity_feedforward.md`
- 数据来源：口述验收结果（装弹实测）
- 关键帧/时间段：装弹工况 `speed≈0`、`angle_err≈0.36rad`、电流约 `10330`，pitch 被重力压住无法到位。
- 状态：`Partial -> In Progress`
- 差异：发现“角度误差门限阻断学习”的死锁链路，本轮改为无角误差门限学习。
- 下一步：验证装弹后是否可在几秒内自动恢复到位。

## 1. 目标

- 修复自适应前馈在装弹静态压住场景下无法继续学习的问题。
- 保证“推不动但速度近零”场景可触发 LMS 更新并快速补齐前馈。
- 提升负载突变后的收敛速度。

## 2. 改动文件列表

- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `docs/02_ARCHITECTURE.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-27_pitch_adaptive_ff_deadlock_fix.md`

## 3. 关键改动摘要

- `application/gimbal_task.h`
  - `PITCH_FF_GAMMA_K: 0.0005 -> 0.005`
  - `PITCH_FF_GAMMA_B: 0.0005 -> 0.005`
  - `PITCH_FF_ALPHA_DOT_MAX: 5.0 -> 50.0`
  - 删除 `PITCH_FF_ERR_TH` 宏。
- `application/gimbal_task.c`
  - 在 `gimbal_motor_relative_angle_control()` 的自适应门控中删除 `angle_err` 变量。
  - 学习条件改为：`fabsf(speed) < PITCH_FF_SPEED_TH && fabsf(I_cmd) < PITCH_FF_SAT_TH`。
- docs
  - 同步死锁根因、风险更新与当前状态。

## 4. 关键决策与理由

- 决策 1：移除角度误差门限。  
  理由：被重力压住时速度可接近 0，恰好是可学习状态；若要求小角误差会形成学习死锁。
- 决策 2：学习率与变化率限制同比放大 10x。  
  理由：装弹负载阶跃后需要更快追踪，否则长时间大误差影响可用性。
- 备选方案与放弃理由：
  - 维持原门限仅调大电流环：不能解决“门控不通过”根因；
  - 引入复杂状态机强制学习：当前成本高于直接修正门控条件。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增阻塞调用。
- CPU 影响：条件判断更简化（少一次 `angle_err` 计算和比较）。
- 栈影响：局部变量减少一个 `fp32`。
- 帧率/周期：控制周期仍为 1ms，不影响任务时序。

## 6. 风险点

- 学习率提升后，若场景噪声较大，可能导致参数抖动增大。
- 去掉角误差门限后，慢速动态过程也可能触发学习，需关注误学习。
- 若 `PITCH_TURN` 方向配置错误，收敛会向反方向发散。

## ⚠ 未验证假设

- 假设：在低速条件下，`I_pid` 仍可代表前馈缺口。  
  未验证原因：当前无新一轮板端采样曲线。  
  潜在影响：可能出现慢速动态段误学习。  
  后续验证计划：采集慢推/快推/静止三工况，观察 `pitch_given_current` 与收敛趋势差异。
- 假设：`GAMMA=0.005` 与 `ALPHA_DOT_MAX=50` 不会带来明显震荡。  
  未验证原因：尚未完成长时间稳定性测试。  
  潜在影响：电流纹波和机构振动可能上升。  
  后续验证计划：装弹工况连续 10min 运行，记录电流峰峰值与温升。

## 7. 验证方式与结果

- 串口：未执行。
- USB：待 MCP/VOFA+ 采集。
- 示波器：未执行。
- 上位机：待验证 3 项：
  - 空仓上电收敛；
  - 装弹后短暂偏移后恢复；
  - 摇杆上下拉满到位能力。
- 长跑：未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 恢复 `angle_err` 门限判定；
  - 恢复 `GAMMA_K/B` 和 `ALPHA_DOT_MAX` 到上轮参数。
- 参数回滚点：
  - `PITCH_FF_GAMMA_K/B` 可先降回 `0.0005f` 做保守回退。
- 运行时保护：
  - 回滚参数时先确认电流不过饱和，再执行连续动作测试。

## 9. 附件/证据

- 上轮会话：`docs/ai_sessions/2026-02-27_pitch_adaptive_gravity_feedforward.md`
- 本轮风险条目：`docs/risk_log.md`（2026-02-27 新增 deadlock 条目）

## 10. 控制链路三问（规则 6.1）

1. 当前故障的实际执行路径是什么？  
   - pitch ENCONDE 路径 `gimbal_motor_relative_angle_control()` 中，自适应学习门控要求 `|angle_err| < TH`；装弹静态压住时该条件长期不满足，`K_hat/b_hat` 不更新。
2. 本次修改命中的是哪条路径？  
   - 直接修改同函数同门控分支，删除 `angle_err` 条件并上调学习速度参数。
3. 如何用数据证明本次修改生效？  
   - 观测 `pitch_relative`、`pitch_relative_set`、`pitch_gyro`、`pitch_given_current`。  
   - 预期：装弹卡住场景中前馈参数继续更新，误差从大值逐步回落并恢复可到位。
