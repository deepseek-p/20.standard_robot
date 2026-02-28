# CURRENT_STATE（收纳页）

同步日期：`2026-02-28`

## 当前生效控制语义（以代码为准）

- `s0=DOWN`：`CHASSIS_NO_MOVE` + `GIMBAL_ZERO_FORCE`
- `s0=MID`：`CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW` + `GIMBAL_ABSOLUTE_ANGLE`（HUST_Act）
- `s0=UP`：`CHASSIS_HUST_SELF_PROTECT` + `GIMBAL_ABSOLUTE_ANGLE`（HUST_SelfProtect）
- yaw 连续旋转链路保持启用（`GIMBAL_YAW_CONTINUOUS_TURN` 路径仍在）
- pitch 使用 `GIMBAL_MOTOR_ENCONDE` 模式（yaw 保持 GYRO）

## 各模块当前状态

### Chassis Follow（MID 跟随）
- 状态：Mitigated（待长跑）
- 参数：`Kp=8.0, Ki=0.5, Kd=0.0, max_out=3.0, max_iout=1.0`；刹车区间 `start=PI*0.6, end=PI*0.95`
- 最新文档：`2026-02-23_chassis_follow_wraparound_brake_fix.md`
- 待办：长跑温升验证

### Chassis SelfProtect（UP 小陀螺）
- 状态：Mitigated（口述，缺 VOFA+/长跑）
- 最新文档：`2026-02-24_chassis_selfprotect_sin_sign_fix.md`
- 待办：四场景联测（静止自旋、旋转+平移、方向切换、MID/UP 切挡瞬态）

### Yaw 连续旋转
- 状态：部分验收
- 最新文档：`2026-02-23_gimbal_yaw_continuous_turn_v4_callsite_split.md`
- 待办：长跑与温升验证（含反向长期工况）

### Pitch ENCONDE 基础环（角度+速度）
- 状态：Mitigated（待长跑）
- 参数：角度环 `Kp=24.0, Ki=0.0, Kd=0.0, max_out=10.0, max_iout=0.5`；速度环 `Kp=800, Ki=10, max_out=15000, max_iout=3000`；ECD LPF `alpha=0.05`
- 最新文档：`2026-02-25_pitch_encode_pid_kp24_ki0_finalize.md`
- 待办：长跑热稳定性验证

### Pitch 自适应重力前馈
- 状态：In Progress（7a→7b→7c→7d 四轮迭代，未完成闭环验证）
- 当前方案：LMS 自适应 `I_ff = K_hat*cos(θ) + b_hat`，误差自适应 gamma，硬限幅 ±16000，遥测通道 `55:ff_k_hat / 56:ff_b_hat`
- 最新文档：`2026-02-27_pitch_ff_adaptive_gamma.md`（7d）
- 关联文档：`2026-02-27_pitch_gravity_feedforward_bullet_count.md`（7a，已被 7b 替代）、`2026-02-27_pitch_adaptive_gravity_feedforward.md`（7b）、`2026-02-27_pitch_adaptive_ff_deadlock_fix.md`（7b-fix）、`2026-02-27_pitch_ff_decouple_telemetry.md`（7c）
- 待办：装弹工况闭环验证（收敛时间、稳态误差、电流纹波）；连续 4 轮未闭环，按规则 6.2 下一步应先采数据再改码

### Shoot 摩擦轮 CAN 闭环
- 状态：In Progress
- 最新文档：`2026-02-26_shoot_fric_can_closed_loop_migration.md`
- 待办：板端方向确认、ready 状态切换、连发验证

### WiFi 桥接（USART1 + ESP32）
- 状态：Blocked - 需数据
- 最新文档：`2026-02-26_wifi_bridge_migrate_to_usart1.md`
- 待办：实机联调（capture/set/get/dump 压测、丢帧统计、遥控链路回归）

### USB/WiFi 遥测互斥 + DBUS 稳定性
- 状态：In Progress
- 最新文档：`2026-02-27_usb_task_priority_below_normal_dbus_offline_fix.md`
- 关联：`2026-02-27_usb_telem_mode_mutex_drop_split.md`
- 待办：Keil 三模式编译验证 + 板端 WiFi 模式 5min 无 DBUS 误报

### USB CDC 在线调参
- 状态：In Progress
- 最新文档：`2026-02-24_usb_cdc_online_pid_tuning.md`
- 待办：Keil 编译 + 板端命令交互验证

### Pitch RC 灵敏度
- 状态：In Progress
- 最新文档：`2026-02-24_pitch_rc_sensitivity_reduce.md`
- 参数：`PITCH_RC_SEN = -0.000001f`（原 -0.000004f）
- 待办：VOFA+ 验证 rc_ch3 → pitch_abs_set 斜率

## 当前关键参数快照

- `application/chassis_task.h`：`CHASSIS_FOLLOW_GIMBAL_PID` = `8.0/0.5/0.0/3.0/1.0`
- `application/chassis_task.c`：`BRAKE_START=PI*0.6, BRAKE_END=PI*0.95`
- `application/gimbal_task.h`：
  - `PITCH_SPEED_PID` = `800/10/0/15000/3000`
  - `PITCH_ENCODE_RELATIVE_PID` = `24.0/0.0/0.0/10.0/0.5`
  - `PITCH_GYRO_ABSOLUTE_PID` = `5.0/0.3/0.1/5.0/1.0`
  - `PITCH_RC_SEN = -0.000001f`
  - Adaptive FF: `GAMMA_BASE=0.002, GAMMA_ERR_GAIN=0.05, GAMMA_MAX=0.02, ALPHA_DOT_MAX=50.0`

## 风险日志对应关系

- MID 跟随自旋/振荡：`risk_log.md` 2026-02-23 条目（Mitigated，待长跑）
- UP 小陀螺平移画圆：`risk_log.md` 2026-02-24 条目（Mitigated，缺 VOFA+/长跑）
- Pitch 自适应前馈迭代：`risk_log.md` 2026-02-27 条目（In Progress，按规则 6.2 需先采数据）
