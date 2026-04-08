# AI 会话记录：拨轮双环 + 热量预测/爆发模式落地
日期：2026-02-28

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-26_shoot_fric_can_closed_loop_migration.md`
- 数据来源：`CURRENT_STATE` 历史状态 + 本轮静态代码核对（未新增实机数据）
- 关键帧/时间段：无（本轮未执行板端采集）
- 状态：`In Progress -> In Progress`
- 差异：上轮完成摩擦轮 CAN 闭环；本轮新增拨轮双环控制与本地热量预测
- 下一步：完成 Keil 编译与板端单发/连发热量门控验收

## 1. 目标

- 按 `docs/plans/trigger_motor_optimization.md` 执行 Step1~Step3：
  - 新增增强型 PID（不影响原 PID）
  - 拨轮从单环速度改为位置+速度双环
  - 引入本地热量预测与爆发模式（R 键）
- 保持摩擦轮 CAN 闭环链路、CAN 发送链路与云台主任务结构不变。

## 2. 改动文件列表

- `components/controller/pid.h`
- `components/controller/pid.c`
- `application/shoot.h`
- `application/shoot.c`
- `application/usb_task.c`
- `docs/02_ARCHITECTURE.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/plans/trigger_motor_optimization.md`
- `docs/ai_sessions/2026-02-28_shoot_trigger_motor_optimization.md`

## 3. 关键改动摘要

- `components/controller/pid.h` / `pid.c`
  - 新增 `pid_enhanced_t`。
  - 新增 `PID_enhanced_init/calc/clear`，支持死区、变速积分、梯形积分、不完全微分滤波。
  - 原 `pid_type_def` 与 `PID_init/calc/clear` 未改动。

- `application/shoot.h` / `shoot.c`
  - 拨轮控制改为双环：
    - 外环 `trigger_pos_pid`（位置）
    - 内环 `trigger_spd_pid`（速度）
  - 新增连续编码器位置变量：`trigger_ecd_fdb/trigger_ecd_set`。
  - 单发模式改为“一格位置目标 + 位置到达判定”。
  - 连发模式保留速度直驱，按编码器位移累计发弹计数。
  - 新增本地热量 `local_heat`（每 ms 冷却 + 发弹加热 + 裁判值校准）。
  - 新增 `burst_mode`（R 键上升沿切换）并在比赛模式下按
    `HEAT_LIMIT_SAFE/HEAT_LIMIT_BURST` 门控射击。

- `application/usb_task.c`
  - 在线调参 `target=trigger` 从旧 `trigger_motor_pid` 改为新 `trigger_spd_pid`。
  - 新增 enhanced PID 参数读写适配函数，避免接口失效。

## 4. 关键决策与理由

- 决策 1：增强 PID 与原 PID 并存。
  - 理由：避免影响云台/底盘现有控制环，风险隔离到拨轮链路。
- 决策 2：保留连发速度直驱，不对连发引入位置环。
  - 理由：连发主要追求吞吐，保持原策略并仅补齐热量计数与堵转反转。
- 决策 3：热量门控只在 `SHOOT_DEBUG_MODE==0` 生效。
  - 理由：保留调试场景可控性，同时满足比赛模式硬约束。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增阻塞调用（无新增 `osDelay/vTaskDelay`）。
- CPU 影响：`shoot_control_loop()` 每周期新增常数级浮点计算（双环 PID + 热量更新 + 连发位移计数）。
- 栈影响：未新增大数组；`shoot_control_t` 扩容（新增双环状态与热量状态）。
- 帧率/周期：`gimbal_task` 1ms 周期不变，CAN 发送频率不变。

## 6. 风险点

- 双环参数与 `TRIGGER_ONEGRID/TRIGGER_POS_THRESHOLD` 若不匹配，可能导致少拨、过拨或卡弹恢复慢。
- 连发“编码器位移计弹”在强干扰/反转场景可能出现计数偏差，影响热量预测精度。
- 爆发模式允许高于 80 的预测热量窗口，若操作不当仍会触发裁判锁枪惩罚。

## ⚠ 未验证假设

- 假设：`TRIGGER_ONEGRID=36864` 与当前机构一发位移一致。
- 未验证原因：本轮未执行实机单发位移标定。
- 潜在影响：若步进量不准，会导致单发不到位或过拨。
- 后续验证计划：板端执行 20 次单发，记录拨轮编码器增量分布并回调阈值。

- 假设：连发位移累计计弹在堵转反转后仍能保持可接受误差。
- 未验证原因：未执行连发+堵转混合工况测试。
- 潜在影响：`local_heat` 与裁判热量偏差扩大，导致门控提前或滞后。
- 后续验证计划：连发 5s + 人工阻塞触发反转，比较 `local_heat` 与裁判热量差值。

## 7. 验证方式与结果

- 串口：未执行。
- USB：完成静态接口核对（`USB_PID_TARGET_TRIGGER` 可读写到 `trigger_spd_pid`）。
- 示波器：未执行。
- 上位机：未执行。
- 长跑：未执行。
- 编译：未在本会话内完成 Keil 编译（需用户环境验证）。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - `application/shoot.c/h` 回退到单环速度 PID + 原热量判断。
  - `components/controller/pid.*` 删除 `pid_enhanced_*` 增量。
  - `application/usb_task.c` 的 `target=trigger` 映射恢复到旧字段。
- 参数回滚点：
  - `TRIGGER_POS_*`、`TRIGGER_SPD_*`、`TRIGGER_ONEGRID`、热量阈值宏恢复到改动前版本。
- 运行时保护：
  - 回滚后先在 `SHOOT_STOP` 验证零电流，再切入 `READY_FRIC`/`READY_BULLET`。

## 9. 附件/证据

- 代码 diff：本会话改动文件列表所示。
- 关联风险条目：`docs/risk_log.md`（2026-02-28 新增 shoot 触发链路条目）。

## 10. 控制链路三问（规则 6.1）

1. 当前故障/需求的实际执行路径是什么？  
   需求路径是 `shoot_control_loop -> shoot_bullet_control -> trigger_motor_turn_back` 的拨轮控制链路，以及 `shoot_set_mode` 的热量门控路径；现状为单环速度 + 被动热量判断，难以满足 1v1 热量窗口约束。
2. 本次修改命中的是哪条路径？  
   直接命中上述两条路径：把拨轮链路替换为位置+速度双环，并把热量门控替换为 `local_heat` 预测 + 裁判校准 + 爆发模式阈值切换。
3. 如何用数据证明本次修改生效？  
   采集并对比 `trigger_ecd_set/trigger_ecd_fdb/speed_set/speed/given_current/local_heat/referee_heat`：验证单发到位误差、连发计弹一致性、热量门控触发点与阈值一致。

## v2: 审查意见收敛（2026-02-28）

### 改动摘要
- application/shoot.c：shoot_set_mode() 局部变量由 heat_limit 改名为 effective_limit，避免与 shoot_control.heat_limit 同名遮蔽。
- application/shoot.c：trigger_motor_turn_back() 堵转分支保持固定反向输出 speed_set = -fallback_speed，避免调用间方向翻转。
- application/shoot.h：为 TRIGGER_ONEGRID 增加推导注释：8192 * 36 * 45 / 360 = 36864。
- components/controller/pid.c：PID_enhanced_calc() 的变速积分缩放方向维持当前实现（误差越大积分越弱，超 I_U 停止新增积分）。

### 决策理由
- 变量重名会增加误读风险，尤其热量门控属于发射安全路径，必须消除歧义。
- 堵转恢复必须方向确定，翻转行为会引入“本次反转/下次正转”的非确定性。
- 常量推导注释用于减少后续参数维护门槛。
- 变速积分策略优先保守稳定：当前实现属于典型“大误差抑制积分”抗饱和策略，更符合堵转与快速误差场景的安全需求。

### ⚠ 未验证假设
- 假设：trigger_motor_turn_back() 固定反向在全部装配方向下都与“退弹”机械方向一致。
- 未验证原因：本轮未做实机堵转回退测试。
- 潜在影响：若机械方向定义与软件相反，可能导致堵转工况恢复变慢或无效。
- 后续验证计划：板端执行“人工堵转 -> 回退释放”测试，观察 speed_set、编码器增量和恢复时长是否符合预期。