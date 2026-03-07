# CURRENT_STATE（收纳页）

同步日期：`2026-03-07`

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
- 状态：**Passed**（空载 + 实弹验证通过，2026-03-07）
- 最新文档：`2026-03-07_shoot_hust_validation_and_fixes.md`
- 当前方案：
  - 摩擦轮：双 3508 CAN 速度闭环
  - 拨轮：`trigger_pos_pid`（位置外环）+ `trigger_spd_pid`（速度内环）
  - 单发：`SHOOT_BULLET` 位置步进一格后回 `SHOOT_READY_BULLET`
  - 连发：`SHOOT_CONTINUE_BULLET` 跳过位置环，速度环直驱（`3500/4500 rpm`）
  - 反转：`reverse` 边沿触发一格回退（无旧堵转状态机）
  - 目标管理：`trigger_ecd_set` 在 `SHOOT_STOP/SHOOT_READY_FRIC` 回贴反馈，其余 armed 态保持持仓目标
  - 状态集：`STOP/READY_FRIC/READY_BULLET/BULLET/CONTINUE_BULLET`（5 态）
  - 步进：`TRIGGER_ONEGRID=36864`（1/8 turn）
  - 热量：`local_heat` 预测 + 裁判热量校准 + `R` 键爆发模式
  - 遥测现状：`shoot_mode/given_current/speed/speed_set/trigger_ecd_fdb/trigger_ecd_set/local_heat/bullet_cnt/trigger_sw/reverse_flag/referee_heat` 已可观测
- 待办：完成 Keil 编译与空载 12 项验收（静态、单发、连发、反转、热量门控）；空载未闭环前继续禁止实弹
- 2026-03-06 v4 补充：
  - 本地代码已落地 HUST 风格“命令层 + executor”重构，新增 `application/shoot_logic.c/h`
  - `trigger_ecd_set` 现在由 executor 独占，手动反转改为 one-grid reverse recover
  - 长按 `reverse_trigger=1` 已增加边沿门控，避免连续多格回退
  - `given_current` 已增加 `±8000` 末端限流
  - FireWater 代码层已补入 `trigger_sw/reverse_flag/referee_heat`
  - 主机侧 `tests/shoot_logic_test.c` 已通过，覆盖 idle hold、single fire、manual reverse 和 reverse 边沿门控
  - 但当前仍未完成 Keil 编译、烧录和新固件板端空载 20 发复验，所以状态不能改判为 `Passed`
- 2026-03-06 v5 补充：
  - `COM22` 板端实测已持续输出 `67` 列 FireWater 帧
  - `D:\tools\stm32-telemetry-mcp\frame_parser.py` 原先仍按 `65` 列解析，导致 `capture()` 返回 `0` 帧
  - 本轮已将 parser 补齐到 `67` 列，并新增 `trigger_sw/reverse_flag/referee_heat` 映射
  - 修复后用 `stm32-telemetry-mcp` 试采 `COM22`，`2.5s` 内拿到 `123` 帧
  - 当前板端验收阻塞点已不再是串口/工具链，而是尚未完成新的空载 20 发采集
- 2026-03-06 v6 补充：
  - 新固件上板后执行 READY 静态 `5s` 复验，证据见 `data/tune_2026-03-06_shoot_hust_acceptance_live_003_static_ready.csv`
  - `shoot_mode` 全程 `2(READY_BULLET)`，`trigger_ecd_set` 全程固定 `0`
  - 但 `trigger_ecd_fdb` 在 `-90618 ~ 96767` 间往返，误差符号翻转 `41` 次
  - `trigger_cur` 全程饱和在 `±8000`，`trigger_spd_set` 全程打满 `±8000`
  - `trigger_sw=0`、`reverse_flag=0` 全程未变化，说明 READY 自激并非微动或反转路径触发
  - 因 READY 静态门再次失败，空载单发 20 次与实弹继续禁止
