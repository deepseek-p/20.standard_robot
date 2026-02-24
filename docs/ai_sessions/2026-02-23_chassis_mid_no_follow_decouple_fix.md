# 2026-02-23 chassis MID no-follow decouple fix

## 1. 目标

- 本次要解决的问题：ATTI（`s0=MID`）无摇杆输入时，底盘持续自旋且 `wz_set` 长期饱和。
- 预期可观测结果：MID 下 `ch_mode` 进入 `NO_FOLLOW`，`rc_ch2=0` 时 `wz_set` 贴近 0，不再出现 `±6000` 持续饱和。

## 2. 改动文件列表

- `application/chassis_behaviour.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-23_chassis_mid_no_follow_decouple_fix.md`

## 3. 关键改动摘要

- `application/chassis_behaviour.c`：将 `switch_is_mid(...)` 分支由 `CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW` 改为 `CHASSIS_NO_FOLLOW_YAW`。
- `docs/04_MODULE_MAP.md`：更新挡位语义说明，明确 `s0=MID` 为 HUST_Act 且不跟随 yaw。
- `docs/risk_log.md`：将“FOLLOW + ENCONDE 导致自旋”风险从“方案阶段”更新为“已实施修复，待回归验证”。
- `docs/INDEX.md`：新增本次会话索引入口。

## 4. 关键决策与理由

- 决策 1：不改云台 MID（`GIMBAL_RELATIVE_ANGLE`）语义，只改底盘 MID 模式。  
  理由：问题根因是“底盘 FOLLOW 与云台 ENCONDE”耦合死锁，底盘侧改为 no-follow 可直接解除耦合，改动最小。

- 决策 2：不通过 PID 调参规避。  
  理由：该问题是控制目标冲突，不是参数震荡；只调参会降低症状但不消除死锁。

- 备选方案与放弃理由：
  - 备选 A：保持底盘 FOLLOW，MID 把云台改成 GYRO。  
    放弃理由：会改变云台 MID 操控语义，联调迁移成本高。
  - 备选 B：FOLLOW 分支加条件降级。  
    放弃理由：可作为二级保险，但首轮先做最小映射改动，便于快速验证。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增阻塞调用，无 `osDelay` / `vTaskDelay` 变更。
- CPU 影响：仅模式分支选择变化，无高频计算增加。
- 栈影响：无新增局部大对象，无任务栈配置修改。
- 帧率/周期：控制任务周期与时序不变。

## 6. 风险点

- 风险 1：MID 从 FOLLOW 变为 NO_FOLLOW 后，操作手体感会变化（需依赖 `ch2` 主动给 yaw）。
- 风险 2：若后续再次引入 FOLLOW 且云台仍为 ENCONDE，问题可能复现。

## ⚠ 未验证假设

- 假设：当前战术语义允许 `s0=MID` 不做底盘自动跟随。
- 未验证原因：本次已改代码，但尚未完成实机回传日志验证。
- 潜在影响：若期望 MID 必须自动对准云台，需改为“仅在 yaw=GYRO 时 FOLLOW”的条件策略。
- 后续验证计划：实机回传 `11/14/15/16/23/42/44` 连续帧，覆盖 MID 静止、MID 旋转输入、UP 切回 SelfProtect 三类场景；负责人 `@Codex`。

## 7. 验证方式与结果

- 串口：未执行（待实机）。
- USB：待实机回传新帧。
- 示波器：未执行。
- 上位机：待 VOFA+ 回归。
- 长跑：未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：`application/chassis_behaviour.c` 中 MID 分支将 `CHASSIS_NO_FOLLOW_YAW` 改回 `CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW`。
- 参数回滚点：本次无参数宏改动。
- 运行时保护（例如先发零电流）：回滚/刷写后先架空验证，再落地联调。

## 9. 附件/证据

- 日志路径：本轮执行前使用的 VOFA+ 帧（用户会话 2026-02-23 18:23）。
- 截图/波形：暂无。
- 相关风险条目：`docs/risk_log.md` 中“FOLLOW + ENCONDE 自旋”条目。
