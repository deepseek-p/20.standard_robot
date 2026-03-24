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

### 3.5 遥测输出模式互斥 + 双 drop 计数（2026-02-27）

- 在 `usb_task.h` 中新增遥测输出模式宏：
  `TELEM_MODE_NONE / TELEM_MODE_USB / TELEM_MODE_WIFI`，
  默认 `TELEM_OUTPUT_MODE = TELEM_MODE_USB`。
- `wifi_bridge.h` 的 `WIFI_BRIDGE_ENABLE` 改为从 `TELEM_OUTPUT_MODE` 自动派生：
  仅在 `TELEM_MODE_WIFI` 编译启用 WiFi 桥接路径。
- `usb_task` 遥测发送改为互斥：
  - USB 模式：仅走 `CDC_Transmit_FS`。
  - WiFi 模式：仅走 `HAL_UART_Transmit(huart1)`。
  - NONE 模式：不调用 FireWater 帧发送函数。
- drop 计数拆分：
  - `usb_debug_drop_cnt`：USB 发送失败计数。
  - `wifi_debug_drop_cnt`：WiFi(UART1) 发送失败计数（仅 WiFi 模式编译）。
  - FireWater 第 2 列 `drop` 在 WiFi 模式输出 `wifi_debug_drop_cnt`，其它模式输出 `usb_debug_drop_cnt`。

## 2026-02-27 Supplement: Pitch Adaptive Gravity Feedforward Data Path

- `gimbal_motor_relative_angle_control()` 在 pitch 分支先得到速度环输出 `I_pid`，再叠加
  自适应前馈 `I_ff = K_hat * cos(relative_angle) + b_hat`。
- 准静态判定条件：
  `|speed| < PITCH_FF_SPEED_TH`、`|angle_err| < PITCH_FF_ERR_TH`、`|I_cmd| < PITCH_FF_SAT_TH`。
- 满足准静态时，用 `I_pid` 作为残余误差做 LMS 更新：
  `K_hat += gamma_k * error * cos(theta)`，`b_hat += gamma_b * error`，
  并加变化率限制与参数限幅。
- 自适应前馈不依赖弹丸计数，可覆盖空仓到装弹负载变化；任务调度周期保持不变，
  仅在 `gimbal_task` 1ms 路径增加常数级浮点计算。

## 2026-02-27 Supplement: Adaptive FF Deadlock Fix (7b)

- 去除准静态学习中的角度误差门限（不再依赖 `|angle_err| < TH`），
  允许”被重力压住但速度近零”的工况继续学习。
- 学习触发条件收敛为：
  `|speed| < PITCH_FF_SPEED_TH` 且 `|I_cmd| < PITCH_FF_SAT_TH`。
- 学习率与变化率限制同步上调（10x）：
  `PITCH_FF_GAMMA_K/B: 0.0005 -> 0.005`，
  `PITCH_FF_ALPHA_DOT_MAX: 5 -> 50`。

## 2026-02-27 Supplement: Pitch FF Decouple + Telemetry (7c)

- 在 `gimbal_motor_relative_angle_control()` 的 pitch 分支中，将前馈与 PID 合成改为：
  `I_total = I_pid ± I_ff`，并在赋值前执行硬限幅 `[-16000, 16000]`（GM6020 预留余量）。
- `K_hat/b_hat` 从函数内静态变量提升为文件级可观测状态：
  `pitch_ff_K_hat`、`pitch_ff_b_hat`，并通过 `get_pitch_ff_K_hat()` /
  `get_pitch_ff_b_hat()` 对外导出。
- USB FireWater 帧新增两列观测通道：
  `55:ff_k_hat`（milli-scale）和 `56:ff_b_hat`（raw），用于在线判断前馈收敛与偏置变化。

## 2026-02-27 Supplement: Pitch FF Adaptive Gamma (7d)

- 在 pitch LMS 更新块中，学习率从固定值改为误差自适应：
  `gamma = PITCH_FF_GAMMA_BASE + PITCH_FF_GAMMA_ERR_GAIN * |angle_err|`，
  并限幅到 `PITCH_FF_GAMMA_MAX`。
