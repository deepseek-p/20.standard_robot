# C 板物理接口与外设连接手册

> 信息来源：RoboMaster 开发板 C 型用户手册 v1.0 (2020.01)、VT03&VT13 使用说明书、
> 裁判系统串口协议附录 V1.7.0 (2024.12.25)、2026 通信协议 V1.2.0 (2025.02.09)。
> 本文由人工整理，非自动生成。芯片级引脚映射见 `hardware_map.md`。

---

## 1. 外壳丝印与 STM32 串口对照

**注意：C 板外壳丝印名称与 STM32 实际串口编号不对应！**

| 外壳丝印 | 实际 STM32 外设 | 引脚 | 连接器 | 当前用途 |
|----------|---------------|------|--------|---------|
| **UART1** | USART6 | PG14(TX) / PG9(RX) | 3-pin 卧式 (GND, TXD, RXD) | 裁判系统常规链路 |
| **UART2** | USART1 | PA9(TX) / PB7(RX) | 4-pin 卧式 (RXD, TXD, GND, 5V) | ESP32 WiFi 桥接 |
| *(PWM 座 C8)* | USART3 (仅 RX) | PC11(RX) | 24-pin 座 Pin-C8 | DR16 DBUS 遥控 |

---

## 2. UART 接口详情

### 2.1 丝印 UART1（STM32 USART6）— 3-pin

```
Pin1: GND
Pin2: TXD (PG14)
Pin3: RXD (PG9)
```

- 线序与裁判系统电源管理模块一致，但 **TX/RX 需要交叉接线**（手册原文）
- 信号电平：3.3V TTL（板载 ESD 保护 + 上拉）
- 当前配置：115200, 8N1, DMA 双向

### 2.2 丝印 UART2（STM32 USART1）— 4-pin

```
Pin1: RXD (PB7)
Pin2: TXD (PA9)
Pin3: GND
Pin4: 5V
```

- 信号电平：3.3V / 5V 兼容（板载 ESD 保护 + 上拉）
- 当前配置：115200, 8N1, DMA TX
- VT03 的 3-pin (TX/RX/GND) 可接入前 3 pin，Pin4 (5V) 空置

### 2.3 DBUS 接口（STM32 USART3）— PWM 座 Pin-C8

```
Pin-C8: DBUS 信号
Pin-B8: 5V
Pin-A8: GND
```

- **板载 NPN 反相电路**（Q10 PMBT3904 + R109 4.7KΩ 上拉至 3.3V）
- DBUS 信号经反相后连接到 USART3_RX (PC11)
- 当前配置：100000, 8E1 (偶校验), DMA 双缓冲
- **仅 RX**，USART3_TX (PC10) 未引出到连接器

> **重要**：由于硬件反相器存在，只有 DJI DBUS/SBUS（反相信号）能正确使用此接口。
> 标准 TTL UART 设备（如 VT03）接入此口会被反相，无法通信。

---

## 3. PWM 连接器详情（J16 24-pin 座）

### 3.1 物理布局

```
     槽1    槽2    槽3    槽4    槽5    槽6    槽7    槽8(DBUS)
C1   C2     C3     C4     C5     C6     C7     C8       ← 信号
B1   B2     B3     B4     B5     B6     B7     B8       ← 5V (VCC_5V_M)
A1   A2     A3     A4     A5     A6     A7     A8       ← GND (PGND)
```

### 3.2 信号引脚映射

| 槽位 | Pin | STM32 引脚 | 默认功能 | UART AF 可用？ |
|------|-----|-----------|---------|---------------|
| 1 | C1 | PE9 | TIM1_CH1 | 无 — PE9 仅有 TIM1/FMC |
| 2 | C2 | PE11 | TIM1_CH2 | 无 — PE11 仅有 TIM1/FMC |
| 3 | C3 | PE13 | TIM1_CH3 | 无 — PE13 仅有 TIM1/FMC |
| 4 | C4 | PE14 | TIM1_CH4 | 无 — PE14 仅有 TIM1 |
| 5 | C5 | PC6 | TIM8_CH1 | **USART6_TX (AF8)** — 仅 TX，且 USART6 已被裁判系统占用 |
| 6 | C6 | PI6 | TIM8_CH2 | 无 — PI6 仅有 TIM8/DCMI |
| 7 | C7 | PI7 | TIM8_CH3 | 无 — PI7 仅有 TIM8 |
| 8 | C8 | PC11 | DBUS | USART3_RX (AF7) / UART4_RX (AF8) — 已用于 DBUS，且有反相器 |

