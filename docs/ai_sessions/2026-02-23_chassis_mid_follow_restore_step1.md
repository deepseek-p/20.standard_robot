# 2026-02-23 chassis MID follow restore step1

## 1. 目标

- 本次要解决的问题：按需求恢复 MID 档“枪口朝哪，车头慢跟哪”的底盘跟随行为。
- 预期可观测结果：MID 工况下 `ch_mode=0(FOLLOW_GIMBAL_YAW)`，底盘 `wz_set` 由跟随 PID 输出而非 `rc_ch2` 直通。

## 2. 改动文件列表

- `application/chassis_behaviour.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-23_chassis_mid_follow_restore_step1.md`

## 3. 关键改动摘要

- `application/chassis_behaviour.c`：`switch_is_mid(...)` 分支从 `CHASSIS_NO_FOLLOW_YAW` 回切到 `CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW`。
- `docs/04_MODULE_MAP.md`：同步当前映射语义为“MID -> FOLLOW_GIMBAL_YAW（step1 回切）”。
- `docs/risk_log.md`：将相关风险缓解策略更新为“已执行 step1，待用新数据判定是否进入 step2/step3 调参”。
- `docs/INDEX.md`：新增本会话索引入口。

## 4. 关键决策与理由

- 决策 1：先执行用户指定的 step1（仅回切 MID->FOLLOW），不同时引入 PID 参数改动。  
  理由：先验证模式路径正确性，再决定参数层改动，避免一次改动过多导致定位失真。

- 决策 2：保留 `UP -> HUST_SelfProtect` 映射不动。  
  理由：本轮聚焦 MID 跟随功能恢复。

- 备选方案与放弃理由：
  - 备选 A：继续保持 MID=no-follow。  
    放弃理由：与当前需求“枪口朝哪车头慢跟哪”不一致。
  - 备选 B：同步直接上 step2/step3 调参。  
    放弃理由：未先验证 step1 的路径效果，存在叠加变量风险。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增/删除 `osDelay`、`vTaskDelay`。
- CPU 影响：无新增高频计算路径，仍是既有跟随 PID 链路。
- 栈影响：无任务栈大小变更。
- 帧率/周期：任务周期与调度时序不变。

## 6. 风险点

- 风险 1：由于 MID 下云台仍可能处于 `yaw_mode=ENCONDE`，恢复 FOLLOW 后可能再次出现耦合冲突与饱和输出。
- 风险 2：若跟随 PID 仅 P 环，可能出现稳态误差（车头“差几度”）。

## ⚠ 未验证假设

- 假设：在当前机械/摩擦条件下，MID 回切 FOLLOW 能满足“慢跟枪口”且不触发持续自旋。
- 未验证原因：本次仅改代码与文档，尚未拿到回切后的新实机帧。
- 潜在影响：若出现再次自旋，需尽快进入“模式兼容+参数”联合策略，避免反复回切。
- 后续验证计划：采集 MID 静止 5s 与 MID 旋转阶跃段，重点观察 `11/14/15/16/23/42/44`；负责人 `@Codex`。

## 7. 验证方式与结果

- 串口：未执行（待用户上电回传）。
- USB：未执行（待用户上电回传）。
- 示波器：未执行。
- 上位机：未执行。
- 长跑：未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：`application/chassis_behaviour.c` MID 分支改回 `CHASSIS_NO_FOLLOW_YAW`。
- 参数回滚点：本次无参数变更。
- 运行时保护（例如先发零电流）：建议先架空验证，再进行落地测试。

## 9. 附件/证据

- 日志路径：`data`（本轮分析前基线数据）。
- 截图/波形：暂无。
- 相关风险条目：`docs/risk_log.md` 中“FOLLOW_GIMBAL_YAW + ENCONDE 耦合”条目。

