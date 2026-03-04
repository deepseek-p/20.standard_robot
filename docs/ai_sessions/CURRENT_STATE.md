# CURRENT_STATE（收纳页）

同步日期：`2026-03-02`

## 当前生效控制语义（以代码为准）

- `s0=DOWN`：`CHASSIS_NO_MOVE` + `GIMBAL_ZERO_FORCE`
- `s0=MID`：`CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW` + `GIMBAL_ABSOLUTE_ANGLE`（HUST_Act）
- `s0=UP`：`CHASSIS_HUST_SELF_PROTECT` + `GIMBAL_ABSOLUTE_ANGLE`（HUST_SelfProtect）
- yaw 连续旋转链路保持启用（`GIMBAL_YAW_CONTINUOUS_TURN` 路径仍在）
- pitch 使用 `GIMBAL_MOTOR_ENCONDE` 模式（yaw 保持 GYRO）
- VT03 键鼠动作层已接入（`keyboard_action`）：支持 `fn_1/fn_2/pause/trigger/dial` 映射

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

### Shoot 发射链路（摩擦轮 CAN + 拨轮双环 + 热量预测）
- 状态：In Progress（单发/反转/格数三项缺陷已修复，待回归）
- 最新文档：`2026-03-02_shoot_single_fire_reverse_grid_fix.md`
- 当前方案：
  - 摩擦轮：双 3508 CAN 速度闭环
  - 拨轮：`trigger_pos_pid`（位置外环）+ `trigger_spd_pid`（速度内环）
  - 单发：`shoot_bullet_control()` 移除 key=OFF 提前退出，按纯位置环到位后进入 `SHOOT_DONE`
  - 制动：`SHOOT_DONE/READY_BULLET` 均为位置 PID 持仓，`max_out` 使用 HOLD 限幅（10.0f）
  - READY_BULLET：目标点不跟随，低限幅位置环抑制 overshoot
  - BULLET：进入 `shoot_bullet_control()` 时恢复 `max_out=20000`，保证推弹力矩
  - 门控：零电流门控收窄为 `<= SHOOT_READY_FRIC`，允许 DONE/READY_BULLET 输出受限制动电流
  - 堵转：反转次数达到 `MAX_REVERSE_COUNT=3` 后放弃本次并回 `READY_BULLET`
  - 反转：`fn_2` 长按期间持续输出 `reverse_trigger`
  - 步进：`TRIGGER_ONEGRID` 回调为 `36864`（1/8 turn）
  - 热量：`local_heat` 预测 + 裁判热量校准 + `R` 键爆发模式
- 待办：Keil 编译 + 测试 0~8 回归（重点 4/5/8）+ 单发 20 次统计

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
- `application/shoot.h`：
  - `TRIGGER_POS_PID` = `0.4/0.02/0.0, max_out=20000, max_iout=1500`
  - `TRIGGER_SPD_PID` = `6.0/3.0/0.0, max_out=10000, max_iout=1000`
  - `TRIGGER_POS_MAX_OUT_HOLD=10.0, TRIGGER_POS_MAX_OUT_FIRE=20000.0, MAX_REVERSE_COUNT=3`
  - `TRIGGER_ONEGRID=36864, TRIGGER_POS_THRESHOLD=5000`
  - `HEAT_LIMIT_SAFE=80, HEAT_LIMIT_BURST=180, HEAT_COOL_RATE=12/s`

### VT03 UART 模式切换（USART1/USART6）
- 状态：In Progress（代码已落地，待三模式编译和实机链路验证）
- 最新文档：`2026-02-28_vt03_uart_mode_switching.md`
- 当前方案：
  - `CURRENT_UART_MODE` 统一切换 `DEBUG_WIFI / DEBUG_VT03 / COMPETITION`
  - `USART6` 在 referee 与 VT03 间切换（115200/921600）
  - `USART1` 在 WiFi 与 VT03 间切换（115200/921600）
  - VT03 解析模块输出复用 `rc_ctrl`，并上报 `VT03_TOE`
- 待办：Keil 三模式编译矩阵、mode_sw 映射核对、VT03 断链检测回归

### VT03 Keyboard Action（键鼠动作集中层）
- 状态：In Progress（v7 问题已开始收敛，仍需回归闭环）
- 最新文档：`2026-03-02_shoot_single_fire_reverse_grid_fix.md`
- 当前方案：
  - 新增 `application/keyboard_action.c/h` 统一输出 `keyboard_cmd_t`
  - `gimbal_task` 1ms 周期调用 `keyboard_action_update()`
  - `chassis/gimbal/shoot` 按命令消费，实现 DBUS/VT03 双源兼容