### 3.3 结论

7 个 PWM 槽的信号引脚中，PE9/PE11/PE13/PE14/PI6/PI7 在 STM32F407 的 AF 表中
**完全没有任何 UART 复用选项**。PC6 唯一能做 USART6_TX，但缺少配对 RX 且 USART6 已占用。
因此 **PWM 槽无法复用为 UART 口**，这是芯片硬件限制。

---

## 4. STM32F407 空闲 UART 资源

以下 UART 外设在芯片上可用，但 C 板未引出连接器：

| 外设 | TX 引脚 | RX 引脚 | 板上是否引出 | 备注 |
|------|---------|---------|------------|------|
| USART2 | PA2 或 PD5 | PA3 或 PD6 | 否 | PA2/PA3 未接任何连接器 |
| UART4 | PA0 或 PC10 | PA1 或 PC11 | 否 | PA0=用户按键, PC10/PC11=USART3 |
| UART5 | PC12 | PD2 | 否 | PC12/PD2 未接任何连接器 |

如需使用这些 UART，必须飞线焊接到对应芯片焊盘。

---

## 5. 其他板载接口

### 5.1 CAN 总线

| 接口 | 引脚 | 连接器 | 用途 |
|------|------|--------|------|
| CAN1 | PD0(RX) / PD1(TX) | 2-pin × 2 | 底盘 3508×4 + yaw/pitch 6020 + trigger 2006 |
| CAN2 | PB5(RX) / PB6(TX) | 4-pin × 2 | 摩擦轮 3508×2 |

### 5.2 可配置 I/O（8-pin 牛角座）

```
Pin1: SPI2_CS  (PB12)     Pin2: GND
Pin3: SPI2_CLK (PB13)     Pin4: 3.3V
Pin5: SPI2_MOSI(PB15)     Pin6: I2C2_SCL (PF1)
Pin7: SPI2_MISO(PB14)     Pin8: I2C2_SDA (PF0)
```

- 支持 3.3V 或 5V 设备（5V 需焊接 R210、去除 R209）
- 这些引脚 (PB12-15, PF0-1) 同样没有 UART AF

### 5.3 USB (micro USB)

- PA11 (DM) / PA12 (DP)，PA10 (OTG)
- 当前用于 USB CDC：FireWater 遥测 + 在线 PID 命令

### 5.4 IMU

- BMI088 (SPI1)：PA7/PB3/PB4，CS=PA4(Accel)/PB0(Gyro)
- IST8310 (I2C3)：PA8(SCL)/PC9(SDA)
- 陀螺仪加热：TIM10_CH1 (PF6)

---

## 6. 裁判系统通信架构

### 6.1 两条独立链路

```
┌──────────────┐                    ┌──────────┐
│  裁判系统服务器  │──── 无线 ────→│ 主控模块   │
│              │                    │          │
│              │                    │ 电源管理  │
│              │                    │  模块 [6] │──── SM03B ──→ STM32 USART6 (115200)
│              │                    │          │     常规链路
│              │                    │  模块 [5] │──── 航空线 ──→ VT03 (供电+内部总线)
│              │                    └──────────┘
│              │                                         │
│  选手端客户端  │◄──── 图传无线 ──── VT03 ────┘
│              │                       │
│              │                       └── UART 3-pin ──→ STM32 (921600 或 115200)
│              │                            图传链路
└──────────────┘
```

- **常规链路**：电源管理模块 User 串口 [6] → C 板 USART6，115200 8N1
  - 传输：所有裁判系统数据 (0x0001~0x0301)，包括血量、热量、功率等
- **图传链路**：VT03 UART 3-pin → C 板 (需另一路 UART)
  - 传输：键鼠遥控 + 自定义控制器数据

两条链路是 **物理上独立的两根串口线**，不能合并到同一个 UART。

### 6.2 图传链路协议对比

| 特性 | 旧协议 V1.7.0 (DR16 时代) | 新协议 V1.2.0 (VT03) |
|------|--------------------------|---------------------|
| 波特率 | 115200 | **921600** |
| 帧格式 | SOF=0xA5 + CRC8/CRC16 | 遥控帧：0xA9 0x53 + CRC16 |
| 键鼠数据 | cmd_id 0x0304 (12字节, 30Hz) | 21字节遥控帧 (14ms周期) |
| 摇杆通道 | 无（仅键鼠） | 有（4×11bit 摇杆 + wheel + trigger） |
| 自定义控制器 | 0x0302 (30字节) | 0x0302 (30字节)，SOF=0xA5 格式 |
| 机器人→控制器 | — | 0x0309 (30字节)，SOF=0xA5 格式 |
| 兼容现有代码？ | 可复用裁判系统帧解析器 | 需要全新解析器 |

