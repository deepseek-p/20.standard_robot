# 2026-02-23 ATTI 跟随模式自旋修复（切模态 set 对齐）

## 1. 目标

- 本次要解决的问题：ATTI（`s0=MID`）切入跟随后出现 `wz_set` 高值，底盘在无输入时高速旋转。
- 预期可观测结果：切入 ATTI 跟随时无明显冲击；底盘跟随能力保留（目标仍为 `relative_angle -> 0`）。

## 2. 改动文件列表

- `application/chassis_behaviour.c`
- `application/chassis_task.c`
- `docs/INDEX.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/2026-02-23_chassis_atti_follow_spin_baseline_fix.md`

## 3. 关键改动摘要

- `application/chassis_behaviour.c`：恢复 `chassis_infantry_follow_gimbal_yaw_control()` 原逻辑 `*angle_set = swing_angle;`，不再使用“每帧 set=get”写法。
- `application/chassis_task.c`：`chassis_mode_change_control_transit()` 切入 `CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW` 时，将 `chassis_relative_angle_set` 从硬编码 `0.0f` 改为当前 `chassis_yaw_motor->relative_angle`。

## 4. 关键决策与理由

- 决策 1：保留 DJI 跟随语义（ATTI 目标仍是云台正前方，即 `relative=0`）。
  理由：避免“每帧 set=get”导致跟随功能失效。
- 决策 2：把修复点放在模式切换过渡而不是行为目标。
  理由：切模态时先做 `set=get`，用于抑制切换瞬间冲击；后续控制链路保持原设计。

## 5. 实时性影响（必须写）

- 阻塞变化：无。
- CPU 影响：仅变量赋值，开销可忽略。
- 栈影响：无。
- 帧率/周期：底盘 2ms 周期不变。

## 6. 风险点

- 若 `relative_angle` 本身长期偏置较大（机械/标定问题），后续仍可能出现较大跟随输出。
- ATTI/GPS 切换语义已调整，操作手需按新挡位认知使用。

## 6.1 改码前三问（00 规则 6.1）

1. 当前故障的实际执行路径是什么？
   从 VOFA 字段可见 `ch_mode=0(FOLLOW_GIMBAL)`、`wz_set=6000`、`rel=176mrad`、`rel_set=0`，命中 `chassis_set_contorl()` 的跟随分支并出现输出饱和。
2. 本次修改命中的是哪条路径？
   命中 `chassis_mode_change_control_transit()` 在切入 FOLLOW_GIMBAL 时的初始化路径。
3. 如何用数据证明本次修改生效？
   观察切入 MID 瞬间 `rel_set` 先与 `rel` 对齐，不出现首拍大误差导致的 `wz_set` 突刺。

## ⚠ 未验证假设

- 假设：当前异常主要由切模态初始化瞬间误差触发。
- 未验证原因：本次会话未执行实机复测。
- 潜在影响：若主因还包含标定偏置/机械拖拽，该修复可能只能部分改善。
- 后续验证计划：复测并记录 `ch_mode/wz_set/rel/rel_set/rc_ch2`，覆盖 MID/UP 切换和静止工况。

## 7. 验证方式与结果

- 代码级验证：已确认两处关键行已按方案改动。
- 编译验证：未执行 Keil 编译。
- 实机验证：待用户上电复测。

## 8. 回滚方式（如何恢复）

- 回滚 `application/chassis_task.c`：将该行改回 `chassis_relative_angle_set = 0.0f;`。
- 回滚 `application/chassis_behaviour.c`：保持 `*angle_set = swing_angle;`（本次已为该状态）。

## 9. 附件/证据

- 数据证据：用户会话 VOFA 帧（2026-02-23 17:15）。
- 相关风险条目：`docs/risk_log.md` 对应 2026-02-23 底盘跟随条目。

