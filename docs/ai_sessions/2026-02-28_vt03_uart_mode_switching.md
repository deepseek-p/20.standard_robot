# AI 会话记录：VT03 图传链路接入与 UART 三模式切换
日期：2026-02-28

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-26_wifi_bridge_migrate_to_usart1.md`
- 数据来源：`CURRENT_STATE` 历史结论 + 本轮静态代码核对（未新增实机数据）
- 关键帧/时间段：无（本轮未执行板端采集）
- 状态：`In Progress -> In Progress`
- 差异：上一轮完成 WiFi 桥接迁移至 USART1；本轮新增 VT03 帧解析与 UART 三模式编译期切换。
- 下一步：Keil 三模式编译矩阵验证 + VT03 实机收帧与 `VT03_TOE` 离线检测回归。

## 1. 目标

- 按 `docs/plans/2026-02-28-vt03-uart-mode-switching.md` 落地 Task1~Task7。
- 新增 VT03 遥控帧解析模块，支持 CRC16 校验与状态机解析。
- 通过 `CURRENT_UART_MODE` 统一切换 USART1/USART6 角色，实现 Debug/Competition 场景切换。

## 2. 改动文件列表

- `application/uart_mode.h`（新增）
- `application/vt03_link.h`（新增）
- `application/vt03_link.c`（新增）
- `application/detect_task.h`
- `application/detect_task.c`
- `application/referee_usart_task.c`
- `application/wifi_bridge.h`
- `application/remote_control.c`
- `Src/usart.c`
- `Src/main.c`
- `MDK-ARM/standard_robot.uvprojx`
- `docs/02_ARCHITECTURE.md`
- `docs/03_TASK_MAP.md`
- `docs/04_MODULE_MAP.md`
- `docs/05_BOARD_PHYSICAL_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-28_vt03_uart_mode_switching.md`
- `docs/plans/2026-02-28-vt03-uart-mode-switching.md`

## 3. 关键改动摘要

- `application/uart_mode.h`
  - 新增三模式宏：`UART_MODE_DEBUG_WIFI / UART_MODE_DEBUG_VT03 / UART_MODE_COMPETITION`。
  - 新增派生开关：`USART6_VT03 / USART6_REFEREE / USART1_VT03 / USART1_WIFI / VT03_ENABLE`。
  - 增加编译期冲突保护：比赛模式下禁止 `TELEM_MODE_WIFI`。

- `application/vt03_link.h/.c`
  - 新增 VT03 21 字节帧解析，RXNE 字节流状态机 + CRC16 校验。
  - 解包后写入 `rc_ctrl`（复用 `RC_ctrl_t` 数据通道）。
  - 解析成功后刷新 `detect_hook(VT03_TOE)`。

- `application/detect_task.h/.c`
  - 在枚举末尾新增 `VT03_TOE`。
  - 在 `set_item` 末尾新增 VT03 离/在线阈值配置行。

- `Src/usart.c`
  - `MX_USART1_UART_Init()` 按模式设置波特率（115200/921600）。
  - `MX_USART6_UART_Init()` 按模式设置波特率（115200/921600）。

- `application/referee_usart_task.c`
  - `USART6_IRQHandler` 条件分支：`VT03 RXNE` 或 `Referee DMA+IDLE`。
  - `referee_usart_task` 条件分支：`usart6_init(...)` 或仅启用 `UART_IT_RXNE`。
  - `USART1_IRQHandler` 条件分支：`VT03 RXNE` 或 `WiFi ring buffer`。

- `application/wifi_bridge.h`
  - `WIFI_BRIDGE_ENABLE` 改为受 `USART1_WIFI` + `TELEM_MODE_WIFI` 双条件控制。

- `Src/main.c`
  - `USART1_VT03` 模式下跳过 `usart1_tx_dma_init()`，启用 `USART1 RXNE + NVIC`。

- `application/remote_control.c`
  - 两处 `sbus_to_usart1` 发送条件增加 `!USART1_VT03` 防护。

- `MDK-ARM/standard_robot.uvprojx`
  - 新增 `application/vt03_link.c` 到 `applications` 组，确保 Keil 工程参与编译。

## 4. 关键决策与理由

- 决策 1：采用编译期宏切换 UART 角色，不做运行时自动切换。
  - 理由：中断路由、DMA、波特率与链路职责在编译期确定可降低竞态与误配置风险。

- 决策 2：VT03 解析结果直接复用 `rc_ctrl`。
  - 理由：减少上层行为模块改动面，保持云台/底盘/射击输入消费接口不变。

- 决策 3：保留 `VT03_TOE` 于 `errorList` 末尾新增。
  - 理由：避免改变既有设备枚举值顺序，降低索引错位风险。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增阻塞等待；VT03 路径使用 RXNE 字节中断 + 状态机。
- CPU 影响：VT03 模式下 UART ISR 每字节执行常数级解析逻辑；Referee 模式保持原 DMA+IDLE 路径。
- 栈影响：新增 `vt03_link.c` 静态缓冲与少量局部变量，任务栈配置未改。
- 帧率/周期：`referee_usart_task` 仍为 10ms 周期任务；USART1/USART6 中断触发路径按模式切换。

## 6. 风险点

- `mode_sw -> rc.s[0]` 编码可能与 DBUS 语义不完全一致，需实机映射确认。
- VT03 与 DBUS 同时在线时共享 `rc_ctrl`，存在“最后写入覆盖”风险。
- 竞赛模式若误开 WiFi 遥测会触发编译错误，需要同步配置 `TELEM_OUTPUT_MODE`。

## ⚠ 未验证假设

- 假设：VT03 的 `mode_sw` 与现有上层状态机对 `rc.s[0]` 的取值语义兼容。
- 未验证原因：本轮未接入 VT03 实机发送端执行模式切换测试。
- 潜在影响：模式挡位解释错误会导致云台/底盘行为模式与预期不一致。
- 后续验证计划：在 `CURRENT_UART_MODE=1/2` 下采集 `rc.s[0]`、`rc.ch[0..3]`、`key.v`，对照按键/拨杆动作逐项核验并必要时补映射表。

## 7. 验证方式与结果

- 串口：完成静态代码核对（USART1/USART6 波特率与 IRQ 分支均受模式宏控制）。
- USB：未执行板端联调。
- 示波器：未执行。
- 上位机：未执行。
- 长跑：未执行。
- 编译：未在本会话执行 Keil 编译；已更新 `uvprojx` 并完成静态符号检查。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 移除 `application/uart_mode.h` 与 `application/vt03_link.*`。
  - `referee_usart_task.c` 回退到单一路径（USART6 referee + USART1 WiFi）。
  - `usart.c/main.c/remote_control.c/wifi_bridge.h` 回退到原逻辑。
- 参数回滚点：
  - `CURRENT_UART_MODE` 回退到 `UART_MODE_DEBUG_WIFI`（默认）。
- 运行时保护：
  - 回滚后优先验证 `REFEREE_TOE` 与 `DBUS_TOE` 在线状态，再执行上层动作联调。

## 9. 附件/证据

- 代码 diff：本会话改动文件列表所示。
- 关联风险条目：`docs/risk_log.md`（2026-02-28 新增 VT03/UART 模式切换风险条目）。
