# Hardware Map — STM32F407IGHx (UFBGA176)

> 由 `tools/parse_ioc.py` 从 `standard_robot.ioc` 自动生成，勿手动编辑。
> 重新生成: `python tools/parse_ioc.py`

## 时钟树

| 时钟域 | 频率 |
|--------|------|
| HSE (外部晶振) | 12 MHz |
| SYSCLK | 168 MHz |
| AHB (HCLK) | 168 MHz |
| APB1 | 42 MHz |
| APB1 Timer | 84 MHz |
| APB2 | 84 MHz |
| APB2 Timer | 168 MHz |
| USB 48MHz | 48 MHz |

## 通信外设

### CAN1 — 底盘 3508×4 (0x201-0x204) + yaw 6020 (0x205) + pitch 6020 (0x206) + trigger 2006 (0x207)
- PD0 → RX
- PD1 → TX
- BS1: `CAN_BS1_10TQ`
- BS2: `CAN_BS2_3TQ`
- CalculateTimeQuantum: `71.42857142857143`
- Prescaler: `3`
- **波特率**: 1000 Kbps (APB1=42 MHz, PSC=3, 1+10+3=14 TQ)

### CAN2 — 摩擦轮 3508×2 (0x201/0x202)
- PB5 → RX
- PB6 → TX
- BS1: `CAN_BS1_10TQ`
- BS2: `CAN_BS2_3TQ`
- CalculateTimeQuantum: `71.42857142857143`
- Prescaler: `3`
- **波特率**: 1000 Kbps (APB1=42 MHz, PSC=3, 1+10+3=14 TQ)

### USART1 — ESP32 WiFi 桥接 (115200 默认)
- PA9 → TX
- PB7 → RX

### USART3 — SBUS 遥控器 (100000, EVEN parity)
- PC10 → TX
- PC11 → RX
- BaudRate: `100000`
- Parity: `PARITY_EVEN`

### USART6 — 裁判系统
- PG14 → TX
- PG9 → RX

### USB_OTG_FS — USB CDC — FireWater 遥测 + 在线 PID 调参
- PA11 → OTG_FS_DM
- PA12 → OTG_FS_DP

### SPI1 — BMI088 IMU (CS_ACCEL=PA4, CS_GYRO=PB0)
- PA7 → MOSI
- PB3 → SCK
- PB4 → MISO
- BaudRatePrescaler: `SPI_BAUDRATEPRESCALER_256`
- CLKPhase: `SPI_PHASE_2EDGE`
- CLKPolarity: `SPI_POLARITY_HIGH`
- CalculateBaudRate: `328.125 KBits/s`
- Direction: `SPI_DIRECTION_2LINES`
- Mode: `SPI_MODE_MASTER`

### SPI2 — 备用 SPI (CS=PB12)
- PB13 → SCK
- PB14 → MISO
- PB15 → MOSI
- BaudRatePrescaler: `SPI_BAUDRATEPRESCALER_256`
- CLKPhase: `SPI_PHASE_2EDGE`
- CLKPolarity: `SPI_POLARITY_HIGH`
- CalculateBaudRate: `164.062 KBits/s`
- Direction: `SPI_DIRECTION_2LINES`
- Mode: `SPI_MODE_MASTER`

### I2C1 — (PB8/PB9)
- PB8 → SCL
- PB9 → SDA
- I2C_Mode: `I2C_Fast`

### I2C2 — (PF1/PF0)
- PF0 → SDA
- PF1 → SCL
- I2C_Mode: `I2C_Fast`

### I2C3 — IST8310 磁力计 (PA8/PC9)
- PA8 → SCL
- PC9 → SDA
- I2C_Mode: `I2C_Fast`

## DMA 通道分配

| 请求 | DMA 实例 | 方向 | 优先级 |
|------|----------|------|--------|
| SPI1_RX | DMA2_Stream2 | P→M (RX) | Very High |
| SPI1_TX | DMA2_Stream3 | M→P (TX) | High |
| I2C2_TX | DMA1_Stream7 | M→P (TX) | High |
| USART3_RX | DMA1_Stream1 | P→M (RX) | High |
| USART6_RX | DMA2_Stream1 | P→M (RX) | High |
| USART6_TX | DMA2_Stream6 | M→P (TX) | Medium |
| USART1_TX | DMA2_Stream7 | M→P (TX) | Medium |

