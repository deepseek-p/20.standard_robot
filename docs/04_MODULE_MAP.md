# 模块地图（按仓库真实目录初填）

## 0. 参考入口

- 仓库结构与构建说明：[`../AGENTS.md`](../AGENTS.md)
- 项目总览：[`01_PROJECT_OVERVIEW.md`](01_PROJECT_OVERVIEW.md)

## 1. 目录级地图

| 目录 | 职责 | 关键入口/说明 |
| --- | --- | --- |
| `application/` | 业务逻辑与任务实现（底盘、云台、射击、检测、通信） | `*_task.c/h`、`*_behaviour.c/h`、`CAN_receive.*`、`remote_control.*` |
| `bsp/boards/` | 板级外设驱动封装（CAN/UART/SPI/I2C/ADC/PWM 等） | `bsp_*.c/h` |
| `components/algorithm/` | 算法库与数学支撑（AHRS、中间层、工具函数） | `AHRS.*`、`AHRS_middleware.*`、`user_lib.*` |
| `components/controller/` | 控制器组件 | `pid.c/h` |
| `components/devices/` | 设备驱动与中间层 | `BMI088*`、`ist8310*`、`OLED*` |
| `components/support/` | 通用支撑组件 | `fifo.*`、`CRC8_CRC16.*`、内存管理等 |
| `Src/` | CubeMX 生成的 HAL 初始化、ISR、调度入口 | `main.c`、`freertos.c`、`stm32f4xx_it.c` |
| `Inc/` | CubeMX 生成头文件与系统配置 | `FreeRTOSConfig.h`、`main.h` |
| `Drivers/` | ST HAL / CMSIS 官方驱动 | 通常不直接改业务 |
| `Middlewares/` | FreeRTOS、USB Device 等中间件 | `Third_Party/FreeRTOS`、`ST/STM32_USB_Device_Library` |
| `MDK-ARM/` | Keil 工程、启动文件、编译配置 | `standard_robot.uvprojx` |

## 2. `application/` 关键文件（职责 + 关键接口/数据）

| 文件 | 职责一句话 | 关键接口/数据（初填） |
| --- | --- | --- |
| `application/CAN_receive.c` / `application/CAN_receive.h` | 处理 CAN 电机反馈与电流下发 | `HAL_CAN_RxFifo0MsgPendingCallback`、`CAN_cmd_chassis`、`CAN_cmd_gimbal`、`motor_measure_t` |
| `application/remote_control.c` / `application/remote_control.h` | 解析 SBUS 遥控数据并处理掉线恢复 | `USART3_IRQHandler`、`remote_control_init`、`get_remote_control_point`、`RC_ctrl_t` |
| `application/gimbal_task.c` / `application/gimbal_task.h` | 云台控制主任务（姿态环/速度环 + CAN 输出，导出调试快照） | `gimbal_task`、`gimbal_control_t`、`gimbal_mode_change_control_transit`、`gimbal_absolute_angle_limit`、`gimbal_relative_angle_limit`、`set_cali_gimbal_hook`、`cmd_cali_gimbal_hook`、`get_gimbal_debug_snapshot`；支持 `GIMBAL_YAW_CONTINUOUS_TURN` 下 yaw 限位旁路与校准输出覆盖 |
| `application/gimbal_behaviour.c` / `application/gimbal_behaviour.h` | 云台行为状态机（ZERO_FORCE/INIT/CALI/ANGLE） | `gimbal_behaviour_mode_set`、`gimbal_behaviour_control_set`、`gimbal_cali_control`；连续旋转模式下在校准流程中跳过 yaw max/min 步骤并直接进入 `GIMBAL_CALI_END_STEP` |
| `application/chassis_task.c` / `application/chassis_task.h` | 底盘控制主任务（运动解算、PID、CAN 输出，导出调试快照） | `chassis_task`、`chassis_move_t`、`chassis_rc_to_control_vector`、`get_chassis_debug_snapshot` |
| `application/chassis_behaviour.c` / `application/chassis_behaviour.h` | 底盘行为状态机（跟随云台/不跟随/开环） | `chassis_behaviour_mode_set`、`chassis_behaviour_control_set`、`CHASSIS_NO_FOLLOW_YAW` |
| `application/chassis_power_control.c` / `application/chassis_power_control.h` | 底盘功率限流（参考裁判系统功率/缓冲） | `chassis_power_control`、`get_chassis_power_and_buffer` |
| `application/shoot.c` / `application/shoot.h` | 射击状态机与拨弹/摩擦轮控制 | `shoot_init`、`shoot_control_loop`、`shoot_mode_e` |
| `application/referee.c` / `application/referee.h` | 裁判系统数据结构维护与查询接口 | `referee_data_solve`、`get_robot_id`、`get_shoot_heat0_limit_and_heat0` |
| `application/referee_usart_task.c` / `application/referee_usart_task.h` | 裁判系统串口 DMA 接收与协议解包任务 | `referee_usart_task`、`USART6_IRQHandler`、`referee_unpack_fifo_data` |
| `application/INS_task.c` / `application/INS_task.h` | IMU 采样、姿态解算、温控与数据发布 | `INS_task`、`get_INS_angle_point`、`HAL_GPIO_EXTI_Callback`、`DMA2_Stream2_IRQHandler` |
| `application/detect_task.c` / `application/detect_task.h` | 在线状态与错误检测中心 | `detect_task`、`detect_hook`、`toe_is_error`、`error_list` |
| `application/usb_task.c` / `application/usb_task.h` | USB CDC 调试遥测任务（多通道周期帧 + 事件帧 + 丢包计数） | `usb_task`、`usb_debug_set_channel_mask`、`usb_debug_get_channel_mask`、`CDC_Transmit_FS` |
| `application/oled_task.c` / `application/oled_task.h` | OLED 状态显示任务 | `oled_task`、`OLED_*` 接口 |
| `application/voltage_task.c` / `application/voltage_task.h` | 电池电压采样与电量估算 | `battery_voltage_task`、`get_battery_percentage` |