---

## 7. 遥控输入协议对比

### 7.1 DJI DBUS (DR16 遥控器)

- 物理接口：C 板 DBUS 口 (PWM 座 Pin-C8) → 经反相器 → USART3_RX (PC11)
- 波特率：100000, 8E1 (偶校验, 1 停止位)
- 帧长度：18 字节，无帧头/帧尾，无 CRC
- 周期：14ms
- 数据：4×11bit 摇杆 + 2×2bit 拨杆 + 鼠标 XYZ + 左右键 + 16bit 键盘 + ch4

```c
// 当前代码解析 (remote_control.c : sbus_to_rc)
ch[0] = (buf[0] | (buf[1]<<8)) & 0x07ff;         // 摇杆右水平
ch[1] = ((buf[1]>>3) | (buf[2]<<5)) & 0x07ff;    // 摇杆右垂直
ch[2] = ((buf[2]>>6) | (buf[3]<<2) | (buf[4]<<10)) & 0x07ff; // 摇杆左水平
ch[3] = ((buf[4]>>1) | (buf[5]<<7)) & 0x07ff;    // 摇杆左垂直
s[0]  = (buf[5]>>4) & 0x03;                       // 左拨杆
s[1]  = ((buf[5]>>4) & 0x0C) >> 2;                // 右拨杆
mouse.x = buf[6] | (buf[7]<<8);                   // ... 以此类推
// 所有通道减去偏移 1024
```

### 7.2 VT03 遥控帧 (新图传链路)

- 物理接口：VT03 UART 3-pin (TX/RX/GND, 3.3V TTL)
- 波特率：921600, 8N1
- 帧长度：21 字节 (2 帧头 + 17 数据 + 2 CRC16)
- 周期：14ms
- CRC：CRC-16 (poly 见 crc16_tab，init=0xFFFF)

```c
// VT03 帧结构 (Example_Code_for_Data_Frame_and_Validation.c)
typedef __packed struct {
    uint8_t  sof_1;          // 0xA9
    uint8_t  sof_2;          // 0x53
    uint64_t ch_0:11;        // 摇杆通道 0 (0~2047, 中值 1024)
    uint64_t ch_1:11;        // 摇杆通道 1
    uint64_t ch_2:11;        // 摇杆通道 2
    uint64_t ch_3:11;        // 摇杆通道 3
    uint64_t mode_sw:2;      // 模式开关
    uint64_t pause:1;        // 暂停键
    uint64_t fn_1:1;         // 功能键 1
    uint64_t fn_2:1;         // 功能键 2
    uint64_t wheel:11;       // 滚轮 (0~2047, 中值 1024)
    uint64_t trigger:1;      // 扳机键
    int16_t  mouse_x;        // 鼠标 X 轴速度
    int16_t  mouse_y;        // 鼠标 Y 轴速度
    int16_t  mouse_z;        // 鼠标滚轮速度
    uint8_t  mouse_left:2;   // 鼠标左键
    uint8_t  mouse_right:2;  // 鼠标右键
    uint8_t  mouse_middle:2; // 鼠标中键
    uint16_t key;            // 键盘 16bit (W/S/A/D/Shift/Ctrl/Q/E/R/F/G/Z/X/C/V/B)
    uint16_t crc16;          // CRC-16 校验
} remote_data_t;
```

### 7.3 旧图传链路 0x0304 键鼠数据 (V1.7.0)

- 波特率：115200, 8N1
- 帧格式：SOF=0xA5 标准裁判系统帧，cmd_id=0x0304
- 数据长度：12 字节，30Hz
- **不含摇杆通道**，仅有键鼠

```c
typedef __packed struct {
    int16_t  mouse_x;           // 鼠标 X 速度
    int16_t  mouse_y;           // 鼠标 Y 速度
    int16_t  mouse_z;           // 鼠标滚轮
    int8_t   left_button_down;  // 左键 0/1
    int8_t   right_button_down; // 右键 0/1
    uint16_t keyboard_value;    // 键盘 bit0~15
    uint16_t reserved;          // 保留
} remote_control_t;
```

### 7.4 键盘 bit 映射（三种协议通用）