- 2026-03-06 v7 补充：
  - 根因已进一步收敛到“未就绪阶段 stale setpoint”
  - 证据是工具恢复后的 STOP 基线中，`trigger_ecd_fdb=-258020` 但 `trigger_ecd_set=0`
  - 说明 executor 在 `STOP/READY_FRIC` 阶段只在首次进入 idle hold 时贴目标，早期无效反馈值会被一直带到 `READY_BULLET`
  - 已在 `application/shoot_logic.c` 中改为：`!arm_enable || !fric_ready` 时每周期执行 `trigger_ecd_set = trigger_ecd_fdb`
  - 新增主机侧回归测试 `test_disarmed_or_wait_fric_keeps_target_attached_to_latest_feedback()`，用于防止启动阶段再次锁住旧 setpoint
  - 当前仍待烧录并重做 READY 静态 5s 复验，验证该修复是否足以消除自激
- 2026-03-06 v8 补充：
  - 本轮修复烧录后，STOP 基线板端复验已通过，证据见 `data/tune_2026-03-06_shoot_hust_acceptance_live_004_postfix_stop_baseline.csv`
  - `shoot_mode=0` 全程稳定，`trigger_ecd_fdb = trigger_ecd_set = -4042`
  - `fdb-set` 全程 `0`，`trigger_cur=0`，说明 stale setpoint 已不再停在旧值
  - 下一步只剩重新执行 READY 静态 `5s`，判断持仓自激是否已一并消失
- 2026-03-06 v9 补充：
  - 修复后 READY 静态 `5s` 复验已通过，证据见 `data/tune_2026-03-06_shoot_hust_acceptance_live_005_ready_static_after_fix.csv`
  - `shoot_mode` 正常过渡到 `READY_BULLET` 后稳定，`trigger_ecd_fdb = trigger_ecd_set = -4041`
  - `trigger_cur/trigger_spd/trigger_spd_set` 全程为 `0`，不再出现静态持仓自激
  - `trigger_sw=0`、`reverse_flag=0` 全程稳定
  - 当前门禁已从“禁止进入单发”推进到“允许进入空载单发 20 次”
- 2026-03-06 v10 补充：
  - 修复后空载单发 `5` 发预验收失败，证据见 `data/tune_2026-03-06_shoot_hust_acceptance_live_006_empty_single_5shots.csv`
  - 通过项：`bullet_cnt` `0->5`，每次只 `+1`；未观测到 `CONTINUE_BULLET`；`READY_BULLET` 内未观察到周期性 setpoint 回写
  - 失败项：单发后 `trigger_cur` 在各发间恢复区间持续饱和 `±8000`，未回到低电流；`trigger_ecd_fdb-trigger_ecd_set` 峰值明显劣于基线 `data/tune_2026-03-06_003.csv`
  - 当前同口径前 `5` 发 `peak|fdb-set|` 最大值 `99616`，基线前 `5` 发为 `53924`
  - 因动态空载门未通过，后续 `15` 发与实弹继续禁止
- 2026-03-06 v11 补充：
  - 已按 HUST 单发收尾语义再次修正 executor
  - 当前实现不再在 `|pos_err| < 5000` 时立刻回贴当前位置，而是：
    - `5000` 仅作为本发动作计数阈值
    - 只有在输入已释放且 `|pos_err| < 1000` 时，才 `DONE + 回贴当前位`
  - 主机侧新增 2 条回归测试，覆盖“不要过早 DONE”和“按住 trigger 不得提前 DONE”
  - 当前待烧录并重新执行空载单发 `5` 发预验收
