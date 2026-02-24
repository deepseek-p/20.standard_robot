# 2026-02-23 chassis follow PID damping tune

## 1. 目标

- 本次要解决的问题：MID（ATTI/HUST_Act）在不再满速自旋后仍出现底盘左右摆振、架空慢速持续转。
- 预期可观测结果：在 `s0=MID` 且 `ch_mode=0` 的跟随场景下，`wz_set` 不再高幅反复换向，`rel` 收敛过程更平滑。

## 2. 改动文件列表

- `application/chassis_task.h`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-23_chassis_follow_pid_damping_tune.md`

## 3. 关键改动摘要

- `application/chassis_task.h`
  - `CHASSIS_FOLLOW_GIMBAL_PID_KP: 40.0f -> 15.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_KD: 0.0f -> 5.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_KI` 维持 `0.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT/MAX_IOUT` 维持不变
- `docs/risk_log.md`：将 ATTI FOLLOW 风险条目同步为“step3 首轮阻尼参数已落地”。
- `docs/INDEX.md`：新增本会话索引入口。

## 4. 关键决策与理由

- 决策 1：先做阻尼首调（`Kp=15, Kd=5`），不同时引入积分项。  
  理由：先抑制摆振与超调，再根据是否存在稳态偏差决定是否加 `Ki`，避免一次引入多变量。
- 决策 2：保持 `max_out/max_iout` 不变。  
  理由：本轮目标是降低激进程度与增强阻尼，不改变输出限幅边界，方便与上轮数据直接对比。
- 备选方案与放弃理由：
  - 备选 A：直接加 `Ki`。  
    放弃理由：当前主问题是振荡/超调，不是稳态偏差优先。
  - 备选 B：仅降 `Kp` 不加 `Kd`。  
    放弃理由：仅降比例可能变慢但仍可能欠阻尼，`Kd` 更直接抑制摆振。

### 4.1 改码前三问（对应 `docs/00_AI_HANDOFF_RULES.md` 6.1）

1. 当前故障的实际执行路径是什么？  
   已确认 MID 路径为 `ch_mode=FOLLOW_GIMBAL_YAW` 且已切到 `yaw_mode=ABS`；故障表现为跟随环输出过激导致摆振。
2. 本次修改命中的是什么路径？  
   命中 `chassis_task` 跟随角 PID 宏（`CHASSIS_FOLLOW_GIMBAL_PID_*`），直接作用于 `wz_set` 跟随闭环。
3. 如何用数据证明本次修改生效？  
   对比改前后 MID 日志：`wz_set` 峰值与换向频率下降、`rel` 回零过程振荡幅度减小、四轮电流反向拍击减弱。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增/删除 `osDelay`、`vTaskDelay`、等待路径。
- CPU 影响：仅宏参数变化，无新增计算分支。
- 栈影响：无任务栈改动。
- 帧率/周期：任务周期与调度时序不变。

## 6. 风险点

- 风险 1：`Kp` 下调后可能出现“跟随变慢、松杆后回零拖尾”。
- 风险 2：`Ki=0` 下可能保留小角度稳态偏差（差几度不回零）。

## ⚠ 未验证假设

- 假设：当前摆振主因是跟随环欠阻尼而非机械拖带/地面摩擦不均。
- 未验证原因：本次为离线改参与代码提交，尚未拿到改后 VOFA+ 回传帧。
- 潜在影响：若机械因素占主导，参数调整收益会受限，可能需要机械检查或模式层补偿。
- 后续验证计划：由 `@Codex` 根据用户回传的 MID 静止 + 阶跃帧，按 `Kp 15 -> 10/20` 阶梯法继续收敛，必要时引入 `Ki=0.5,max_iout=1.0`。

## 7. 验证方式与结果

- 串口：未执行（待实机回传）。
- USB：未执行（待 VOFA+ 回传）。
- 示波器：未执行。
- 上位机：未执行。
- 长跑：未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：`application/chassis_task.h` 将 `Kp/Kd` 恢复为 `40/0`。
- 参数回滚点：仅本次两个宏值。
- 运行时保护：优先架空验证，再落地低速验证，确认无异常再扩大动作幅度。

## 9. 附件/证据

- 关联数据文件：`data.md`（改前振荡现象数据）。
- 相关风险条目：`docs/risk_log.md` 中“ATTI FOLLOW + yaw 模式耦合与参数收敛”条目。