| bit | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|-----|---|---|---|---|---|---|---|---|---|---|----|----|----|----|----|----|
| 按键 | W | S | A | D | Shift | Ctrl | Q | E | R | F | G | Z | X | C | V | B |

DBUS、旧图传 0x0304、VT03 遥控帧的键盘 bitmask 定义完全一致。

---

## 8. VT03 接入方案分析

### 8.1 C 板 UART 资源占用现状

| UART | 连接器 | 当前用途 | 可否接 VT03 |
|------|--------|---------|------------|
| USART6 | 丝印 UART1 (3-pin) | 裁判系统常规链路 | 否 — 比赛必须 |
| USART1 | 丝印 UART2 (4-pin) | ESP32 WiFi 调参 | **可以 — 与 ESP32 分时复用** |
| USART3 | DBUS (PWM座 C8) | DR16 遥控器 | 否 — 板载反相器 (PMBT3904) |
| USART2 | 无连接器 | 空闲 | 需飞线 PA2/PA3 |
| UART5 | 无连接器 | 空闲 | 需飞线 PC12/PD2 |
| PWM 槽 1~7 | 24-pin 座 C1~C7 | PWM 输出 | 否 — 无 UART AF (芯片限制) |

### 8.2 推荐方案：USART1 (4-pin) 分时复用

VT03 的 3-pin (TX/RX/GND) 接到 C 板丝印 UART2 的 4-pin 座（前 3 pin 对应，第 4 pin 5V 空置）。

- **比赛模式**：接 VT03，USART1 配置为 921600 8N1
- **调试模式**：接 ESP32，USART1 配置为 115200 8N1
- 物理切换：拔插一根线
- 软件切换：编译宏 `#if VT03_ENABLE` 或运行时自动检测

### 8.3 备选方案：飞线 USART2

在 PCB 上找到 PA2/PA3 焊盘，焊线引出。
- 优点：VT03 和 ESP32 可同时使用
- 缺点：需要 PCB 焊接技能，接线不牢固

---

## 9. 参考文档

| 文档 | 位置 | 关键内容 |
|------|------|---------|
| C 板用户手册 v1.0 | `YD-ESP32-S3资料/RoboMaster 开发板 C 型用户手册 (3).pdf` | 接口原理图、线序、丝印对照 |
| VT03&VT13 使用说明书 | `YD-ESP32-S3资料/RoboMaster裁判系统相机图传模块VT03&VT13使用说明书 (2).pdf` | VT03 UART 参数、配对流程、LED 状态 |
| 通信协议 V1.2.0 | `YD-ESP32-S3资料/RoboMaster 2026 机甲大师高校系列赛通信协议 V1.2.0（20260209）.pdf` | 2026 新协议，常规/图传链路分离 |
| 串口协议 V1.7.0 | `YD-ESP32-S3资料/RoboMaster 裁判系统串口协议附录 V1.7.0（20241225）.pdf` | 旧协议，0x0304 键鼠数据 |
| 机器人制作规范 V1.3.0 | `YD-ESP32-S3资料/RoboMaster 2026 机甲大师高校系列赛机器人制作规范手册V1.3.0（20260209）.pdf` | 电源管理模块接口、航空线连接 |
| VT03 示例代码 | `YD-ESP32-S3资料/Example_Code_for_Data_Frame_and_Validation (1).c` | remote_data_t 结构体、CRC-16 校验 |
| 芯片级引脚映射 | `docs/hardware_map.md` | 自动生成，STM32 外设/DMA/中断配置 |

## 10. 2026-02-28 实施补充：UART 三模式切换

- 配置入口：`application/uart_mode.h`，通过 `CURRENT_UART_MODE` 选择运行模式。
- 三个模式值：
- `UART_MODE_DEBUG_WIFI (0)`：`USART6=Referee(115200)`，`USART1=WiFi(115200)`。
- `UART_MODE_DEBUG_VT03 (1)`：`USART6=VT03(921600)`，`USART1=WiFi(115200)`。
- `UART_MODE_COMPETITION (2)`：`USART6=Referee(115200)`，`USART1=VT03(921600)`。
- 比赛模式接线变更：`USART1`（4-pin 丝印 `UART2`）从 ESP32 改接 VT03 的 `TX/RX/GND` 三线。
- 编译注意事项：当 `CURRENT_UART_MODE=UART_MODE_COMPETITION` 时，`TELEM_OUTPUT_MODE` 不能为 `TELEM_MODE_WIFI`，编译期会触发 `#error`（`USART1` 已被 VT03 占用）。
