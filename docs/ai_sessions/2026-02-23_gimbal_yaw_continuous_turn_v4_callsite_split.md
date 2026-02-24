# 2026-02-23 Yaw 连续角移植 v4（调用点拆分）

## 1. 目标

- 本次要解决的问题：按锁定方案将 yaw 从环绕角链路迁移为连续角链路，避免 `rad_format` 语义导致的切模态突跳、set/get 不一致与控制失配。
- 预期可观测结果：yaw 支持连续旋转；`gimbal_set_control()` 与 `gimbal_control_loop()` 的 yaw 路径均切换到连续角 set/get；pitch 原路径保持不变。

## 2. 改动文件列表

- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `application/gimbal_behaviour.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-23_gimbal_yaw_continuous_turn_v4_callsite_split.md`

## 3. 关键改动摘要

- `application/gimbal_task.h`：新增 `GIMBAL_YAW_CONTINUOUS_TURN`、`GIMBAL_YAW_ERR_LIMIT`、`GIMBAL_YAW_CALI_FAKE_LIMIT`；在 `gimbal_motor_t` 新增 `continuous_absolute_angle/continuous_relative_angle` 及对应 set 字段。
- `application/gimbal_task.c`：新增 yaw 连续角 unwrap 状态与更新函数，保留原 `absolute/relative` 环绕角语义；在 `gimbal_feedback_update()` 中更新连续角反馈。
- `application/gimbal_task.c`：`gimbal_set_control()` 按调用点拆分 yaw/pitch；yaw 连续模式直接累加 `continuous_*_set` 并镜像环绕 set，pitch 维持原 `limit` 函数。
- `application/gimbal_task.c`：`gimbal_control_loop()` 按调用点拆分 yaw/pitch；yaw 在连续模式下调用新增 `gimbal_motor_*_control_nowrap()`。
- `application/gimbal_task.c`：新增 `gimbal_PID_calc_nowrap()`，误差使用 `fp32_constrain(err, -GIMBAL_YAW_ERR_LIMIT, GIMBAL_YAW_ERR_LIMIT)`。
- `application/gimbal_task.c`：`gimbal_mode_change_control_transit()` 的 yaw 三个分支（RAW/GYRO/ENCONDE）均同步连续角 set。
- `application/gimbal_task.c`：`get_gimbal_debug_snapshot()` 在连续模式下输出 yaw 连续角 set/get；`cmd_cali_gimbal_hook()` 连续模式下输出 yaw fake limit（`±1000`），宏关闭时恢复原逻辑。
- `application/gimbal_behaviour.c`：连续模式下跳过 yaw 校准步并直接进入 `GIMBAL_CALI_END_STEP`；`TURN_KEYBOARD` 180°掉头逻辑用 `#if !GIMBAL_YAW_CONTINUOUS_TURN` 包裹。

## 4. 关键决策与理由

- 决策 1：不改 `rad_format` 全局语义，改调用点并新增连续角字段。  
  理由：`chassis_task` 等外部消费者仍依赖 yaw 环绕角语义，直接改 `rad_format` 会扩大回归面。
- 决策 2：`limit` 与原控制函数签名不改，仅在 yaw 调用点切换连续链路。  
  理由：满足“yaw 连续化、pitch 零侵入”，并降低回退难度。
- 决策 3：模式切换分支强制同步连续 set。  
  理由：防止切模态瞬间 set/get 断层导致误差暴涨。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增 `osDelay/vTaskDelay`，调度周期不变。
- CPU 影响：每周期 yaw 新增常数级 unwrap 运算与若干分支判断，开销很小。
- 栈影响：未引入大对象，函数调用深度变化极小。
- 帧率/周期：`gimbal_task` 1ms 控制周期不变，CAN 发送节奏不变。

## 6. 风险点

- 连续模式依赖硬件满足连续旋转条件（滑环/走线）；误刷到有限绕线机体仍有绕线风险。
- 本次仅完成代码落地，未在实机完成“20 次切模态 + 往返 10 圈”回归，仍有参数与工况风险。

## 6.1 改码前三问（00 规则 6.1）

1. 当前故障的实际执行路径是什么？  
   用户给出的实机现象是“yaw 已可连续旋转，但旧版本切换与控制链路存在明显问题并已回退到官方基线”；结合代码路径，故障聚焦在 yaw 仍走环绕角 set/get 的 `gimbal_set_control()` 与 `gimbal_control_loop()` 两处调用点，以及 `gimbal_mode_change_control_transit()` 切模态同步不完整路径。
2. 本次修改命中的是哪条路径？  
   直接命中上述三条路径：调用点拆分（set/control_loop）+ 模式切换三分支连续 set 同步，并补齐 yaw feedback unwrap 与 snapshot 显示一致性。
3. 如何用数据证明本次修改生效？  
   观测 `get_gimbal_debug_snapshot()` 中 yaw 字段（连续模式下输出连续角 set/get）：
   - 连续单向旋转时 `yaw_absolute/yaw_relative` 单调连续；
   - 快速 MID/UP 切模态时 `yaw_*_set` 与 `yaw_*` 不出现阶跃性大偏差；
   - 输入存在时 `yaw_*_set` 持续变化，电机有同步响应，不出现“set 不变”。

## ⚠ 未验证假设

- 假设：当前机体硬件允许 yaw 连续旋转（滑环/走线/机械干涉满足）。
- 未验证原因：本次会话未连接实机，仅完成代码与文档改造。
- 潜在影响：若硬件不满足连续旋转，持续同向操作可能造成绕线或机构风险。
- 后续验证计划：按“抬轮静态 -> 落地低速 -> 正常机动”三阶段验证连续 10 圈正反转与 20 次切模态，记录温升、电流与 set/get 曲线。

## 7. 验证方式与结果

- 代码级验证：已完成关键函数分支和字段链路静态核对（set/control_loop/mode_change/snapshot/cali）。
- 编译验证：本次会话未在 Keil 执行完整编译。
- 实机验证：本次会话未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：回退 `application/gimbal_task.h`、`application/gimbal_task.c`、`application/gimbal_behaviour.c` 到本次改动前提交。
- 参数回滚点：将 `GIMBAL_YAW_CONTINUOUS_TURN` 置 `0`，恢复 yaw 环绕角控制链路。
- 运行时保护：首次上电先抬轮验证，确认无暴走后再落地测试。

## 9. 附件/证据

- 日志路径：本次未产出实机日志。
- 截图/波形：本次未产出示波器/上位机截图。
- 相关风险条目：`docs/risk_log.md`（新增 2026-02-23 yaw 连续角 v4 条目）。

