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
  允许“被重力压住但速度近零”的工况继续学习。
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

## 4. 中断与任务耦合点（高风险）

- CAN 接收中断：`HAL_CAN_RxFifo0MsgPendingCallback` 写电机反馈并触发 `detect_hook`。
- USART3/USART6 中断：串口 DMA 缓冲切换 + 协议解析入口。
- EXTI + DMA（IMU）：`HAL_GPIO_EXTI_Callback` / `DMA2_Stream2_IRQHandler` 与 `INS_task` 通过 `ulTaskNotifyTake` 同步。

这些点改动时，必须同步更新 `docs/03_TASK_MAP.md` 与会话记录。

## 5. 实时性基线

- RTOS 节拍：`configTICK_RATE_HZ = 1000`（`1 tick = 1ms`）。
- 高频控制任务：`gimbal_task`（1ms）、`chassis_task`（2ms）。
- 事件驱动任务：`INS_task`（主循环由通知唤醒，不是固定 `osDelay` 周期）。
- 栈水位观测已在部分任务中启用：`calibrate/chassis/detect/gimbal`（`uxTaskGetStackHighWaterMark`）。