## 中断优先级 (NVIC_PRIORITYGROUP_4)

| 中断 | 抢占优先级 |
|------|-----------|
| USART3_IRQn | 4 |
| DMA1_Stream1_IRQn | 5 |
| DMA1_Stream7_IRQn | 5 |
| DMA2_Stream1_IRQn | 5 |
| DMA2_Stream2_IRQn | 5 |
| DMA2_Stream3_IRQn | 5 |
| DMA2_Stream6_IRQn | 5 |
| DMA2_Stream7_IRQn | 5 |
| EXTI3_IRQn | 5 |
| EXTI4_IRQn | 5 |
| EXTI9_5_IRQn | 5 |
| OTG_FS_IRQn | 5 |
| USART6_IRQn | 5 |
| CAN1_RX0_IRQn | 6 |
| CAN2_RX0_IRQn | 6 |
| EXTI0_IRQn | 6 |
| PendSV_IRQn | 15 |
| SysTick_IRQn | 15 |

## 定时器

- **TIM1**: PWM CH1-CH4 (PE9/PE11/PE13/PE14), PSC=167 → 1MHz, Period=19999 → 50Hz 舵机
- **TIM3**: PWM CH3 (PC8), Period=8399
- **TIM4**: PWM CH3 (PD14=BUZZER), PSC=167, Period=65535
- **TIM5**: PWM CH1-CH3 (PH10=LED_B, PH11=LED_G, PH12=LED_R), PSC=0, Period=65535
- **TIM8**: PWM CH1-CH2 (PC6/PI6), PSC=167 → 1MHz, Period=19999 → 50Hz
- **TIM10**: PWM CH1 (PF6), Period=4999

## GPIO（带标签）

| 引脚 | 标签 | 方向 | 备注 |
|------|------|------|------|
| PA0-WKUP | KEY | IN | 用户按键（上拉） |
| PA4 | CS1_ACCEL | OUT | BMI088 加速度计片选 |
| PB0 | CS1_GYRO | OUT | BMI088 陀螺仪片选 |
| PB12 | SPI2_CS | OUT | SPI2 片选（备用） |
| PC0 | HW0 | IN | 硬件版本位 0 |
| PC1 | HW1 | IN | 硬件版本位 1 |
| PC2 | HW2 | IN | 硬件版本位 2 |
| PC4 | INT1_ACCEL | EXTI | BMI088 加速度计中断 (EXTI4, 下降沿) |
| PC5 | INT1_GYRO | EXTI | BMI088 陀螺仪中断 (EXTI5, 下降沿) |
| PD14 | BUZZER | PWM/AF | 蜂鸣器 (TIM4_CH3 PWM) |
| PF10 | ADC_BAT | PWM/AF | 电池电压 ADC (ADC3_IN8) |
| PG3 | DRDY_IST8310 | EXTI | IST8310 数据就绪 (EXTI3, 下降沿) |
| PG6 | RSTN_IST8310 | OUT | IST8310 复位（高有效） |
| PH10 | LED_B | PWM/AF | 蓝色 LED (TIM5_CH1) |
| PH11 | LED_G | PWM/AF | 绿色 LED (TIM5_CH2) |
| PH12 | LED_R | PWM/AF | 红色 LED (TIM5_CH3) |
| PI7 | BUTTON_TRIG | IN | 触发按钮（上拉） |

## CAN ID 映射（来自 CAN_receive.h）

| CAN 总线 | 发送 ID | 电机 ID | 电机类型 | 用途 |
|----------|---------|---------|----------|------|
| hcan1 (CAN1) | 0x200 | 0x201-0x204 | M3508 | 底盘轮组 M1-M4 |
| hcan1 (CAN1) | 0x1FF | 0x205 | GM6020 | Yaw 云台 |
| hcan1 (CAN1) | 0x1FF | 0x206 | GM6020 | Pitch 云台 |
| hcan1 (CAN1) | 0x1FF | 0x207 | M2006 | 拨弹电机 |
| hcan2 (CAN2) | 0x200 | 0x201 | M3508 | 摩擦轮 1 |
| hcan2 (CAN2) | 0x200 | 0x202 | M3508 | 摩擦轮 2 |

## FreeRTOS

- 总堆大小: 24576 bytes
- API: CMSIS-RTOS v1
- Tick: 1ms (configTICK_RATE_HZ=1000)
