# 任务映射（自动初填）

> 初填来源（代码提取）  
> - `Src/freertos.c`：`osThreadDef(...)` / `osThreadCreate(...)`（创建顺序、优先级、栈）  
> - `application/*`：`osDelay(...)` / `vTaskDelay(...)` 与任务宏  
> - `Inc/FreeRTOSConfig.h`：`configTICK_RATE_HZ = 1000`（`1 tick = 1ms`）

## 1. 任务总表

| 创建顺序 | osThreadDef 名称 | 任务入口函数 | CMSIS 优先级 | 栈大小 | 周期初值（ms） | 主要阻塞点/等待 | 是否记录栈余量 | 共享资源（初填） | 输入 -> 输出（初填） | 涉及外设（初填） | 风险（初填） | 验证方式（建议） | 回滚方式（建议） |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `test` | `test_task` | `osPriorityNormal` | 128 | 10（`application/test_task.c`） | `osDelay(10)` | 否 | `error_list`、蜂鸣器状态 | 检测错误 -> 蜂鸣器告警 | 蜂鸣器 PWM | 告警误报/漏报 | 人工制造故障码，观察蜂鸣器节奏 | 临时停用该任务或恢复原告警逻辑 |
| 2 | `cali` | `calibrate_task` | `osPriorityNormal` | 512 | `CALIBRATE_CONTROL_TIME = 1` | `osDelay(CALIBRATE_CONTROL_TIME)` | 是 | 校准参数、Flash、RC 数据 | 遥控命令/传感器 -> 校准参数写入 | Flash、ADC、蜂鸣器 | 误写 Flash、校准中断导致姿态偏移 | 串口日志 + 上位机 + 实机校准流程 | 恢复旧校准参数并回退改动 |
| 3 | `ChassisTask` | `chassis_task` | `osPriorityAboveNormal` | 512 | `CHASSIS_CONTROL_TIME_MS = 2` | `vTaskDelay(CHASSIS_CONTROL_TIME_MS)` | 是 | `chassis_move`、RC、INS、电机反馈 | RC/姿态/电机反馈 -> `CAN_cmd_chassis` | CAN1 | 控制环抖动、CAN 电流槽位误配 | 上位机看轮速与电流，长跑温升/功率观察 | 切回原 PID/模式映射并发零电流验证 |
| 4 | `DETECT` | `detect_task` | `osPriorityNormal` | 256 | `DETECT_CONTROL_TIME = 10` | `vTaskDelay(DETECT_CONTROL_TIME)` | 是 | 全局 `error_list` | `detect_hook` 时间戳 -> 错误状态 | 无直接外设（依赖各模块 hook） | 离线阈值不当导致误保护 | 拔插设备/断总线测试离线判定 | 恢复 `set_item` 离线参数 |
| 5 | `gimbalTask` | `gimbal_task` | `osPriorityHigh` | 512 | `GIMBAL_CONTROL_TIME = 1` | `vTaskDelay(GIMBAL_CONTROL_TIME)` | 是 | `gimbal_control`、`shoot_control`、RC、INS | RC/姿态/电机反馈 -> `CAN_cmd_gimbal` | CAN2 | 模式切换瞬态、射击耦合影响稳定性 | 云台阶跃测试 + 实弹链路联调 + 长跑 | 回退行为状态机与控制模式改动 |
| 6 | `imuTask` | `INS_task` | `osPriorityRealtime` | 1024 | 未知（事件驱动，需测量/需确认） | `ulTaskNotifyTake(portMAX_DELAY)`；初始化 `osDelay(INS_TASK_INIT_TIME)`、`osDelay(100)` 重试 | 否 | `INS_angle/INS_gyro/INS_quat` 全局数据 | IMU 采样/中断 -> 姿态解算结果 | SPI1 DMA、I2C3、EXTI | 中断-任务同步竞态、姿态漂移 | 静态漂移、动态转台、示波器看中断节拍 | 恢复原 DMA/EXTI 时序与解算参数 |
| 7 | `led` | `led_RGB_flow_task` | `osPriorityNormal` | 256 | 1（颜色渐变内循环） | `osDelay(1)` | 否 | RGB 颜色表 | 渐变状态 -> RGB LED 输出 | RGB LED | CPU 占用上升 | 观察灯效平滑度与系统负载 | 降低刷新频率或禁用任务 |
| 8 | `OLED` | `oled_task` | `osPriorityLow` | 256 | `OLED_CONTROL_TIME = 10` | `osDelay(1000)`、`osDelay(10)`、`osDelay(OLED_CONTROL_TIME)` | 否 | `error_list`、电池百分比 | 错误状态/电池 -> OLED 页面 | I2C2 OLED | I2C ACK 异常导致显示阻塞 | OLED 上电自检 + ACK 监控 | 停用 OLED 刷新或恢复原显示逻辑 |
| 9 | `REFEREE` | `referee_usart_task` | `osPriorityNormal` | 128 | 10 | `osDelay(10)` | 否 | `referee_fifo`、裁判结构体 | USART6 DMA 数据 -> 裁判状态结构 | USART6 DMA | FIFO 溢出、CRC 失败导致数据陈旧 | 上位机喂帧 + CRC 异常注入 | 回退解析状态机与 FIFO 参数 |
| 10 | `USBTask` | `usb_task` | `osPriorityBelowNormal` | 512 | 20（FireWater 帧） | `osDelay(USB_DEBUG_TASK_PERIOD_MS=5)` | 否 | `error_list`、云台/底盘调试快照、RC 数据、丢包计数、USB RX 命令队列 | 调试快照 -> USB FireWater 数值帧（VOFA+）；USB 文本命令 -> 运行时 PID 参数更新与回读 | USB FS CDC | CDC 拥塞丢包、格式化开销、在线调参误设参数风险 | VOFA+ 连续接收 + 命令交互并观测 `drop` 与 `OK/ERR/DUMP` 回复可达性 | 降低输出频率/关闭部分通道；回退在线调参补丁 |
| 11 | `BATTERY_VOLTAGE` | `battery_voltage_task` | `osPriorityNormal` | 128 | 100（启动后） | `osDelay(1000)`，循环 `osDelay(100)` | 否 | `battery_voltage`、`electricity_percentage` | ADC 采样 -> 电量百分比 | ADC | 电量估算曲线偏差 | 万用表对比 + 长时放电曲线 | 回退 `VOLTAGE_DROP`/估算曲线 |
| 12 | `SERVO` | `servo_task` | `osPriorityNormal` | 128 | 10 | `osDelay(10)` | 否 | `servo_pwm[]`、RC 键值 | 键盘输入 -> `servo_pwm_set` | TIM PWM 舵机口 | 机械行程越界/抖动 | 空载与负载行程测试 | 恢复 PWM 限幅与按键映射 |

