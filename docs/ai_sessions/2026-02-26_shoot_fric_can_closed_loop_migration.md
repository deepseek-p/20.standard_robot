# 2026-02-26 shoot fric can closed-loop migration

## 1. 目标

- 将摩擦轮控制从 `TIM1 PWM 开环` 迁移为 `hcan2 + C620 + M3508` 的 CAN 闭环速度控制。
- 保持拨弹电机（`0x207`）逻辑不变，仅替换摩擦轮控制链路与发射状态机中的“就绪判据”。

## 2. 上轮验收结论（先回填）

- 上轮会话：`docs/ai_sessions/2026-02-25_pitch_encode_pid_kp24_ki0_finalize.md`
- 数据来源：`口述`
- 关键结论：上一轮底盘/云台联动口述为“现象正常，可边转边走”；本轮未涉及该链路。
- 状态：`Passed -> Mitigated（待长跑）`
- 下一步：本轮聚焦发射链路改造，需单独做实机验收。

## 3. 改动文件列表

- `application/CAN_receive.h`
- `application/CAN_receive.c`
- `application/shoot.h`
- `application/shoot.c`
- `application/gimbal_task.c`
- `docs/ai_sessions/2026-02-26_shoot_fric_can_closed_loop_migration.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`

## 4. 关键改动摘要

- `application/CAN_receive.h`
- 新增摩擦轮 CAN ID 枚举：`CAN_FRIC_ALL_ID=0x200`、`CAN_FRIC1_MOTOR_ID=0x201`、`CAN_FRIC2_MOTOR_ID=0x202`。
- 新增接口声明：`CAN_cmd_fric()`、`get_fric1_motor_measure_point()`、`get_fric2_motor_measure_point()`。

- `application/CAN_receive.c`
- 新增 `motor_fric[2]` 接收缓存与 `fric_tx_message/fric_can_send_data` 发送缓存。
- `HAL_CAN_RxFifo0MsgPendingCallback()` 改为按总线区分：
- `hcan1` 只接收底盘 `0x201~0x204` 到 `motor_chassis[0..3]`（保留 `detect_hook`）。
- `hcan2` 接收 `0x205~0x207` 到 `motor_chassis[4..6]`（保留 `detect_hook`），接收 `0x201/0x202` 到 `motor_fric[0/1]`（不挂离线检测）。
- 新增 `CAN_cmd_fric()`：通过 `GIMBAL_CAN(hcan2)` 发送 `StdId=0x200`，前 4 字节写入双摩擦轮电流。
- 新增双摩擦轮测量指针 getter。

- `application/shoot.h`
- 删除 PWM/ramp 相关宏与结构体字段。
- 新增键鼠映射宏：`Q` 开关切换、`C` 高射频切换、`F/SHIFT+F` 弹速微调、`G` 手动反转。
- 新增摩擦轮 PID 宏与速度档位宏（`4900/5800/8000 RPM`）及就绪阈值宏。
- `shoot_control_t` 新增：双摩擦轮测量指针、双速度 PID、目标/基础转速、双电流输出、高射频与按键边沿状态。
- 新增 `shoot_get_fric_current()` 声明。

- `application/shoot.c`
- 删除 `bsp_fric.h` 依赖及全部 PWM/ramp 控制代码。
- `shoot_init()`：接入摩擦轮测量指针，初始化双摩擦轮速度 PID，初始化新状态字段。
- `shoot_set_mode()`：
- 保留遥控拨杆主流程；
- 键鼠模式改为 `Q/C/F` 边沿逻辑；
- `SHOOT_READY_FRIC -> SHOOT_READY_BULLET` 改为基于实际 `speed_rpm` 就绪判据；
- 连发状态根据 `high_freq_flag` 动态放大 `trigger_speed_set`；
- `shoot_control_loop()` 中新增 `G` 键按住反转拨弹。
- `shoot_control_loop()`：摩擦轮改为速度闭环电流输出（`fric1` 正转，`fric2` 反转），`SHOOT_STOP` 下目标转速置 0。
- 新增 `shoot_get_fric_current()` 供云台任务发送 CAN。

