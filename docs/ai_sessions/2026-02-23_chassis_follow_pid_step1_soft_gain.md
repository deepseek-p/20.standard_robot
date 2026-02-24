# 2026-02-23 chassis follow PID step1 soft gain

## 1. 目标

- 本次要解决的问题：MID（底盘跟随云台 yaw）在实机中出现左右摆振与架空慢速持续转。
- 预期可观测结果：不改控制结构前提下，通过温和参数降低饱和与过冲，`wz_set` 不再高频大幅换向。

## 2. 改动文件列表

- `application/chassis_task.h`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-23_chassis_follow_pid_step1_soft_gain.md`

## 3. 关键改动摘要

- `application/chassis_task.h`：仅改底盘跟随 PID 五个宏（第一步最小改动），无 `.c` 改动。
  - `CHASSIS_FOLLOW_GIMBAL_PID_KP: 40.0f -> 6.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_KI: 0.0f -> 0.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_KD: 15.0f -> 0.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT: 6.0f -> 4.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT: 0.2f -> 0.2f`
- `docs/risk_log.md`：同步记录本轮 step1 参数组与下一步触发条件。
- `docs/INDEX.md`：新增本会话索引。

## 4. 关键决策与理由

- 决策 1：严格按用户约束执行 step1，仅改头文件参数宏，不改任意 `.c` 文件。
- 决策 2：先降 `Kp` 与 `max_out`、去掉 `Kd`，避免误差差分 D 项在 2ms 周期下放大噪声/离散抖动。
- 决策 3：不提前实现串级速度环，等待 step1 实机数据闭环后再进入 step2。

### 4.1 改码前三问（对应 `docs/00_AI_HANDOFF_RULES.md` 6.1）

1. 当前故障的实际执行路径是什么？  
   已确认故障发生于 MID 跟随链路：`chassis_task.c` 的单环跟随 `wz_set = -PID_calc(angle_pid, rel, rel_set)`，且实机表现为振荡/持续慢转。
2. 本次修改命中的是哪条路径？  
   命中同一条路径的关键参数宏：`CHASSIS_FOLLOW_GIMBAL_PID_*`，不改模式层、不改 PID 库、不改云台侧代码。
3. 如何用数据证明本次修改生效？  
   对比改前后 MID 帧：`wz_set` 峰值与换向频率下降，`rel` 回零过程振荡幅度下降；若仍不满足，再进入 step2 串级速度环。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增/删除 `osDelay`、`vTaskDelay`、等待机制。
- CPU 影响：仅宏常量变化，无新增计算路径。
- 栈影响：无任务栈大小变更。
- 帧率/周期：任务周期与控制时序不变。

## 6. 风险点

- 风险 1：参数过软可能导致跟随迟滞，操控手感“慢”。
- 风险 2：无速度环时，受云台反作用力矩影响仍可能残留低频摆动。

## ⚠ 未验证假设

- 假设：当前主导问题是参数过激（饱和+过冲），step1 足以显著改善振荡。
- 未验证原因：本次仅完成代码参数修改，尚未拿到改后实机 VOFA+ 数据。
- 潜在影响：若系统结构性不足（缺速度内环）占主导，step1 只能部分改善，仍需 step2。
- 后续验证计划：回传 MID 静止 5s + MID yaw 阶跃松杆 5s，重点看 `11/14/15/16/23/42/44`；若仍振荡或响应不足，按计划进入串级速度环改造。

## 7. 验证方式与结果

- 串口：未执行（待用户实机回传）。
- USB：未执行（待用户实机回传）。
- 示波器：未执行。
- 上位机：未执行。
- 长跑：未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：`application/chassis_task.h` 恢复上一组跟随参数（`Kp=40, Ki=0, Kd=15, max_out=6, max_iout=0.2`）。
- 参数回滚点：仅上述五个宏。
- 运行时保护：建议先架空验证，再低速落地验证。

## 9. 附件/证据

- 关联数据：`data.md`（改前振荡数据基线）。
- 相关风险条目：`docs/risk_log.md` 中 2026-02-23 ATTI 跟随风险条目。
