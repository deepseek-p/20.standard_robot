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
- [adr/ADR_TEMPLATE.md](adr/ADR_TEMPLATE.md)：架构决策记录模板

## 根目录兼容入口

- [../AI_HANDOVER_CURRENT_STATE.md](../AI_HANDOVER_CURRENT_STATE.md)
- [../AGENTS.md](../AGENTS.md)
- [../git-guide.md](../git-guide.md)
- [../README.md](../README.md)