- `application/gimbal_task.c`
- 在云台主循环增加 `shoot_get_fric_current()` 读取。
- 发送链路改为：
- 正常：`CAN_cmd_gimbal(...)` + `CAN_cmd_fric(...)`
- `DBUS` 掉线或云台/拨弹电机异常：两路均发送零电流。

## 5. 改码前三问（6.1）

1. 当前故障/需求路径是什么？
- 旧链路摩擦轮为 PWM 开环，状态机“到速判据”来自 ramp，不是电机真实速度。

2. 本次修改命中哪条路径？
- 直接替换摩擦轮控制链路为 CAN 闭环，状态机就绪判据改为 `speed_rpm` 真实反馈，发射 CAN 发送路径从单帧扩展为双帧（`0x1FF + 0x200`）。

3. 如何用数据证明生效？
- 验证 `shoot_mode` 从 `SHOOT_READY_FRIC` 进入 `SHOOT_READY_BULLET` 的触发由 `speed_rpm` 阈值决定；
- 观测 `hcan2` 上摩擦轮反馈 `0x201/0x202` 与发送 `0x200` 电流闭环一致；
- 观测 `SHOOT_STOP` 时摩擦轮目标转速为 0，电流自然衰减。

## 6. 关键决策与理由

- 不改动拨弹 2006 链路，仅替换摩擦轮链路，降低改动耦合风险。
- `CAN_receive` 中对 `0x201/0x202` 必须按总线分流，否则会与底盘 `hcan1` 的同 ID 反馈混淆。
- 保持 `bsp_fric.*` 文件存在但不引用，满足“可回溯”和“最小侵入”。

## 7. 实时性影响

- 新增 `hcan2` 每周期一帧发送（`CAN_cmd_fric`），与原 `CAN_cmd_gimbal` 组合为两帧连续发送。
- `shoot_control_loop()` 增加双摩擦轮 PID 计算，均为常数阶运算，无阻塞路径。
- 未新增 `osDelay/vTaskDelay`，任务周期保持不变。

## 8. 风险点

- 摩擦轮方向若实装与代码符号不一致，会出现双轮同向转动，导致无法夹弹。
- `F/SHIFT+F` 在线调速若过快调整，可能超出当前机械/弹道最佳区间。
- 当前未把摩擦轮纳入 `detect_task` 离线判断，实机需确认该策略是否满足安全要求。

## ⚠ 未验证假设

- 假设：`fric1` 正转、`fric2` 反转与当前机械装配方向一致。
- 未验证原因：本轮仅完成代码迁移，尚未做实机上弹与射击验证。
- 潜在影响：若方向反了，摩擦轮无法正常出弹且可能加剧卡弹。
- 后续验证计划：上电后先空转验证方向，再做 `READY_FRIC -> READY_BULLET` 到速验证与实弹连发，负责人 `@Codex`。

## 9. 验证方式与结果

- 代码静态验证：
- 已确认旧 PWM/ramp 关键符号在 `shoot.c/h` 中清零；
- 已确认 `CAN_cmd_fric`、摩擦轮测量 getter、`shoot_get_fric_current` 均已接线。
- 实机验证：`未执行`（需烧录后板级测试）。

## 10. 回滚方式

- 回滚点：
- `application/shoot.c`/`application/shoot.h` 恢复到 PWM ramp 版本；
- `application/CAN_receive.c`/`application/CAN_receive.h` 删除 fric 分流与 fric 发送接口；
- `application/gimbal_task.c` 删除 `CAN_cmd_fric` 发送。
- 运行时保护：若联调异常，可先保留本版代码并强制 `CAN_cmd_fric(0,0)` 快速止损。

