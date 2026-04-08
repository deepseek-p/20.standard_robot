# AI 会话记录：Pitch 自适应重力前馈（替换固定K方案）

日期：2026-02-27

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-27_pitch_gravity_feedforward_bullet_count.md`
- 数据来源：口述 + MCP 历史采集结论（`docs/ai_sessions/2026-02-27_tune_pitch_encode.md`）
- 关键帧/时间段：用户反馈固定 `K*cos(theta)` 在 22° 工作区间无法覆盖 13000+ 电流差异，且装填量不固定导致固定K与计数方案泛化不足。
- 状态：`Failed -> Open`
- 差异：本轮从“固定K+计数衰减”切换为“在线自适应估计 `K_hat/b_hat`”。
- 下一步：实机验证收敛速度、稳态误差与装填变化下的跟踪能力。

## 1. 目标

- 将 pitch ENCONDE 前馈从固定系数替换为自适应 LMS。
- 在准静态区间以速度环残余输出 `I_pid` 作为学习误差，在线更新 `K_hat` 与 `b_hat`。
- 保持原级联 PID 主结构和已验证的 `PITCH_ENCODE_RELATIVE_PID` 参数不变。

## 2. 改动文件列表

- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `docs/02_ARCHITECTURE.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-27_pitch_adaptive_gravity_feedforward.md`

## 3. 关键改动摘要

- `application/gimbal_task.h`
  - 删除固定K方案宏（`PITCH_GRAVITY_K_FIXED`、`PITCH_GRAVITY_K_AMMO_FULL`、`PITCH_AMMO_CAPACITY`）。
  - 新增自适应前馈宏：学习率、初值、限幅、准静态门限、变化率限制。
- `application/gimbal_task.c`
  - 去除 `shoot_control_t` 依赖（删除 `extern shoot_control_t shoot_control;`）。
  - `gimbal_motor_relative_angle_control()` 改为：
    - 先算 `I_pid`（速度环输出）；
    - pitch 分支叠加 `I_ff = K_hat*cos(theta)+b_hat`；
    - 准静态满足时执行 LMS 更新 `K_hat/b_hat`，并做 step limit + clamp；
    - yaw 分支保持 `current_set = I_pid`。
  - 仍保留 `shoot_control_loop`/`shoot_get_fric_current` 调用（通过函数声明）。
- docs
  - 架构、模块图、风险日志、当前状态和索引同步到“自适应前馈”版本。

## 4. 关键决策与理由

- 决策 1：使用二参数模型 `K_hat*cos(theta)+b_hat`，而非继续增大固定K复杂度。  
  理由：在窄角域内重力项角度变化弱，常值偏置（摩擦/线束）不可忽略，二参数模型更贴近实测。
- 决策 2：仅在准静态区间学习。  
  理由：动态机动时 `I_pid` 含加速度与扰动分量，直接学习会污染参数。
- 决策 3：增加每周期变化率限制。  
  理由：1ms 控制周期下可避免噪声引发参数突跳和电流抖动。
- 备选方案与放弃理由：
  - 继续固定K+弹量计数：无法覆盖装填不确定与非重力偏置；
  - 引入更高阶模型：当前证据不足，先用最小可实现模型闭环验证。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增阻塞调用，无任务周期变更。
- CPU 影响：pitch ENCONDE 1ms 路径新增常数级浮点计算（`cos`、若干乘加、限幅判断）。
- 栈影响：仅增加少量局部变量；`K_hat/b_hat` 为函数内静态变量。
- 帧率/周期：`gimbal_task` 仍为 1ms；CAN 下发节拍不变。

## 6. 风险点

- 学习率过大：可能造成参数振荡与电流波动增大。
- 准静态门限不合适：可能“学不到”或“误学习”。
- `PITCH_TURN` 符号配置错误：会把误差学习到反方向。

## ⚠ 未验证假设

- 假设：`I_pid` 在准静态时主要反映重力/偏置残差，可作为 LMS 误差信号。  
  未验证原因：当前未完成实机对比采集。  
  潜在影响：若误差源不纯，`K_hat/b_hat` 收敛偏移。  
  后续验证计划：采集 `pitch_gyro/pitch_gyro_set/pitch_given_current/pitch_relative_set`，在静止保持段验证 `I_pid` 下降趋势。
- 假设：`PITCH_FF_SPEED_TH=0.5` 与 `PITCH_FF_ERR_TH=0.02` 可有效筛出准静态样本。  
  未验证原因：未做不同操控速度工况统计。  
  潜在影响：参数更新频率过低或受动态污染。  
  后续验证计划：空仓/装弹各做慢推、快推、静止三工况，统计更新触发占比与收敛质量。
- 假设：`PITCH_FF_ALPHA_DOT_MAX=5.0` 的步进限制能兼顾收敛速度与稳定性。  
  未验证原因：尚未比较不同 `alpha_dot_max`。  
  潜在影响：收敛过慢或对负载变化响应迟滞。  
  后续验证计划：对比 `5/10` 两档，记录 5s 内稳态误差和电流波动。

## 7. 验证方式与结果

- 串口：未执行。
- USB：待执行 MCP/VOFA+ 在线采集。
- 示波器：未执行。
- 上位机：待验证空仓上电学习、装弹后自适应增益、动态扰动后再收敛。
- 长跑：未执行（待 10min 连续动作温升与稳定性观察）。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 回退 `gimbal_motor_relative_angle_control()` 到固定前馈或纯 PID 版本；
  - 恢复 `gimbal_task.h` 的旧前馈参数宏组。
- 参数回滚点：
  - 将 `PITCH_FF_GAMMA_K/B` 设为 `0` 可临时冻结自适应（仅保留当前参数）。
- 运行时保护：
  - 回滚验证前先切安全挡，确认电流输出稳定再切换控制模式。

## 9. 附件/证据

- 上轮结论：`docs/ai_sessions/2026-02-27_tune_pitch_encode.md`
- 相关风险条目：`docs/risk_log.md`（2026-02-27 `gimbal_task`）

## 10. 控制链路三问（规则 6.1）

1. 当前故障的实际执行路径是什么？  
   实际运行在 pitch `ENCONDE` 控制路径：`gimbal_motor_relative_angle_control()`（角度环 -> 速度环 -> 电流输出），固定K前馈在该路径中无法覆盖实测负载变化。
2. 本次修改命中的是哪条路径？  
   直接命中同一函数同一分支：把固定前馈替换为准静态门控的自适应 `K_hat/b_hat` 在线估计。
3. 如何用数据证明本次修改生效？  
   观测 `pitch_relative/pitch_relative_set/pitch_gyro/pitch_gyro_set/pitch_given_current`：预期静止保持段 `I_pid` 贡献下降、稳态误差减小，装填变化后可再次收敛。