## 3. `components/` 初填说明

| 子目录/文件 | 简要说明 |
| --- | --- |
| `components/devices/BMI088driver.*` | BMI088 陀螺仪/加速度计驱动，提供初始化、读数与温度读取接口 |
| `components/devices/BMI088Middleware.*` | BMI088 驱动与平台适配层（SPI 读写、CS 管脚、延时） |
| `components/devices/ist8310driver.*` | IST8310 磁力计驱动（初始化与磁场读取） |
| `components/devices/ist8310driver_middleware.*` | IST8310 平台适配层（I2C 读写、复位脚、延时） |
| `components/devices/OLED.*` | OLED 图形与字符显示驱动（I2C + GRAM 缓冲） |
| `components/algorithm/AHRS_middleware.*` | 姿态解算中间层，封装数学函数与平台相关能力（高度/纬度等） |
| `components/algorithm/AHRS.*` | 姿态解算核心接口（四元数更新、欧拉角计算） |
| `components/controller/pid.*` | 通用 PID 控制器实现 |
| `components/support/fifo.*` | 字节 FIFO，裁判系统串口解包关键依赖 |
| `components/support/CRC8_CRC16.*` | 协议 CRC 校验工具 |

## 4. `bsp/boards/` 外设 -> 提供给谁用（初填）

| BSP 文件 | 外设/能力 | 主要调用方（初填） |
| --- | --- | --- |
| `bsp_can.c/h` | CAN 过滤器与接收中断使能 | `Src/main.c`（`can_filter_init`），`application/CAN_receive.c`（回调链路） |
| `bsp_rc.c/h` | 遥控接收 DMA 初始化/重启 | `application/remote_control.c` |
| `bsp_usart.c/h` | USART6 双缓冲、USART1 DMA 发送 | `application/referee_usart_task.c`、`application/remote_control.c` |
| `bsp_spi.c/h` | SPI1 DMA 初始化与启动 | `application/INS_task.c` |
| `bsp_i2c.c/h` | I2C 主机发送/ACK 检测/DMA 发送 | `components/devices/OLED.c`、`components/devices/ist8310driver_middleware.c` |
| `bsp_adc.c/h` | 电压/温度采样与参考校准 | `application/voltage_task.c`、`application/calibrate_task.c` |
| `bsp_imu_pwm.c/h` | IMU 加热 PWM 控制 | `application/INS_task.c` |
| `bsp_servo_pwm.c/h` | 舵机 PWM 输出 | `application/servo_task.c` |
| `bsp_fric.c/h` | 摩擦轮 PWM 输出 | `application/shoot.c` |
| `bsp_laser.c/h` | 激光开关控制 | `application/shoot.c` |
| `bsp_led.c/h` | RGB LED 显示 | `application/led_flow_task.c` |
| `bsp_flash.c/h` | Flash 擦写读 | `application/calibrate_task.c` |
| `bsp_buzzer.c/h` | 蜂鸣器控制 | `application/test_task.c`、`application/calibrate_task.c`、`application/gimbal_behaviour.c` |
| `bsp_delay.c/h` | us/ms 级延时 | `components/devices/BMI088Middleware.c`、`components/devices/ist8310driver_middleware.c` |
| `bsp_rng.c/h` | 随机数接口 | `application/CAN_receive.c`（已包含） |

## 5. 维护说明

- 新增任务时，同时更新：`docs/03_TASK_MAP.md` 与本文件 `application` 部分。
- 新增 `bsp_*.c/h` 或 `components/devices/*` 时，补充“外设 -> 使用方”映射。
- 改动架构层级（如新增通信链路）时，同步更新 `docs/02_ARCHITECTURE.md`。

## 2026-02-20 Supplement: USB Telemetry Safety

- `application/usb_task.c/h`: switched to FireWater fixed-frame telemetry for VOFA+; kept channel-mask interface for future debug expansion.
- `Src/usbd_cdc_if.c`: added null guard before using `hUsbDeviceFS.pClassData` in `CDC_Transmit_FS` to prevent USB-not-configured hard fault.
- `Src/freertos.c`: `USBTask` stack increased to improve robustness for long-format telemetry output.
