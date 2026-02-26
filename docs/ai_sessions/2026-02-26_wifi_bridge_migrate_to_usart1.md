# 2026-02-26 WiFi Bridge 迁移到 USART1

## 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-26_wifi_bridge_usart6_esp32_mcp.md`
- 数据来源：用户编译结果（`standard_tpye_c.axf`，0 error/1 warning）
- 关键帧/时间段：编译日志中 `referee_usart_task.c` 的未引用函数告警
- 状态：Partial
- 差异：上轮 WiFi 桥接实现绑定在 USART6，本轮迁移到 USART1（板上 UART2 4pin 实际映射）
- 下一步：重新编译并进行串口/WiFi 实机链路回归

## 1. 目标

- 将 WiFi 桥接接收与发送链路从 USART6 迁移到 USART1。
- 保持 `WIFI_BRIDGE_ENABLE==0` 时裁判系统 USART6 行为不变。
- 处理 `remote_control.c` 中 `usart1_tx_dma_enable()` 与 WiFi RXNE 模式的冲突风险。

## 2. 改动文件列表

- `application/referee_usart_task.c`
- `application/usb_task.c`
- `application/wifi_bridge.h`
- `application/remote_control.c`
- `docs/02_ARCHITECTURE.md`
- `docs/03_TASK_MAP.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`

## 3. 关键改动摘要

- `application/referee_usart_task.c`
  - `USART6_IRQHandler` 恢复为纯裁判 `IDLE + DMA 双缓冲` 逻辑（不再带 WiFi 分支）。
  - 新增 `USART1_IRQHandler`（仅 `WIFI_BRIDGE_ENABLE=1` 时编译），执行 RXNE 入环形缓冲。
  - WiFi 缓冲 API 从 `uart6_rx_*` 改为 `uart1_rx_*`。
- `application/usb_task.c`
  - `wifi_uart6_init()` 改为 `wifi_uart1_init()`。
  - WiFi 命令接收改为读取 `uart1_rx_available()/uart1_rx_read_byte()`。
  - WiFi 命令回复与遥测转发串口改为 `HAL_UART_Transmit(&huart1, ...)`。
  - WiFi 初始化阶段新增 `USART1_IRQn` 使能（priority 5）。
- `application/wifi_bridge.h`
  - API 声明统一改为 `uart1_rx_available/read_byte`。
- `application/remote_control.c`
  - 新增 `wifi_bridge.h` 引用。
  - WiFi 模式下屏蔽 `sbus_to_usart1()` 调用，避免 `wifi_uart1_init()` 清 DMAT 后的无效 DMA 发送路径。

## 4. 关键决策与理由

- 决策 1：WiFi 中断入口迁移到 `USART1_IRQHandler`，裁判 `USART6_IRQHandler` 不混用。
  - 理由：按板级物理映射接线，且避免不同业务共享同一 IRQ 逻辑。
- 决策 2：在 WiFi 初始化中显式使能 `USART1_IRQn`。
  - 理由：CubeMX 当前未为 USART1 开启 NVIC，中断路径否则不生效。
- 决策 3：WiFi 模式禁用 `sbus_to_usart1` 转发。
  - 理由：WiFi 模式下 USART1 用于桥接链路，继续走 DMA TX 转发会与桥接用途冲突，且 DMAT 被清除后该路径不再可靠。

## 5. 实时性影响（必须写）

- 阻塞变化：
  - 维持原 WiFi 模式阻塞发送策略，仅发送口从 USART6 改为 USART1。
- CPU 影响：
  - WiFi RX 中断从 USART6 切到 USART1，单字节入环形缓冲策略不变。
- 栈影响：
  - `usb_task` 栈行为无新增大对象；未新增栈水位观测点。
- 帧率/周期：
  - 波特率与发送模式未变，WiFi 遥测速率预期与迁移前同级（约 18~22Hz）。

## 6. 风险点

- USART1 现与 WiFi 桥接复用，WiFi 模式下原 `sbus_to_usart1` 旁路被关闭，依赖该旁路的外设将不可用。
- 未完成实机联调，尚未验证 USART1 RXNE 中断在长期运行下的稳定性与丢帧率。

## ⚠ 未验证假设

- 假设：WiFi 模式下关闭 `sbus_to_usart1` 不影响当前机器人主功能闭环。
- 未验证原因：缺少“依赖 USART1 SBUS 转发”的实机工况回放数据。
- 潜在影响：若有外部设备依赖该转发链路，WiFi 模式下该设备将收不到数据。
- 后续验证计划：上板后分别在 `WIFI_BRIDGE_ENABLE=0/1` 执行遥控、裁判、WiFi 调参链路回归，并记录差异。

## 7. 验证方式与结果

- 串口：
  - 代码级检查通过：WiFi RX API 全量替换为 `uart1_rx_*`，USART6 WiFi分支已清除。
- USB：
  - 未做板级实测。
- 示波器：
  - 未做板级实测。
- 上位机：
  - 未做板级实测；本轮无 MCP/ESP32 代码修改。
- 长跑：
  - 未做板级实测。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 直接回滚本次四个代码文件：`referee_usart_task.c`、`usb_task.c`、`wifi_bridge.h`、`remote_control.c`。
- 参数回滚点：
  - 维持 `WIFI_BRIDGE_ENABLE=0` 可回到原裁判+USB链路。
- 运行时保护：
  - 上板前先确认线束在“UART2(4pin)->ESP32”路径，裁判线单独回接 USART6。

## 9. 附件/证据

- 用户编译日志（本轮输入）：0 error, 1 warning（迁移前）。
- 相关风险条目：`docs/risk_log.md` 2026-02-26 WiFi 桥接条目。
