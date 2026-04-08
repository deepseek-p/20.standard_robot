# 2026-02-26 WiFi Bridge USART6 + ESP32 + MCP 落地记录

> 后续迁移说明：`2026-02-26_wifi_bridge_migrate_to_usart1.md` 已将 WiFi 串口从 USART6 迁移到 USART1，本记录仅保留为历史阶段文档。

## 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-2-26_wifi_handoff_prompt2.md`
- 数据来源：代码方案交接文档（无新增实机数据）
- 关键帧/时间段：不适用（本轮为实现落地）
- 状态：Blocked - 需数据
- 差异：上轮仅方案设计，本轮完成 STM32/ESP32/MCP 代码实现
- 下一步：完成硬件联调采集后回填实测结果

## 1. 目标

- 将 USB 调参链路扩展为 USART6 + ESP32 WiFi 桥接链路。
- 保持 `WIFI_BRIDGE_ENABLE == 0` 时原裁判链路行为不变。

## 2. 改动文件列表

- `application/usb_task.c`
- `application/referee_usart_task.c`
- `application/wifi_bridge.h`（新增）
- `D:/tools/esp32_wifi_bridge/platformio.ini`（新增）
- `D:/tools/esp32_wifi_bridge/src/main.cpp`（新增）
- `D:/tools/stm32-telemetry-mcp/serial_manager.py`
- `D:/tools/stm32-telemetry-mcp/server.py`
- `docs/02_ARCHITECTURE.md`
- `docs/03_TASK_MAP.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`

## 3. 关键改动摘要

- `application/referee_usart_task.c`
  - 新增 `WIFI_BRIDGE_ENABLE` 条件分支。
  - WiFi 模式下 `referee_usart_task` 早退空转，不再调用 `usart6_init()`。
  - WiFi 模式下 `USART6_IRQHandler` 改为 RXNE + 环形缓冲（`uart6_rx_*`）。
  - 编译告警清理：`referee_unpack_fifo_data()` 在 `WIFI_BRIDGE_ENABLE=1` 下改为条件编译，仅在 `!WIFI_BRIDGE_ENABLE` 时声明/定义，消除“declared but never referenced”。
- `application/usb_task.c`
  - 新增 `wifi_uart6_init()`：清 `DMAR/DMAT`，关闭 IDLE，启 RXNE。
  - 新增 `wifi_cmd_process()` 独立行缓冲，避免与 USB 命令缓冲共享 static 状态。
  - 命令解析改为 `reply_fn` 路由：USB 命令回 USB，WiFi 命令回 UART6。
  - FireWater 遥测保留 USB 发送，同时在 WiFi 模式额外阻塞发送到 USART6。
- `application/wifi_bridge.h`
  - 集中定义 `WIFI_BRIDGE_ENABLE` 与 UART6 RX API 声明。
- `D:/tools/esp32_wifi_bridge/src/main.cpp`
  - AP 模式（SSID `TX`，密码 `2188630464`），禁用 WiFi 省电。
  - UART2 (GPIO16/17) 行级收包，按行分类：
    - `OK/ERR/DUMP` -> TCP:8402
    - 其余行 -> UDP:8401
  - TCP 单客户端策略（新连入踢掉旧连接），`setNoDelay(true)`。
  - UDP 默认子网广播 `192.168.4.255`，支持 PC 注册包后定向单播。
- `D:/tools/stm32-telemetry-mcp/serial_manager.py`
  - 新增 `wifi://` 端口识别与分发。
  - 新增 `wifi_capture()`（UDP 采集）与 `wifi_send_command()`（TCP 命令）。
  - 现有 `capture()/send_command()` 自动兼容串口与 WiFi。
- `D:/tools/stm32-telemetry-mcp/server.py`
  - `list_ports()` 增加 `wifi://192.168.4.1` 使用提示。

## 4. 关键决策与理由

- 决策 1：USART6 在 WiFi 模式使用 RXNE 中断 + 软件环形缓冲，不复用裁判 DMA 双缓冲。
  - 理由：避免 IDLE+DMA 与逐字节解析并存时的状态冲突。
- 决策 2：命令解析共享一套语义逻辑，但回复路径通过函数指针切换。
  - 理由：避免复制 `SET/GET/DUMP` 核心逻辑，减少后续维护分叉。
- 决策 3：WiFi 链路先采用阻塞式 `HAL_UART_Transmit`。
  - 理由：先保证路径稳定，避免 USART6 TX DMA 与命令回复竞争。
- 放弃方案：USART6 TX/RX 全 DMA 化。
  - 放弃理由：当前目标是先跑通调参闭环，DMA 仲裁和双通道缓存复杂度更高。

## 5. 实时性影响（必须写）

- 阻塞变化：
  - WiFi 模式新增 USART6 阻塞发送（`usb_task` 路径），会拉长 `usb_task` 周期。
- CPU 影响：
  - WiFi 模式新增 UART6 命令解析与中断逐字节入环形缓冲，开销主要在 `usb_task`。
- 栈影响：
  - `usb_task` 新增 WiFi 行缓冲与回复格式化路径，未新增栈水位实测数据。
- 帧率/周期：
  - 115200 波特率下 WiFi 遥测预期由 50Hz 降至约 18~22Hz（按方案估算）。

## 6. 风险点

- 未完成板级联调，`WIFI_BRIDGE_ENABLE=1` 下实机中断与串口链路稳定性未闭环。
- USB 未连接时，USB 命令回复路径仍可能发生重试等待（与 WiFi 路并行存在）。
- WiFi 环境拥塞时 UDP 丢包与 TCP 断连会影响采样连续性。

## ⚠ 未验证假设

- 假设：`USART6` 在 WiFi 模式下仅由 `wifi_uart6_init()` 配置，且不会被其他路径重新开启 DMA/IDLE。
- 未验证原因：当前未接入真实 C 板 + ESP32 + PC 三端硬件链路。
- 潜在影响：若初始化顺序被改写，可能再次出现 DMA/RXNE 混用冲突导致丢包或异常中断。
- 后续验证计划：由当前对话负责人在实机执行 4 组验证（上电、短连命令、持续采集、切换开关回归），并回填 CSV/日志证据。

## 7. 验证方式与结果

- 串口：
  - 未执行板级实测（缺硬件在线）。
- USB：
  - 未执行板级实测（缺硬件在线）。
- 示波器：
  - 未执行（缺硬件在线）。
- 上位机：
  - 已执行 `python -m py_compile` 验证 MCP 改动语法通过。
  - 已本地导入测试 `serial_manager` 的 `wifi://` 解析逻辑通过。
- 长跑：
  - 未执行（缺硬件在线）。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 将 `application/wifi_bridge.h` 中 `WIFI_BRIDGE_ENABLE` 置 `0`，恢复裁判链路。
  - 若需彻底回退，可还原：
    - `application/usb_task.c`
    - `application/referee_usart_task.c`
    - `application/wifi_bridge.h`
- 参数回滚点：
  - ESP32 项目独立目录可直接停用，不影响 STM32 USB 原链路。
  - MCP 端改动仅在 `port=wifi://...` 时触发，串口路径仍可继续使用。
- 运行时保护：
  - 上板切回前先断开 ESP32，仅保留原裁判线，确认 `WIFI_BRIDGE_ENABLE=0` 后再联调。

## 9. 附件/证据

- 代码证据：
  - `python -m py_compile D:/tools/stm32-telemetry-mcp/serial_manager.py D:/tools/stm32-telemetry-mcp/server.py`
- 相关风险条目：
  - `docs/risk_log.md` 中 2026-02-26 新增 WiFi 桥接条目
