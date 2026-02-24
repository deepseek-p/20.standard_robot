# CURRENT_STATE（收纳页）

同步日期：`2026-02-24`
最近补录：`docs/ai_sessions/2026-02-24_chassis_selfprotect_sin_sign_fix.md`

## 当前生效控制语义（以代码为准）

- `s0=DOWN`：`CHASSIS_NO_MOVE` + `GIMBAL_ZERO_FORCE`
- `s0=MID`：`CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW` + `GIMBAL_ABSOLUTE_ANGLE`（HUST_Act）
- `s0=UP`：`CHASSIS_HUST_SELF_PROTECT` + `GIMBAL_ABSOLUTE_ANGLE`（HUST_SelfProtect）
- yaw 连续旋转链路保持启用（`GIMBAL_YAW_CONTINUOUS_TURN` 路径仍在）

## 前几次改动验收状态（补录）

| 会话 | 验收状态 | 当前结论 |
| --- | --- | --- |
| `2026-02-23_chassis_mid_follow_restore_step1.md` + `2026-02-23_mid_follow_gimbal_absolute_alignment_fix.md` | 已验收 | MID 下已切到 FOLLOW + ABS 组合，解除 FOLLOW/ENCONDE 结构性冲突。 |
| `2026-02-23_chassis_follow_pid_step1_soft_gain.md` + `2026-02-23_chassis_follow_wraparound_brake_fix.md` + 后续参数补调 | 已验收 | MID 振荡主问题已压住，用户反馈“MID 模式已经基本调好”。 |
| `2026-02-23_gimbal_yaw_continuous_turn_v4_callsite_split.md` | 部分验收 | 用户反馈 MID/UP 切挡与 yaw 连续转圈可用；长跑与温升验证仍待补齐。 |
| `2026-02-23_chassis_hust_mode_mapping_swap.md` + `2026-02-24_chassis_selfprotect_frame_rotation_compensation.md` + `2026-02-24_chassis_selfprotect_sin_sign_fix.md` | 已验收（口述） | 用户反馈 UP“现象正常，可边转边走”；当前缺 VOFA+ 量化与长跑证据。 |

## 当前关键参数快照

- `application/chassis_task.h`
- `CHASSIS_FOLLOW_GIMBAL_PID_KP=8.0f`
- `CHASSIS_FOLLOW_GIMBAL_PID_KI=0.5f`
- `CHASSIS_FOLLOW_GIMBAL_PID_KD=0.0f`
- `CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT=3.0f`
- `CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT=1.0f`
- `application/chassis_task.c`
- `CHASSIS_FOLLOW_ANGLE_BRAKE_START=(PI*0.6f)`
- `CHASSIS_FOLLOW_ANGLE_BRAKE_END=(PI*0.95f)`

## 仍待完成的验收项

- UP（HUST_SelfProtect）四场景联测：静止自旋、旋转+平移、方向切换、MID/UP 切挡瞬态。
- yaw 连续旋转长跑与温升验证（含正反向长期工况）。

## 风险日志对应关系

- MID 跟随自旋/振荡：见 `docs/risk_log.md` 中 2026-02-23 `chassis_task / gimbal_behaviour` 条目（已转为“Mitigated，待长跑”）。
- UP 小陀螺平移画圆：见 `docs/risk_log.md` 中 2026-02-24 `chassis_behaviour / chassis_task` 条目（`Mitigated（口述，待VOFA+/长跑）`）。
