# Docs 索引

本目录用于维护 `STM32F407 + FreeRTOS (CMSIS-RTOS v1)` 标准步兵固件的 AI 开发交接文档。

## 必读入口

- [00_AI_HANDOFF_RULES.md](00_AI_HANDOFF_RULES.md)：AI 交接与记录强约束（先读）
- [01_PROJECT_OVERVIEW.md](01_PROJECT_OVERVIEW.md)：项目概览与导航
- [02_ARCHITECTURE.md](02_ARCHITECTURE.md)：系统架构与关键数据流
- [03_TASK_MAP.md](03_TASK_MAP.md)：任务映射（由代码自动初填）
- [04_MODULE_MAP.md](04_MODULE_MAP.md)：模块地图（目录/关键文件）

## 模板与过程资产

- [risk_log.md](risk_log.md)：风险日志模板
- [ai_sessions/TEMPLATE.md](ai_sessions/TEMPLATE.md)：AI 会话记录模板
- [ai_sessions/CURRENT_STATE.md](ai_sessions/CURRENT_STATE.md)：当前状态收纳页
- [ai_sessions/2026-02-19_gimbal_yaw_continuous_turn.md](ai_sessions/2026-02-19_gimbal_yaw_continuous_turn.md)：Yaw 无限旋转改造会话记录
- [ai_sessions/2026-02-19_gimbal_yaw_continuous_turn_field_analysis.md](ai_sessions/2026-02-19_gimbal_yaw_continuous_turn_field_analysis.md)：Yaw 连续旋转实机现象与排查方案
- [ai_sessions/2026-02-20_chassis_decouple_pitch_down_guard.md](ai_sessions/2026-02-20_chassis_decouple_pitch_down_guard.md)：连续 yaw 版本底盘解耦与 pitch 下行保护改动记录
- [ai_sessions/2026-02-20_usb_debug_telemetry_framework.md](ai_sessions/2026-02-20_usb_debug_telemetry_framework.md)：USB 多通道调试遥测框架改造记录
- [ai_sessions/2026-02-21_pitch_relative_min_clamp_guard.md](ai_sessions/2026-02-21_pitch_relative_min_clamp_guard.md)：pitch 相对角模式最小边界夹紧触发条件修复记录
- [ai_sessions/2026-02-21_pitch_absolute_no_cmd_guard.md](ai_sessions/2026-02-21_pitch_absolute_no_cmd_guard.md)：pitch 绝对角模式无输入防风up保护记录
- [ai_sessions/2026-02-22_pitch_mode_switch_pid_reset_guard.md](ai_sessions/2026-02-22_pitch_mode_switch_pid_reset_guard.md)：pitch 模式切换瞬间 PID 状态清理与下冲保护记录
- [ai_sessions/2026-02-22_pitch_no_cmd_disturbance_rate_limit.md](ai_sessions/2026-02-22_pitch_no_cmd_disturbance_rate_limit.md)：pitch 无输入扰动触发下冲的绝对角纠偏限速修复记录
- [ai_sessions/2026-02-22_rollback_to_usb_debug_telemetry_framework.md](ai_sessions/2026-02-22_rollback_to_usb_debug_telemetry_framework.md)：按指令回退到 USB 刚修好基线的会话记录
- [ai_sessions/2026-02-22_chassis_gps_follow_yaw_direction_fix.md](ai_sessions/2026-02-22_chassis_gps_follow_yaw_direction_fix.md)：GPS 跟随模式底盘自激旋转（改动前后记录）
- [ai_sessions/2026-02-22_gimbal_yaw_continuous_turn_reapply.md](ai_sessions/2026-02-22_gimbal_yaw_continuous_turn_reapply.md)：Yaw 连续旋转与 yaw 校准跳步改造重新落地记录
- [ai_sessions/2026-02-23_usb_debug_telemetry_restore_after_rollback.md](ai_sessions/2026-02-23_usb_debug_telemetry_restore_after_rollback.md)：USB 调试遥测在回溯后的恢复记录
- [ai_sessions/2026-02-23_chassis_hust_mode_mapping_swap.md](ai_sessions/2026-02-23_chassis_hust_mode_mapping_swap.md)：ATTI/GPS 挡位映射为 HUST_Act/HUST_SelfProtect 的改动记录
- [ai_sessions/2026-02-23_gimbal_yaw_continuous_turn_v4_callsite_split.md](ai_sessions/2026-02-23_gimbal_yaw_continuous_turn_v4_callsite_split.md)：Yaw 连续角 v4（调用点拆分 + 模式切换同步 + snapshot 对齐）改动记录
- [ai_sessions/2026-02-23_chassis_atti_follow_spin_baseline_fix.md](ai_sessions/2026-02-23_chassis_atti_follow_spin_baseline_fix.md)：ATTI 跟随模式无输入自旋（wz_set 饱和）修复记录
- [ai_sessions/2026-02-23_chassis_atti_spin_mode_mismatch_analysis.md](ai_sessions/2026-02-23_chassis_atti_spin_mode_mismatch_analysis.md)：ATTI 自旋问题二次定位（FOLLOW 与 yaw ENCONDE 模式错配）记录
- [ai_sessions/2026-02-23_chassis_mid_no_follow_decouple_fix.md](ai_sessions/2026-02-23_chassis_mid_no_follow_decouple_fix.md)：执行版修复：MID 改 NO_FOLLOW 解除底盘-云台耦合死锁
- [ai_sessions/2026-02-23_chassis_mid_follow_restore_step1.md](ai_sessions/2026-02-23_chassis_mid_follow_restore_step1.md)：按需求执行 step1：MID 回切 FOLLOW_GIMBAL_YAW
- [ai_sessions/2026-02-23_mid_follow_gimbal_absolute_alignment_fix.md](ai_sessions/2026-02-23_mid_follow_gimbal_absolute_alignment_fix.md)：按根因修复 MID 跟随自旋：云台 MID 改绝对角，解除 FOLLOW+ENCONDE 死锁
- [ai_sessions/2026-02-23_chassis_follow_pid_damping_tune.md](ai_sessions/2026-02-23_chassis_follow_pid_damping_tune.md)：MID 跟随阻尼首调：Kp 40->15、Kd 0->5，抑制摆振
- [ai_sessions/2026-02-23_chassis_follow_pid_step1_soft_gain.md](ai_sessions/2026-02-23_chassis_follow_pid_step1_soft_gain.md)：MID 跟随振荡 step1：Kp/Kd/max_out 降级到温和参数组
- [ai_sessions/2026-02-23_chassis_follow_wraparound_brake_fix.md](ai_sessions/2026-02-23_chassis_follow_wraparound_brake_fix.md)：MID 跟随 ±PI 回绕极限环修复：大偏差刹车因子 + 参数重标定
- [ai_sessions/2026-02-24_docs_acceptance_backfill.md](ai_sessions/2026-02-24_docs_acceptance_backfill.md)：前几次底盘/云台联调的 docs 验收补录与状态收口
- [ai_sessions/2026-02-24_chassis_selfprotect_frame_rotation_compensation.md](ai_sessions/2026-02-24_chassis_selfprotect_frame_rotation_compensation.md)：UP 小陀螺平移坐标补偿，修复“推前进画圆”
- [ai_sessions/2026-02-24_chassis_selfprotect_sin_sign_fix.md](ai_sessions/2026-02-24_chassis_selfprotect_sin_sign_fix.md)：UP 平移补偿第二轮：修正 `sin(relative_angle)` 方向符号
- [adr/ADR_TEMPLATE.md](adr/ADR_TEMPLATE.md)：架构决策记录模板

## 根目录兼容入口

- [../AI_HANDOVER_CURRENT_STATE.md](../AI_HANDOVER_CURRENT_STATE.md)
- [../AGENTS.md](../AGENTS.md)
- [../git-guide.md](../git-guide.md)
- [../README.md](../README.md)