- `dk/db` 更新由固定 `PITCH_FF_GAMMA_K/B` 替换为统一 `gamma`，其余门控与 rate-limit 保持不变：
  `PITCH_FF_SPEED_TH`、`PITCH_FF_SAT_TH`、`PITCH_FF_ALPHA_DOT_MAX` 不变。
- 目标：大误差工况（如满仓逆重力追目标）提高学习速度，小误差稳态段降低学习激进度以抑制抖动。

## 2026-02-28 Supplement: Shoot Trigger Dual-Loop + Local Heat Predictor

- `shoot_control_loop()` 的拨轮控制从单环速度 PID 改为双环串级：
  - 外环：位置环 `trigger_pos_pid` 输出 `speed_set`
  - 内环：速度环 `trigger_spd_pid` 输出 `given_current`
- `shoot_feedback_update()` 在原 `ecd_count` 基础上新增连续编码器反馈 `trigger_ecd_fdb`，
  供位置环与连发计弹使用。
- 单发路径 `shoot_bullet_control()` 改为“目标位置 + 到位阈值”判定；
  堵转路径继续复用 `trigger_motor_turn_back()`，并在回退时同步回滚目标位置。
- 新增本地热量预测状态 `local_heat`：
  - 每 1ms 按 `HEAT_COOL_RATE` 冷却
  - 发弹成功按 `HEAT_PER_BULLET` 累加
  - 裁判热量更新时用 `get_shoot_heat0_limit_and_heat0()` 校准
- `shoot_set_mode()` 新增 `R` 键爆发模式切换，比赛模式下按
  `HEAT_LIMIT_SAFE/HEAT_LIMIT_BURST` 对单发/连发统一门控。
- 在线调参链路同步更新：`usb_task` 的 `target=trigger` 参数读写映射到
  新速度环 `trigger_spd_pid`。

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

## 2026-02-28 Supplement: VT03 Link + UART Three-Mode Switching

- Added compile-time UART assignment hub: `application/uart_mode.h`.
- Added three selectable modes:
  - `UART_MODE_DEBUG_WIFI`: `USART6=referee(115200)`, `USART1=WiFi(115200)`.
  - `UART_MODE_DEBUG_VT03`: `USART6=VT03(921600)`, `USART1=WiFi(115200)`.
  - `UART_MODE_COMPETITION`: `USART6=referee(115200)`, `USART1=VT03(921600)`.
- Added VT03 parser module:
  - `application/vt03_link.c/h` implements RXNE byte-stream state machine and frame CRC16 validation.
  - Parser writes decoded values into existing global `rc_ctrl` (`RC_ctrl_t`) to reuse upper control chain.
- `referee_usart_task` now has a mode-dependent path:
  - `USART6_REFEREE`: original DMA double-buffer + IDLE interrupt + FIFO unpack.
  - `USART6_VT03`: RXNE byte ISR path feeding `vt03_parse_byte`.
- `USART1_IRQHandler` is mode-dependent:
  - `USART1_VT03`: RXNE byte ISR path feeding `vt03_parse_byte`.
  - `WIFI_BRIDGE_ENABLE`: original UART1 ring-buffer for WiFi command ingress.
- Added compile-time conflict guard:
  - in competition mode, `TELEM_OUTPUT_MODE` cannot be `TELEM_MODE_WIFI` because USART1 is occupied by VT03.

## 2026-03-01 Supplement: VT03 Keyboard Action Layer

- Added a centralized key-action module: `application/keyboard_action.c/h`.
- Runtime path:
  - `gimbal_task` (1ms loop) calls `keyboard_action_update()` once per cycle.
  - `keyboard_action` samples `rc_ctrl.key.v` and VT13 extension keys (`fn_1/fn_2/pause/trigger/dial`).
  - Module outputs normalized command pulses/state via `keyboard_cmd_t`.
- Consumer split:
  - `shoot.c` consumes `shoot_toggle/high_freq_toggle/burst_toggle/fric_speed_adj/reverse_trigger/vt03_trigger`.
  - `chassis_behaviour.c` consumes `kb_chassis_mode` (CTRL toggle state).
  - `gimbal_behaviour.c` consumes `kb_zero_force` (Z toggle state).