- 本轮补丁（v5）：
  - `shoot.c`：`READY_BULLET` 取消自动拨轮旋转；允许 `READY_BULLET` 直接响应 `trigger/mouse` 开火边沿
  - `shoot.c`：键盘发射辅助统一门控为 `s1=MID` 或 `VT03在线且s0!=DOWN`
  - `usb_task.c`：`event_bits` 新增 `bit8..20`（VT13 原始键状态 + keyboard_cmd 脉冲 + dial 方向 + s0门控位）
- MCP 侧状态（v6）：
  - `D:\tools\stm32-telemetry-mcp` 已支持 `event_bits -> evt_*` 虚拟通道展开
  - 可直接订阅：`evt_fn1_cur/evt_fn2_cur/evt_trigger_cur/evt_cmd_* /evt_dial_pos/evt_dial_neg/evt_s0_not_down`
- 本轮遥测结论（项目1~8，CSV证据已回填到 v7）：
  - 项目1/6/7通过；
  - 项目2~5出现“输入可见但执行链未闭环”；
  - 项目8失败（发射后速度越过 15~25m/s 安全区间）。
- 本轮补丁（2026-03-02 v2）：
  - `vt03_link.c`：`s[1]` 默认值 `DOWN -> MID`，消除 `shoot_set_mode` 假边沿。
  - `vt03_link.c`：`mouse.press_l/press_r` 强制清零，消除 VT13 `d[14]` 噪声导致的自动开火。
- 本轮补丁（2026-03-02 v3）：
  - `shoot.c`：移除 `shoot_bullet_control()` 内 `key==SWITCH_TRIGGER_OFF` 的提前 DONE 退出。
  - `shoot.h`：`TRIGGER_ONEGRID` 回调至 `36864`，与 1/8 分度一致。
- 本轮补丁（2026-03-02 v4）：
  - `shoot.c`：`READY_BULLET` 与 `SHOOT_DONE` 改为位置环计算 `speed_set`（目标点不跟随）。
  - `shoot.c`：零电流门控边界 `<= READY_BULLET -> < READY_BULLET`，保留 READY_BULLET 制动电流。
- 本轮补丁（2026-03-02 v5）：
  - `shoot.c`：`READY_BULLET` 恢复为零速度 + 位置跟随，制动不在 READY_BULLET 执行。
  - `shoot.c`：零电流门控恢复 `<= READY_BULLET`，避免 READY_BULLET 满电流堵转。
  - `shoot.c`：`shoot_set_mode` 的 DONE->READY_BULLET 增加低速判据 `fabs(speed)<0.3f` 与 `500ms` 兜底超时。
- 本轮补丁（2026-03-02 v6）：
  - `shoot.h`：新增 `TRIGGER_POS_MAX_OUT_HOLD/FIRE` 与 `MAX_REVERSE_COUNT`，`shoot_control_t` 新增 `reverse_count`。
  - `shoot.c`：`READY_BULLET` 与 `SHOOT_DONE` 改为低限幅位置 PID 持仓，`BULLET` 恢复全力矩 `max_out`。
  - `shoot.c`：零电流门控改为 `<= SHOOT_READY_FRIC`，并在堵转反转逻辑加入“最多 3 次后放弃本次”。 
- 待办：
  - 完成剩余 `dial` 脉冲化与 `vt03_trigger` 保持窗口改造；
  - 完成后按项目1~8逐项复测并更新状态。

## 风险日志对应关系

- MID 跟随自旋/振荡：`risk_log.md` 2026-02-23 条目（Mitigated，待长跑）
- UP 小陀螺平移画圆：`risk_log.md` 2026-02-24 条目（Mitigated，缺 VOFA+/长跑）
- Pitch 自适应前馈迭代：`risk_log.md` 2026-02-27 条目（In Progress，按规则 6.2 需先采数据）
- Shoot 拨轮双环与热量预测：`risk_log.md` 2026-02-28 条目（In Progress，待实机验收）
- Shoot 单发/反转/格数修复：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- VT03 假边沿/鼠标噪声自动开火：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- Shoot BULLET 微动提前退出：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- Shoot READY_BULLET overshoot 自由滑行：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- Shoot READY_BULLET 堵转满电流：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- Shoot 堵转反转超限与放弃策略：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