- 2026-03-06 v12 补充：
  - 已烧录 HUST 收尾语义修复，并完成空载单发 `5` 发复验，证据见 `data/tune_2026-03-06_shoot_hust_acceptance_live_007_empty_single_5shots_hust_finish.csv`
  - 通过项：`bullet_cnt` `0->5`，每次只 `+1`；未观测到 `CONTINUE_BULLET`；`READY_BULLET` 内未观察到周期性 setpoint 回写
  - 改善项：前 `5` 发 `peak|fdb-set|` 最大值由 `99616` 回落到 `83377`，平均值由 `76977.6` 回落到 `73486.6`
  - 未通过项：相对基线 `data/tune_2026-03-06_003.csv` 仍明显恶化；各发恢复区间 `trigger_cur` 仍几乎全程饱和 `±8000`；`SHOOT_DONE` 未稳定显式出现
  - 当前判定仍为 `Failed`，继续禁止空载 `20` 发扩测与实弹
- 2026-03-06 v13 补充：
  - 已完成 shoot executor 集成层 6 项修复：
    - 持仓入口双 PID 清零
    - jam request flag 化，移除 `shoot.c` 对 executor / command 内部字段的越权赋值
    - 连发计弹移入 executor
    - `trigger_ecd_set/fdb/last_fire` 改为 `int32_t`
    - 删除 `SHOOT_READY` 死代码但保持 `shoot_mode` 数值兼容
    - `single_fire_req` 消费后显式清零
  - 主机侧新增 3 条测试并已全绿：
    - jam abandon 回 idle hold
    - continuous run one-grid 计弹
    - single_fire_req 消费后清零
  - 当前仍未重新烧录板端，因此 Shoot 总状态保持 `Failed`，下一步是复烧后重做空载 `5` 发

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
  - `TRIGGER_SPD_PID` = `6.2/3.2/0.0, max_out=10000, max_iout=1000, deadzone=50`
  - `TRIGGER_CAN_CURRENT_LIMIT=10000`（与 HUST speed PID OutMax 对齐）
  - `TRIGGER_ONEGRID=36864, TRIGGER_POS_THRESHOLD=5000`
  - `FRIC_SPEED_LOW=4900, FRIC_SPEED_HIGH=7400`（限 25m/s 弹速）
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
- Shoot 状态机 HUST 对齐：`risk_log.md` 2026-03-06 条目（Open，空载单发后 `READY_BULLET` 持仓持续自激）
- Shoot 单发/反转/格数修复：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- VT03 假边沿/鼠标噪声自动开火：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- Shoot BULLET 微动提前退出：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- Shoot READY_BULLET overshoot 自由滑行：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- Shoot READY_BULLET 堵转满电流：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- Shoot 堵转反转超限与放弃策略：`risk_log.md` 2026-03-02 条目（In Progress，待回归）
- Shoot HUST 控制核心替换：`risk_log.md` 2026-03-07 条目（In Progress，待 Keil/板端复验）

- 2026-03-06 v14 补充：
  - 已按 READY_BULLET 极限环根因收窄 hold 阶段位置环限幅：
    - `TRIGGER_POS_MAX_OUT_HOLD = 15.0f`
    - `TRIGGER_POS_MAX_IOUT_HOLD = 10.0f`
  - `SHOOT_READY_BULLET/SHOOT_DONE` 分支已同步切换 `max_out/max_iout`
  - `shoot_bullet_control()` 中 fire 阶段已恢复 `max_out/max_iout`
  - 主机侧既有测试继续通过
  - 当前仍未重新烧录板端，因此 Shoot 总状态继续保持 `Failed`

- 2026-03-07 v16 补充：
  - 板端验证完成，Shoot 状态从 `In Progress` 升级为 `Passed`
  - 测试通过项：STOP 基线、Enable→Ready、单发、连发、反转、实弹
  - 修复 3 项：
    1. 移除 Butterworth 速度滤波器（振荡根因）
    2. VT03 trigger 改用 `vt03_ext.trigger` 原始持续状态（连发无法触发根因）
    3. `FRIC_SPEED_HIGH` 8000→7400（限 25m/s 弹速）
  - 累计发射 137 发（空载+实弹），无异常
  - 详见 `docs/ai_sessions/2026-03-07_shoot_hust_validation_and_fixes.md`
