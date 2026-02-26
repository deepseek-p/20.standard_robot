# 系统架构

## 1. 分层结构（代码目录视角）

1. 硬件与底层  
`Src/`、`Inc/`、`Drivers/`、`Middlewares/`、`bsp/boards/`

2. 组件层  
`components/algorithm`、`components/controller`、`components/devices`、`components/support`

3. 业务层  
`application/`（底盘/云台/射击/检测/裁判系统/遥控/IMU 等）

4. 工程与配置  
`MDK-ARM/standard_robot.uvprojx`、`standard_robot.ioc`

## 2. 启动与调度链路

`Src/main.c` 启动顺序（简化）：

1. HAL 与时钟初始化
2. CubeMX 外设初始化（GPIO/DMA/CAN/SPI/UART/USB 等）
3. USER CODE 初始化：`can_filter_init()`、`delay_init()`、`cali_param_init()`、`remote_control_init()`、`usart1_tx_dma_init()`
4. `MX_FREERTOS_Init()` 创建任务
5. `osKernelStart()` 进入调度

## 3. 运行期关键数据流

### 3.1 控制主链路

- 传感器链：`INS_task` 读取 BMI088/IST8310，更新 `INS_angle/INS_gyro`。
- 云台链：`gimbal_task` 读取遥控 + INS + 电机反馈，输出 `CAN_cmd_gimbal(...)`。
- 底盘链：`chassis_task` 读取遥控 + INS + 云台相对角 + 电机反馈，输出 `CAN_cmd_chassis(...)`。
- 射击链：`shoot_control_loop()` 在 `gimbal_task` 内执行，共享云台 CAN 发送周期。

### 3.2 监测与故障链

- 采集：各中断/任务调用 `detect_hook(toe)` 更新时间戳。
- 判定：`detect_task` 周期判断 `error_list`（离线/数据错误/频率）。
- 输出：`usb_task`、`oled_task`、`test_task` 读取 `error_list` 做状态上报/显示/蜂鸣器提示。

### 3.3 通信链

- 遥控：`USART3_IRQHandler` + DMA 双缓冲 -> `remote_control.c` 解析 SBUS。
- 裁判系统：`USART6_IRQHandler` + FIFO -> `referee_usart_task` 解包 -> `referee.c` 更新结构体。
- USB：`usb_task` 通过 CDC 输出状态文本。

### 3.4 WiFi 调参与 USART1 复用链路（2026-02-26）

- 编译开关：`application/wifi_bridge.h` 中 `WIFI_BRIDGE_ENABLE`。
- `WIFI_BRIDGE_ENABLE == 0`：
  - 保持原链路：`USART6` 由裁判系统 `IDLE + DMA 双缓冲` 使用。
- `WIFI_BRIDGE_ENABLE == 1`：
  - `referee_usart_task` 继续执行原裁判初始化与解包链路（`usart6_init()` + FIFO 解包）。
  - `USART6_IRQHandler` 保持裁判 `IDLE + DMA` 路径；WiFi 接收改为 `USART1_IRQHandler` 的 `RXNE` 逐字节收包（`uart1_rx_buf` 环形缓冲）。
  - `usb_task` 初始化阶段执行 `wifi_uart1_init()`（清 USART1 的 DMAR/DMAT，启 RXNE，并使能 `USART1_IRQn`）。
  - `usb_task` 在 USB 命令解析外新增 `wifi_cmd_process()`：
    - USB 收到的命令回复走 `CDC_Transmit_FS`。
    - UART1 收到的命令回复走 `HAL_UART_Transmit(&huart1, ...)`。
  - FireWater 遥测在 WiFi 模式下同时：
    - 保留 USB CDC 发出（非阻塞）
    - 额外通过 USART1 阻塞发出到 ESP32。
  - `remote_control.c` 的 `sbus_to_usart1()` 在 WiFi 模式下屏蔽，避免与桥接串口用途冲突。

## 4. 中断与任务耦合点（高风险）

- CAN 接收中断：`HAL_CAN_RxFifo0MsgPendingCallback` 写电机反馈并触发 `detect_hook`。
- USART3/USART6/USART1 中断：串口 DMA 缓冲切换 + 协议解析/桥接入口。
- EXTI + DMA（IMU）：`HAL_GPIO_EXTI_Callback` / `DMA2_Stream2_IRQHandler` 与 `INS_task` 通过 `ulTaskNotifyTake` 同步。

这些点改动时，必须同步更新 `docs/03_TASK_MAP.md` 与会话记录。

## 5. 实时性基线

- RTOS 节拍：`configTICK_RATE_HZ = 1000`（`1 tick = 1ms`）。
- 高频控制任务：`gimbal_task`（1ms）、`chassis_task`（2ms）。
- 事件驱动任务：`INS_task`（主循环由通知唤醒，不是固定 `osDelay` 周期）。
- 栈水位观测已在部分任务中启用：`calibrate/chassis/detect/gimbal`（`uxTaskGetStackHighWaterMark`）。
