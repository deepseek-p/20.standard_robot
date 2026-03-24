# 2026-03-24 功率限制与裁判客户端 UI

## 1. 目标

- 将 `main` 里的底盘功率预测限流思路移植到当前分支。
- 范围只保留“功率预测模型 + 功率限制”本体，不移植超级电容相关逻辑。
- 新增裁判客户端 UI，显示：
  - 底盘模式：`FOLLOW / SPIN`
  - 发射挡位：`LOW / HIGH`
  - 退弹成功绿灯闪烁
  - 位于屏幕高度约 `44%` 的固定准心

## 2. 改动文件列表

- `application/chassis_power_control.c`
- `application/chassis_power_control.h`
- `application/shoot.c`
- `application/shoot.h`
- `application/rm_ui.c`
- `application/rm_ui.h`
- `application/referee_usart_task.c`
- `MDK-ARM/standard_robot.uvprojx`
- `docs/INDEX.md`
- `docs/02_ARCHITECTURE.md`
- `docs/03_TASK_MAP.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`

## 3. 关键改动摘要

- `chassis_power_control`
  - 用模型功率估算替代旧的总电流直接缩放。
  - 保留裁判 `power/buffer` 读数、离线 fallback 和低 buffer 保护。
  - 不引入 supercap 读写、状态机或额外任务耦合。
- `shoot`
  - 增加 `shoot_get_ui_gear()`，只读导出 `LOW/HIGH`。
  - 增加 `shoot_consume_reverse_success_pulse()`，把退弹完成做成一次性 UI 事件。
- `rm_ui`
  - 新增客户端 UI 发送模块，负责 `0x0301` 组帧、CRC、初始化重建和低频刷新。
  - 初次在线时依次发送：delete all、reticle、mode text、gear text。
  - 退弹成功后闪烁绿色指示约 `800ms`。
- `referee_usart_task`
  - 不改接收解包职责，只在 10ms 循环里额外挂 `rm_ui_update()`。

## 4. 关键决策与理由

- 不新建 UI task。
  - 直接复用现有 `REFEREE` 任务，避免引入新的串口发送调度点。
- 不移植 supercap。
  - 用户明确限定只要功率预测模型和限流本体。
- UI 只做低频刷新。
  - 模式/挡位变化频率很低，退弹闪烁也不需要高频重绘；限到 `100ms` 可以减少阻塞发送对裁判接收的影响。
- 底盘模式折叠为两态。
  - 需求只保留 `FOLLOW/SPIN`，因此通过 `chassis_mode + wz_set` 做最小判定，不扩展更多 UI 状态。

## 5. 实时性影响

- 阻塞变化：
  - 新增 `HAL_UART_Transmit(&huart6, ...)` 阻塞发送路径。
  - 发送频率由 `rm_ui.c` 内部限到 `100ms`，每次最多发一个 UI 包。
- CPU 影响：
  - 新增少量协议组帧、字符串拷贝和状态机判断。
  - 未侵入 1ms `gimbal_task` 或 2ms `chassis_task` 主控制路径。
- 栈影响：
  - `rm_ui_send_interactive()` 使用本地 `frame[128]` 缓冲，实际栈占用上升发生在 `REFEREE` 任务。
  - 当前未做板端 stack watermark 复测。
- 周期影响：
  - 无任务周期、优先级、创建数量变化。

## 6. 风险点

- 裁判 UI 发送仍是阻塞 TX，若实机链路窗口紧张，可能影响 `REFEREE` 任务单次循环时间。
- 功率预测模型常数来自参考实现，尚未按本车实测重新标定。
- 客户端颜色枚举与图元表现目前按官方串口协议实现，仍需板端确认显示风格与坐标是否满足现场预期。

## ⚠ 未验证假设

- 假设：当前协议颜色枚举和图元打包方式与裁判系统实机版本兼容。
- 未验证原因：本次会话没有接入裁判系统客户端做实时绘制确认。
- 潜在影响：可能出现颜色不符、图元不显示或重建顺序异常。
- 后续验证计划：
  - 在板端联机观察初次上线、掉线重连和退弹闪烁三种场景。
  - 若出现异常，优先核对 `0x0100/0x0101/0x0103/0x0110` 的实机兼容性与颜色枚举。

## 7. 验证方式与结果

- 主机语法检查：
  - `gcc -fsyntax-only ... application/rm_ui.c`
  - 结果：通过。
- 主机整仓编译替代检查：
  - `gcc -fsyntax-only ... application/referee_usart_task.c`
  - 结果：失败，但失败点位于仓库现有 FreeRTOS ARM/RVDS 移植层和宿主机工具链不兼容，不是本次 UI 改动新引入的语法错误。
- 板端验证：
  - 尚未执行。

## 8. 回滚方式

- 代码回滚点：
  - 删除 `application/rm_ui.c/h`
  - 去掉 `application/referee_usart_task.c` 中的 `rm_ui_init()/rm_ui_update()`
  - 恢复 `MDK-ARM/standard_robot.uvprojx` 的文件列表
- 功率限流回滚点：
  - 恢复 `application/chassis_power_control.c/h` 到旧的电流缩放版本
- 运行时保护：
  - 若 UI 异常影响裁判链路，优先临时注释 `rm_ui_update()`，不动接收解包主链。

## 9. 附件/证据

- 设计文档：`docs/plans/2026-03-24-power-limit-ui-design.md`
- 实施计划：`docs/plans/2026-03-24-power-limit-ui-implementation.md`
- 风险日志：`docs/risk_log.md`
- 官方协议参考：
  - `RoboMaster 裁判系统串口协议附录 V1.5（2023-07-07）`