## 2. 周期初值推导依据

- `chassis_task`：`application/chassis_task.h` -> `CHASSIS_CONTROL_TIME_MS`
- `gimbal_task`：`application/gimbal_task.h` -> `GIMBAL_CONTROL_TIME`
- `detect_task`：`application/detect_task.h` -> `DETECT_CONTROL_TIME`
- `INS_task`：`application/INS_task.h` -> `INS_TASK_INIT_TIME`；主循环由通知唤醒，不是固定周期
- `calibrate_task`：`application/calibrate_task.h` -> `CALIBRATE_CONTROL_TIME`
- `oled_task`：`application/oled_task.c` -> `#define OLED_CONTROL_TIME 10`

## 3. 栈水位记录点（代码已存在）

- 已记录：`calibrate_task`、`chassis_task`、`detect_task`、`gimbal_task`
- 未记录：`INS_task`、`oled_task`、`referee_usart_task`、`usb_task`、`battery_voltage_task`、`servo_task`、`led_RGB_flow_task`、`test_task`

> 注：若任务行为变更（新增缓冲、协议解析、滤波器或日志），应优先补齐该任务的栈水位观测。

## 2026-02-20 Supplement: USB FireWater V1.2

- `USBTask` now outputs fixed-field FireWater CSV frames for VOFA+.
- `USBTask` stack was increased to `512` in `Src/freertos.c` to reduce formatting stack risk.
- USB transmit path now depends on non-blocking send + drop counter; busy path never blocks control tasks.
- New board-level protection in `Src/usbd_cdc_if.c`: if `hUsbDeviceFS.pClassData == NULL`, transmit returns fail instead of dereferencing null.

## 2026-02-24 Supplement: USB Online PID Tuning

- `USBTask` now parses CDC RX text commands in task context before `osDelay`: `SET/GET/DUMP`.
- `CDC_Receive_FS` remains enqueue-only and exports RX ring buffer accessors (`usb_rx_available` / `usb_rx_read_byte`).
- Runtime writable control accessors are added for gimbal/chassis control objects to support direct PID field updates.

## 2026-02-26 Supplement: Shoot Fric CAN Closed-Loop

- `gimbalTask` CAN send path now contains two frames per control cycle on `hcan2`:
- `CAN_cmd_gimbal(0x1FF)` for yaw/pitch/trigger.
- `CAN_cmd_fric(0x200)` for fric1/fric2 currents.
- `shoot_control_loop` now computes fric closed-loop currents from real RPM feedback (`0x201/0x202`) and exports via `shoot_get_fric_current`.
- Fault path (`DBUS` or key gimbal/trigger motor offline) sends zero current to both `gimbal` and `fric` groups.

## 2026-02-26 Supplement: WiFi Bridge via USART1

