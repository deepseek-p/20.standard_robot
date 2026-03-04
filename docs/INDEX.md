# Docs 索引

本目录用于维护 `STM32F407 + FreeRTOS (CMSIS-RTOS v1)` 标准步兵固件的 AI 开发交接文档。

## 必读入口

- [00_AI_HANDOFF_RULES.md](00_AI_HANDOFF_RULES.md)：AI 交接与记录强约束（先读）
- [01_PROJECT_OVERVIEW.md](01_PROJECT_OVERVIEW.md)：项目概览与导航
- [02_ARCHITECTURE.md](02_ARCHITECTURE.md)：系统架构与关键数据流
- [03_TASK_MAP.md](03_TASK_MAP.md)：任务映射（由代码自动初填）
- [04_MODULE_MAP.md](04_MODULE_MAP.md)：模块地图（目录/关键文件）
- [05_BOARD_PHYSICAL_MAP.md](05_BOARD_PHYSICAL_MAP.md)：C 板物理接口与外设连接手册（丝印对照、PWM/DBUS/UART 详情、裁判系统架构、VT03 接入分析）

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
- [ai_sessions/2026-02-24_pitch_linkage_gyro_mode_pid_tune.md](ai_sessions/2026-02-24_pitch_linkage_gyro_mode_pid_tune.md)：Pitch 连杆工况参数重标定（外环/速度环/灵敏度）
- [ai_sessions/2026-02-24_pitch_speed_loop_limit_cycle_tune.md](ai_sessions/2026-02-24_pitch_speed_loop_limit_cycle_tune.md)：Pitch 速度环极限环二调（仅速度环降增益）
- [ai_sessions/2026-02-24_pitch_rc_sensitivity_reduce.md](ai_sessions/2026-02-24_pitch_rc_sensitivity_reduce.md)：Pitch 遥控输入灵敏度下调（仅改 `PITCH_RC_SEN`）
- [ai_sessions/2026-02-24_usb_cdc_online_pid_tuning.md](ai_sessions/2026-02-24_usb_cdc_online_pid_tuning.md)：USB CDC 在线 PID 调参链路（RX 命令解析 + 运行时读写）
- [ai_sessions/2026-02-25_pitch_absolute_mode_encode_fallback.md](ai_sessions/2026-02-25_pitch_absolute_mode_encode_fallback.md)：AHRS pitch 失真场景下的 pitch ENCONDE fallback（yaw 保持 GYRO）
- [ai_sessions/2026-02-25_pitch_encode_speed_feedback_from_encoder.md](ai_sessions/2026-02-25_pitch_encode_speed_feedback_from_encoder.md)：Pitch ENCONDE 模式下速度环改用编码器转速反馈（替代 IMU pitch gyro）
- [ai_sessions/2026-02-25_pitch_encode_ecd_lpf_damping.md](ai_sessions/2026-02-25_pitch_encode_ecd_lpf_damping.md)：Pitch ENCONDE 速度反馈加入一阶低通，抑制 1ms 量化阶梯引发的残余振荡
- [ai_sessions/2026-02-25_pitch_encode_pid_kp24_ki0_finalize.md](ai_sessions/2026-02-25_pitch_encode_pid_kp24_ki0_finalize.md)：Pitch ENCONDE 角度环参数固化为 Kp=24/Ki=0，消除上电重复调参
- [ai_sessions/2026-02-26_shoot_fric_can_closed_loop_migration.md](ai_sessions/2026-02-26_shoot_fric_can_closed_loop_migration.md)：摩擦轮从 PWM 开环迁移到 hcan2 双 3508 CAN 闭环速度控制
- [ai_sessions/2026-02-26_wifi_bridge_usart6_esp32_mcp.md](ai_sessions/2026-02-26_wifi_bridge_usart6_esp32_mcp.md)：WiFi 无线调参链路落地（USART6 + ESP32 + MCP）
- [ai_sessions/2026-02-26_wifi_bridge_migrate_to_usart1.md](ai_sessions/2026-02-26_wifi_bridge_migrate_to_usart1.md)：WiFi 桥接迁移到 USART1（板载 UART2 4pin 映射）
- [ai_sessions/2026-02-27_pitch_gravity_feedforward_bullet_count.md](ai_sessions/2026-02-27_pitch_gravity_feedforward_bullet_count.md)：Pitch ENCONDE 重力前馈补偿 + 拨弹发射计数衰减补偿
- [ai_sessions/2026-02-27_pitch_adaptive_gravity_feedforward.md](ai_sessions/2026-02-27_pitch_adaptive_gravity_feedforward.md)：Pitch ENCONDE 自适应重力前馈（LMS在线估计 K_hat/b_hat）
- [ai_sessions/2026-02-27_pitch_adaptive_ff_deadlock_fix.md](ai_sessions/2026-02-27_pitch_adaptive_ff_deadlock_fix.md)：Pitch 自适应前馈去角度误差门限，修复装弹工况学习死锁
- [ai_sessions/2026-02-27_pitch_ff_decouple_telemetry.md](ai_sessions/2026-02-27_pitch_ff_decouple_telemetry.md)：Pitch 自适应前馈解耦硬限幅 + FF观测遥测通道
- [ai_sessions/2026-02-27_pitch_ff_adaptive_gamma.md](ai_sessions/2026-02-27_pitch_ff_adaptive_gamma.md)：Pitch 自适应前馈误差自适应GAMMA加速收敛
- [ai_sessions/2026-02-27_usb_telem_mode_mutex_drop_split.md](ai_sessions/2026-02-27_usb_telem_mode_mutex_drop_split.md)：USB/WiFi/关闭 三模式互斥遥测 + drop_cnt 双通道独立计数
- [ai_sessions/2026-02-27_usb_task_priority_below_normal_dbus_offline_fix.md](ai_sessions/2026-02-27_usb_task_priority_below_normal_dbus_offline_fix.md)：WiFi 遥测模式下降低 USBTask 优先级，修复 DBUS 误离线告警
- [ai_sessions/2026-02-28_shoot_trigger_motor_optimization.md](ai_sessions/2026-02-28_shoot_trigger_motor_optimization.md)：拨轮双环 + 本地热量预测/爆发模式落地
- [ai_sessions/2026-02-28_vt03_uart_mode_switching.md](ai_sessions/2026-02-28_vt03_uart_mode_switching.md)：VT03 图传链路接入 + UART 三模式编译期切换
- [ai_sessions/2026-03-01_vt03_keyboard_action.md](ai_sessions/2026-03-01_vt03_keyboard_action.md)：VT03/VT13 键鼠动作集中管理（keyboard_action）与底盘/云台/发射联动
- [ai_sessions/2026-03-02_shoot_single_fire_reverse_grid_fix.md](ai_sessions/2026-03-02_shoot_single_fire_reverse_grid_fix.md)：Shoot 单发修复 + fn_2 长按反转持续输出 + 拨轮 1/9 格数修正
- [ai_sessions/2026-03-03_detect_trigger_motor_toe_temp_disable.md](ai_sessions/2026-03-03_detect_trigger_motor_toe_temp_disable.md)：临时禁用 TRIGGER_MOTOR 离线检测（电机拆卸维修期间）
- [ai_sessions/2026-03-03_vt03_dbus_mouse_button_preserve.md](ai_sessions/2026-03-03_vt03_dbus_mouse_button_preserve.md)：VT03 保留 DBUS 鼠标左右键（仅 DBUS 离线时清零）
- [ai_sessions/2026-03-04_gimbal_pitch_mouse_y_invert.md](ai_sessions/2026-03-04_gimbal_pitch_mouse_y_invert.md)：反转鼠标 Y 轴控制 pitch 方向（仅改两处符号）
- [adr/ADR_TEMPLATE.md](adr/ADR_TEMPLATE.md)：架构决策记录模板

## 根目录兼容入口

- [../AI_HANDOVER_CURRENT_STATE.md](../AI_HANDOVER_CURRENT_STATE.md)
- [../AGENTS.md](../AGENTS.md)
- [../git-guide.md](../git-guide.md)
- [../README.md](../README.md)

