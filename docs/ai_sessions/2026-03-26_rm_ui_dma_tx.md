# 2026-03-26 `rm_ui` DMA 非阻塞发送改造

## 目标

将 `application/rm_ui.c` 中裁判 UI 帧发送从阻塞式 `HAL_UART_Transmit()` 改为 `USART6 TX DMA` 非阻塞发送，避免 `REFEREE` 任务在发送 UI 帧时卡住约 10ms，拖慢同任务内的 `referee_unpack_fifo_data()`。

## 上轮验收结论

- 上轮会话：`2026-03-24_power_limit_ui.md`
- 数据来源：口述 + 代码排查
- 关键现象：用户反馈“遥测出现卡死情况”，结合当前实现确认 `rm_ui_send_interactive()` 使用 `HAL_UART_Transmit(&huart6, ..., 20)` 阻塞发送。
- 状态：`In Progress`
- 差异：本轮不改 UI 状态机和刷新间隔，只改 `USART6` 发送路径。
- 下一步：板端验证裁判链路掉线率是否下降，以及 UI 是否仍可正常刷新。

## 改动文件列表

- `application/rm_ui.c`
- `docs/ai_sessions/2026-03-26_rm_ui_dma_tx.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/risk_log.md`
- `docs/INDEX.md`

## 关键改动摘要

### `application/rm_ui.c`

- 删除 `RM_UI_TX_TIMEOUT_MS`，不再使用阻塞式发送超时宏。
- 新增 `RM_UI_FRAME_MAX_LEN` 和静态发送缓冲 `tx_buf[RM_UI_FRAME_MAX_LEN]`，保证 DMA 传输期间源缓冲有效。
- 新增：
  - `extern DMA_HandleTypeDef hdma_usart6_tx;`
  - `extern void usart6_tx_dma_enable(uint8_t *data, uint16_t len);`
- `rm_ui_send_interactive()` 保持原有组帧逻辑不变，仅替换发送路径：
  - 若 `hdma_usart6_tx.Instance == NULL`，直接返回 `0`
  - 若 `DMA_SxCR_EN` 置位，说明 DMA 正忙，直接返回 `0`
  - 若空闲，则将本帧 `memcpy` 到静态 `tx_buf`
  - 调用 `usart6_tx_dma_enable(tx_buf, total_len)` 启动发送

## 关键决策与理由

- 不改 `rm_ui_update()` 状态机和发送间隔。
  - 用户明确要求不动这部分，本轮目标仅验证“阻塞发送是否为主要根因”。
- 不修改 `bsp_usart.c/h`。
  - 现有 `usart6_tx_dma_enable()` 已可用，最小改动是仅在 `rm_ui.c` 内前向声明，避免扩大改动面。
- DMA 忙时直接丢弃当前帧，不排队。
  - 当前 UI 是低频状态刷新，且已有周期重试机制；相比引入队列和并发状态，直接跳过更稳妥，也符合用户要求。

## 实时性影响

- 移除了 `REFEREE` 任务中的一段显式阻塞串口发送路径。
- 常规 UI 发送从“当前任务同步等待串口搬完”改为“复制到静态缓冲后立即返回”。
- 新增开销仅为一次 `memcpy`，最大长度 `128B`，远小于原串口阻塞发送时长。

## 风险点

- 若 UI 状态切换频率短时间高于 DMA 清空速度，个别 UI 帧会被跳过，表现为某次刷新延后一拍。
- `bsp_usart.h` 当前未声明 `usart6_tx_dma_enable()`，本轮在 `rm_ui.c` 内局部前向声明；后续若要统一接口，可单独整理 BSP 头文件。
- 本轮未引入发送完成回调或发送队列，因此“忙时跳过”是有意设计，不是可靠传输语义。

## ⚠ 未验证假设

- 假设内容：`usart6_tx_dma_enable()` 可在当前 `USART6` 裁判链路配置下稳定用于 UI 发送，且不会破坏现有 `USART6 RX DMA + IDLE` 接收链路。
- 当前无法验证原因：本地环境无法做 Keil MDK 整工程编译，也没有板端裁判系统链路做实机联调。
- 潜在影响：若 `USART6 TX DMA` 初始化或标志位使用与现有工程配置不一致，可能出现 UI 不显示、偶发丢帧或编译告警。
- 后续验证计划：
  - 在 Keil 中整工程编译确认无声明/类型问题
  - 上板观察 `REFEREE_TOE` 是否仍误离线
  - 验证 UI 初次上线、模式切换、退弹闪烁和断线重连是否正常

## 验证方式

- 代码静态核对：
  - `rm_ui_send_interactive()` 不再调用任何 `HAL_UART_Transmit`
  - DMA 忙时直接 `return 0`
  - 发送前将栈上 `frame` 复制到静态 `tx_buf`
- 待板端验证：
  - 裁判系统在线时长跑观察 `REFEREE_TOE`
  - UI 初始绘制、模式切换、挡位切换、退弹闪烁、断线重连

## 回滚方式

- 回滚 `application/rm_ui.c` 中本轮 DMA 发送改动：
  - 删除 `tx_buf`
  - 删除 `hdma_usart6_tx` / `usart6_tx_dma_enable` 前向声明
  - 恢复 `HAL_UART_Transmit(&huart6, frame, total_len, RM_UI_TX_TIMEOUT_MS)`
- 若需快速止损，也可临时在上层直接关闭 `rm_ui_update()` 调用，验证是否确为 UI 发送导致裁判链路卡顿。