- Shoot path update (v5):
  - `SHOOT_READY_BULLET` no longer auto-drives trigger-wheel speed; it is now a static pre-fire standby state.
  - fire edge (`trigger/mouse/down-switch`) can directly enter `SHOOT_BULLET` from `SHOOT_READY_BULLET`.
- Safety path update:
  - `gimbal_task/chassis_task/gimbal_behaviour` offline checks now treat control source as dual-input:
    trigger zero-current/zero-force only when `DBUS` and `VT03` are both offline.
- Observability update:
  - `usb_task` extends `event_bits` with `bit8..bit20` to expose VT13 raw key states and keyboard-action pulses for rapid mapping diagnosis.

## 2026-03-06 Supplement: Shoot State Machine Align HUST

- `shoot_control_loop()` now keeps one continuous trigger cascade authority in armed states:
  - `SHOOT_READY_BULLET` / `SHOOT_READY` / `SHOOT_DONE` all run the same position-loop-to-speed-loop hold on persistent `trigger_ecd_set`.
  - Removed the former low-authority hold branch (`TRIGGER_POS_MAX_OUT_HOLD`).
- `trigger_ecd_set` reset points are narrowed to true reset states only:
  - initialize path, `SHOOT_STOP`, and `SHOOT_READY_FRIC`.
  - no per-cycle re-track in `SHOOT_READY_BULLET` / `SHOOT_READY`.
- `SHOOT_DONE` is retained as a compatibility marker only:
  - `shoot_bullet_control()` still enters `SHOOT_DONE` when `fabs(pos_err) < TRIGGER_POS_THRESHOLD`.
  - `shoot_set_mode()` now transitions `SHOOT_DONE -> SHOOT_READY_BULLET` on the next control pass.
- External contracts are unchanged:
  - `shoot_mode` enum values and telemetry field semantics remain compatible.
  - DBUS / keyboard / VT03 trigger interpretation remains unchanged.

## 2026-03-07 Supplement: Shoot HUST Control Core Replacement

## 2026-03-24 Supplement: Chassis Model Power Limit + Referee Client UI

- `application/chassis_power_control.c/h`
  - 底盘限流从“按总电流缩放”升级为“功率预测模型 + 电流反解”。
  - 当前实现只保留裁判链路相关部分：读取 `chassis_power/chassis_power_buffer`，不包含超级电容分支。
- `application/referee_usart_task.c`
  - `REFEREE` 任务仍保持原有 DMA 接收、FIFO 解包职责。
  - 新增 `rm_ui_init()` / `rm_ui_update()` 调用点，把客户端 UI 发送挂到现有裁判任务上。
- `application/rm_ui.c/h`
  - 新增裁判客户端 UI 模块，负责 `0x0301` 组帧、CRC、图元初始化重建和低频刷新。
  - UI 内容包括固定准心、`FOLLOW/SPIN`、`LOW/HIGH`、退弹成功绿灯闪烁。
- 运行时边界
  - 不新建任务。
  - 不改 `usb_task`。
  - 发送仍走 `USART6`，接收主链仍由 `referee_unpack_fifo_data()` 维护。

- `shoot` 内核由“外置 `shoot_logic` 命令/执行层”切换为 `shoot.c` 内联 HUST 风格控制流：
  - 单发：位置环 -> 速度环串级（一步一格）
  - 连发：跳过位置环，速度环直接驱动（`3500/4500 rpm`）
  - 反转：边沿触发的一格回退
- 速度反馈与速度目标统一为 `rpm` 语义：
  - `shoot_feedback_update()` 速度滤波输入改为 `speed_rpm` 原值（不再乘 `MOTOR_RPM_TO_SPEED`）
  - FireWater 中 `shoot.speed/speed_set` 列值域随之变化（rad/s -> rpm）
- 架构边界不变：
  - 仍在 `gimbal_task` 1ms 路径内执行
  - 不新增任务、不新增阻塞调用、不改 CAN 发送节拍