- New compile switch: `application/wifi_bridge.h` -> `WIFI_BRIDGE_ENABLE`.
- `WIFI_BRIDGE_ENABLE=1` behavior changes:
  - `REFEREE` task (`referee_usart_task`) keeps referee UART DMA initialization and unpack loop unchanged.
  - `USART6_IRQHandler` remains referee `IDLE + DMA` path; WiFi RX input runs on `USART1_IRQHandler` byte-ring (`uart1_rx_available` / `uart1_rx_read_byte`).
  - `USBTask` initializes USART1 WiFi RX path (`wifi_uart1_init`) and processes an extra command source (`wifi_cmd_process`).
  - `USBTask` telemetry adds blocking USART1 TX copy in addition to USB CDC output.
  - `remote_control.c` disables `sbus_to_usart1` forwarding when `WIFI_BRIDGE_ENABLE=1`.
- Timing note:
  - At 115200 baud, USART1 blocking telemetry TX may stretch `USBTask` effective telemetry period (expected from ~50Hz to ~18-22Hz for typical frame length).

## 2026-02-27 Supplement: USBTask Priority Lowering for WiFi DBUS Stability

- `Src/freertos.c` changed `USBTask` priority from `osPriorityNormal` to `osPriorityBelowNormal`.
- Rationale: in `TELEM_MODE_WIFI`, blocking `HAL_UART_Transmit` can hold `USBTask` for one long frame and delay same-priority tasks.
- Expected effect: keep `detect_task` (`osPriorityNormal`) schedulable and reduce false DBUS-offline alarms.
- Unchanged scope:
  - DBUS offline timeout config is unchanged.
  - UART1 telemetry send API remains blocking.

## 2026-02-28 Supplement: VT03 + UART Mode Switching

- `REFEREE` task (`referee_usart_task`) now has compile-time split behavior:
  - `USART6_REFEREE=1`: keep legacy referee DMA+IDLE receive and unpack flow.
  - `USART6_REFEREE=0` (`USART6_VT03=1`): skip referee DMA init and use RXNE byte interrupt for VT03 parser.
- `detect_task` adds a new device slot: `VT03_TOE` with offline/online thresholds for VT03 link monitoring.
- `main.c` USER init adds `USART1_VT03` branch:
  - VT03 mode: enable RXNE IRQ on USART1 and skip `usart1_tx_dma_init`.
  - non-VT03 mode: keep original `usart1_tx_dma_init`.
- `USART3_IRQHandler` (DBUS path) now suppresses `sbus_to_usart1` forwarding when `USART1_VT03=1`.

## 2026-03-01 Supplement: Keyboard Action Integration

- `gimbalTask`:
  - added `keyboard_action_init()` after `shoot_init()`.
  - added `keyboard_action_update()` at the beginning of each 1ms loop.
  - offline motor-zero path changed to dual-source check: `DBUS && VT03` offline.
  - `shoot_set_mode` now allows fire edge entry from `SHOOT_READY_BULLET`, reducing dependency on trigger key switch state.
  - `shoot_control_loop` keeps `SHOOT_READY_BULLET` as static standby (no automatic trigger-wheel motion).
- `ChassisTask`:
  - startup wait condition changed to `DBUS && VT03` offline.
  - runtime zero-current guard changed to `DBUS && VT03` offline.
- `USBTask`:
  - `event_bits` high bits (`bit8..bit20`) now expose VT13 raw key states and keyboard-action command pulses for mapping diagnostics.
- Task scheduling unchanged:
  - no task creation changes in `Src/freertos.c`;
  - no priority, stack size, or period change.

## 2026-03-06 Supplement: Shoot State Machine Align HUST

- `gimbalTask` (`application/shoot.c`) trigger control path update:
  - `SHOOT_READY_BULLET` / `SHOOT_READY` / `SHOOT_DONE` now keep active position-loop-to-speed-loop hold on persistent `trigger_ecd_set`.
  - removed low-authority hold dependency (`TRIGGER_POS_MAX_OUT_HOLD`).
  - target re-track `trigger_ecd_set = trigger_ecd_fdb` remains only in reset states (`SHOOT_STOP`, `SHOOT_READY_FRIC`, init path).
  - `SHOOT_DONE` now acts as a one-cycle compatibility transition to `SHOOT_READY_BULLET` in `shoot_set_mode()`.
- Scheduling and RTOS impact:
  - no task creation/priority/period change.
  - no new blocking calls; shoot control remains on existing 1ms `gimbal_task` path.

## 2026-03-07 Supplement: Shoot Core Replacement (HUST)

- `gimbalTask` 内的 `shoot_control_loop` 控制语义更新：
  - `SHOOT_READY_BULLET`：持续位置 hold（串级 PID）
  - `SHOOT_BULLET`：单发一步一格（串级 PID）
  - `SHOOT_CONTINUE_BULLET`：速度环直驱（不经位置环）
- 速度反馈单位改为 `rpm`：
  - `shoot_control.speed` / `shoot_control.speed_set` 的遥测量级从 `rad/s` 切到 `rpm`
- 调度影响：
  - 无任务创建/优先级/周期变更
  - 无新增阻塞路径
