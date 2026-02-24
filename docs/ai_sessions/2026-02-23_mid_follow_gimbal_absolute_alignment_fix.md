# 2026-02-23 MID follow gimbal absolute alignment fix

## 1. 目标

- 本次要解决的问题：ATTI（`s0=MID`）在底盘 FOLLOW 模式下无摇杆输入仍持续自旋（`wz_set` 长时间饱和）。
- 预期可观测结果：MID 静止时底盘不再自旋；“枪口朝哪，车头慢慢跟哪”成立。

## 2. 改动文件列表

- `application/gimbal_behaviour.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-23_mid_follow_gimbal_absolute_alignment_fix.md`

## 3. 关键改动摘要

- `application/gimbal_behaviour.c`：将 MID 分支从 `GIMBAL_RELATIVE_ANGLE` 改为 `GIMBAL_ABSOLUTE_ANGLE`。
- `docs/04_MODULE_MAP.md`：同步记录云台拨杆映射为 `s0=DOWN -> ZERO_FORCE`、`s0=MID/UP -> ABSOLUTE_ANGLE`。
- `docs/risk_log.md`：将“FOLLOW + ENCONDE 耦合死锁”条目标注为已执行 step2（MID 改绝对角），并更新验证指标。
- `docs/INDEX.md`：新增本会话索引入口。

## 4. 关键决策与理由

- 决策 1：只改一行模式映射，不叠加 PID 参数修改。  
  理由：先解除结构性耦合（FOLLOW 对 ENCONDE）再看闭环是否收敛，避免一次改动多个变量导致定位失真。
- 决策 2：保留底盘 MID=FOLLOW 语义不变。  
  理由：需求是 `Chassis_Act_Mode` 风格跟随，不是 no-follow 手动自旋。
- 备选方案与放弃理由：  
  - 备选 A：继续 MID=no-follow。  
    放弃理由：与“枪口朝哪，车头慢慢跟哪”目标相反。  
  - 备选 B：仅调底盘跟随 PID（Kp/Ki）。  
    放弃理由：未解除模式耦合前，参数调优不能消除不可收敛根因。

### 4.1 改码前三问（对应 `docs/00_AI_HANDOFF_RULES.md` 6.1）

1. 当前故障的实际执行路径是什么？  
   用户日志显示：`ch_mode=0(FOLLOW)`、`yaw_mode=2(ENCONDE)`、`rc_ch2=0`、`wz_set=-6000` 持续饱和，符合“底盘跟随环 + 云台相对角环互相对抗”的路径。
2. 本次修改命中的是什么路径？  
   命中 `gimbal_behaviour_mode_set()` 的 MID 分支：把云台 yaw 从相对角控制切到绝对角控制，打断 FOLLOW+ENCONDE 闭锁环。
3. 如何用数据证明本次修改生效？  
   复测 MID 静止与阶跃段，期望：`yaw_mode` 从 `2` 变 `1`；静止时 `wz_set` 由饱和回到近 0；松杆后 `rel` 向 0 收敛。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增/删除 `osDelay`、`vTaskDelay` 或等待机制。
- CPU 影响：无新增高频计算路径，仅改状态机分支枚举值。
- 栈影响：无任务栈大小变更。
- 帧率/周期：任务调度周期与控制链路时序不变。

## 6. 风险点

- 风险 1：MID 与 UP 都为云台绝对角后，操作手对两挡云台手感差异变小，可能产生模式认知偏差。
- 风险 2：若机械/摩擦条件较差，解除耦合后仍可能出现“能跟随但回零慢”，后续可能仍需底盘 Ki 微调。

## ⚠ 未验证假设

- 假设：当前实机在 MID 下确实需要“云台绝对角 + 底盘 FOLLOW”组合才能满足操控目标。
- 未验证原因：本次仅完成代码与文档修改，尚未拿到改后实机回传帧。
- 潜在影响：若实机硬件存在额外耦合（轴承拖带、线束阻尼），可能出现跟随迟滞，需要在参数层二次收敛。
- 后续验证计划：由 `@Codex` 按 MID 静止 5s + MID yaw 阶跃松杆场景复测 `11/14/15/23/42/44`，并据结果决定是否进入 PID 调参。

## 7. 验证方式与结果

- 串口：未执行（待用户上电复测）。
- USB：未执行（待用户回传 VOFA+ 新数据）。
- 示波器：未执行。
- 上位机：未执行（本次仅完成改码与记录）。
- 长跑：未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：`application/gimbal_behaviour.c` 将 MID 分支改回 `GIMBAL_RELATIVE_ANGLE`。
- 参数回滚点：本次无参数改动。
- 运行时保护：先架空验证再落地，确认 `wz_set` 不再饱和后再做长时间地面测试。

## 9. 附件/证据

- 日志路径：`data`（本轮前后对比用）。
- 相关风险条目：`docs/risk_log.md` 中“ATTI 下 FOLLOW + ENCONDE 耦合自旋”条目。

