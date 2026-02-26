# WiFi 实时调参系统 — AI 开发交接提示词

你是一个嵌入式系统开发助手。你的任务是帮我实现一个 WiFi 无线实时调参系统，替代当前的 USB 有线调参。

## 项目背景

这是一个 RoboMaster 步兵机器人项目，主控是 STM32F407（大疆 RoboMaster 开发板 C型），运行 FreeRTOS。

当前调参链路：
```
STM32 (USB CDC) ←→ USB线 ←→ PC (Python MCP Server)
```

目标链路：
```
STM32 (USART6) ←→ UART线 ←→ ESP32-S3 (WiFi AP) ←→ WiFi ←→ PC (Python MCP Server)
```

## 已确定的技术决策

1. ESP32-S3 开发板：YD-ESP32-S3（已有）
2. 开发框架：VSCode + pioarduino IDE 插件（Arduino 框架）
3. WiFi 模式：AP 模式，SSID `RM_PID`，密码 `12345678`
4. UART 接口：C板 UART2 4-pin 口（USART6, PG14-TX/PG9-RX），5V 供电 + 通信
5. 协议：UDP:8401 遥测上行，TCP:8402 命令下行
6. USART6 平时被裁判系统占用，调试时拔裁判系统线插 ESP32，用编译开关 `WIFI_BRIDGE_ENABLE` 切换

## 整体架构

```
┌──────────┐  UART 115200  ┌──────────────┐   WiFi AP    ┌──────────────┐
│  STM32   │ ────TX────→  │  ESP32-S3    │ ──UDP:8401→ │  PC          │
│  F407    │ ←───RX─────  │  (WiFi桥接)  │ ←─TCP:8402─ │  Python MCP  │
│          │              │              │              │  server      │
│ USART6   │ PG14→ESP_RX  │  AP模式      │              │              │
│(4-pin口) │ ESP_TX→PG9   │  SSID:RM_PID │              │              │
└──────────┘  + 5V供电     └──────────────┘              └──────────────┘
```

硬件接线（一根 4-pin 线）：
```
ESP32-S3 GPIO16 (RX) ←── C板 PG14 (USART6_TX)
ESP32-S3 GPIO17 (TX) ──→ C板 PG9  (USART6_RX)
ESP32-S3 5V (VIN)    ←── C板 4-pin 口 5V
ESP32-S3 GND         ─── C板 GND
```

## 现有系统详细说明

### 1. FireWater 遥测协议

STM32 以 50Hz 输出 CSV 帧，每帧 52 个 int32 值，逗号分隔，`\r\n` 结尾。最大帧长 1024 字节。

列定义（按索引）：
- 0-2: tick_ms, seq, drop（健康状态）
- 3-10: bat_pct, dbus_toe, yaw_toe, pitch_toe, gyro_toe, accel_toe, mag_toe, ref_toe
- 11-22: ch_mode, vx_set, vy_set, wz_set, rel, rel_set, ch_yaw, ch_yaw_set, i1-i4（底盘）
- 23-39: yaw_mode ~ pitch_cur（云台，共17列）
- 40-48: rc_ch0-3, rc_s0-1, mouse_x, mouse_y, key_v（遥控器）
- 49-51: event_bits, gimbal_ok, chassis_ok

毫米缩放列（值 ×1000 后输出，PC 端需 ÷1000）：索引 12-18, 26-31, 33-38
无效值标记：2147483647 (INT32_MAX)

### 2. PID 命令协议

命令格式（PC → STM32，文本，`\n` 结尾）：
```
SET <target> <param> <value>\n
GET <target> <param>\n
DUMP\n
```

回复格式（STM32 → PC，`\r\n` 结尾）：
```
OK SET pitch_speed Kp 15.000000\r\n
OK GET pitch_speed Kp 15.000000\r\n
DUMP pitch_speed Kp=15.000000 Ki=0.500000 Kd=2.000000 max_out=30000.000000 max_iout=10000.000000\r\n
DUMP END\r\n
ERR format SET\r\n
ERR target xxx\r\n
```

PID 目标（8个）：pitch_speed, pitch_angle, pitch_encode, yaw_speed, yaw_angle, yaw_encode, chassis_follow, chassis_wheel

PID 参数（5个）：Kp, Ki, Kd (范围 0~50000), max_out, max_iout (范围 0~30000)

### 3. STM32 端现有代码结构

关键文件：
- `application/usb_task.c` — 遥测输出 + 命令解析主逻辑
- `Src/usbd_cdc_if.c` — USB CDC 驱动，环形缓冲区接收
- `Src/usart.c` — UART 外设初始化（USART1/3/6）
- `application/referee_usart_task.c` — 裁判系统，USART6_IRQHandler 在此文件中（不在 usart.c）

USB CDC 接收机制（环形缓冲区，在 usbd_cdc_if.c 中）：
```c
uint8_t usb_rx_buf[256];
volatile uint16_t usb_rx_head = 0, usb_rx_tail = 0;

uint16_t usb_rx_available(void);   // 返回可读字节数
uint8_t  usb_rx_read_byte(void);   // 读一个字节，推进 tail
```

命令解析（usb_task.c 中的关键函数）：
```c
// usb_cmd_process() — 从 usb_rx_buf 逐字节读取，拼成一行后调用 usb_cmd_process_line()
// 注意：用了 static 局部变量做行缓冲
static void usb_cmd_process(void) {
    static char cmd_line[128];   // USB_CMD_LINE_MAX_LEN
    static uint16_t cmd_len = 0;
    static uint8_t cmd_overflow = 0;
    while (usb_rx_available() > 0u) {
        // 逐字节拼行，遇到 \n 调用 usb_cmd_process_line(cmd_line)
    }
}

// usb_cmd_process_line() — 解析 SET/GET/DUMP 命令并执行
// usb_cmd_replyf() — 格式化回复，当前直接调用 CDC_Transmit_FS()
```

USART6 当前配置（usart.c）：
- 115200 baud, 8N1
- DMA RX: DMA2_Stream1, Channel 5
- DMA TX: DMA2_Stream6, Channel 5
- GPIO: PG14 (TX), PG9 (RX)
- IRQ 优先级: 5

### 4. PC 端 MCP Server 现有结构

位置：`D:\tools\stm32-telemetry-mcp\`

关键文件：
- `server.py` — MCP 工具注册入口
- `serial_manager.py` — 串口通信（打开串口、读帧、发命令）
- `frame_parser.py` — CSV 帧解析（52列映射、毫米缩放）

MCP 工具（7个）：
1. `list_ports()` — 列出可用串口
2. `capture_data(port, duration_sec, channels, save_csv)` — 采集遥测
3. `analyze_oscillation(port, channel, duration_sec, save_csv)` — 振荡分析
4. `set_pid(port, target, param, value)` — 设置 PID
5. `get_pid(port, target, param)` — 读取 PID
6. `dump_pid(port)` — 导出所有 PID
7. `log_tune_conclusion(...)` — 记录调参结论

serial_manager.py 关键逻辑：
- 串口打开：`serial.Serial(port, 115200, timeout=0.1)`
- 帧读取：`readline()` 逐行，按逗号分割 52 个 int32
- 命令发送：`write(cmd + '\n')`，等待 `OK`/`ERR`/`DUMP` 开头的回复
- DUMP：持续收集直到 `DUMP END`
- 超时：2.0 秒
- 每次调用都独立开关串口（无持久连接）

---

## 你需要完成的三个任务

### 任务一：ESP32-S3 WiFi 桥接固件

用 pioarduino（Arduino 框架）开发，项目放在 `D:\tools\esp32_wifi_bridge\`。

`platformio.ini`：
```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
```

`src/main.cpp` 功能需求：

1. WiFi AP 模式：SSID `RM_PID`，密码 `12345678`，调用 `esp_wifi_set_ps(WIFI_PS_NONE)` 禁用省电
2. UART：`Serial2.begin(115200, SERIAL_8N1, 16, 17)`，`Serial2.setRxBufferSize(4096)`
3. 遥测转发（UART RX → UDP TX）：从 Serial2 逐行读取（`\n` 分隔，每行最大 1024 字节），每行一个 UDP 包广播到 255.255.255.255:8401
4. 命令转发（TCP:8402 ↔ UART TX/RX）：TCP Server 监听 8402，设置 `TCP_NODELAY`，PC 命令原样转发 UART，STM32 回复原样回传 TCP

5. UART 行分类（关键）：STM32 发来的数据混合了遥测帧和命令回复，ESP32 需要按行分类：
   - 以 `OK `、`ERR `、`DUMP` 开头的行 → 走 TCP 回传给 PC
   - 其余所有行（数字开头的遥测帧）→ 走 UDP 广播
   - 这样即使帧格式变化也不会误分类

### 任务二：STM32 端改动

用编译开关 `#define WIFI_BRIDGE_ENABLE 1`（建议放在 `usb_task.c` 顶部或公共头文件）控制所有改动。`WIFI_BRIDGE_ENABLE == 0` 时代码行为必须与改动前完全一致。

#### 改动 1：USART6 IRQ Handler（在 `application/referee_usart_task.c` 中）

当前 `USART6_IRQHandler` 在 `referee_usart_task.c` 中，用 IDLE+DMA 双缓冲接收裁判系统数据。

改动方式：用 `#if WIFI_BRIDGE_ENABLE` 在同一文件中包裹，为 1 时替换为 RXNE 逐字节环形缓冲区模式：

```c
#if WIFI_BRIDGE_ENABLE
// WiFi 桥接模式：RXNE 逐字节接收到环形缓冲区
uint8_t  uart6_rx_buf[256];
volatile uint16_t uart6_rx_head = 0, uart6_rx_tail = 0;

void USART6_IRQHandler(void) {
    if (USART6->SR & UART_FLAG_RXNE) {
        uint8_t byte = (uint8_t)(USART6->DR & 0xFF);
        uint16_t next = (uart6_rx_head + 1) % sizeof(uart6_rx_buf);
        if (next != uart6_rx_tail) {
            uart6_rx_buf[uart6_rx_head] = byte;
            uart6_rx_head = next;
        }
    }
}

uint16_t uart6_rx_available(void) {
    return (uart6_rx_head - uart6_rx_tail + sizeof(uart6_rx_buf)) % sizeof(uart6_rx_buf);
}
uint8_t uart6_rx_read_byte(void) {
    uint8_t c = uart6_rx_buf[uart6_rx_tail];
    uart6_rx_tail = (uart6_rx_tail + 1) % sizeof(uart6_rx_buf);
    return c;
}

#else
// 原有裁判系统 IRQ Handler（保持不动）
void USART6_IRQHandler(void) { ... }
#endif
```

#### 改动 2：新增 `wifi_cmd_process()`（在 `application/usb_task.c` 中）

**不要复用 `usb_cmd_process()`**。原因：`usb_cmd_process()` 内部用了 `static char cmd_line[128]` 和 `static uint16_t cmd_len` 做行缓冲。如果 USB 和 UART6 共享这些静态变量，两路同时收到半行数据会拼出错误命令。

正确做法：新写一个 `wifi_cmd_process()`，有自己独立的 static 行缓冲区，但拼好一行后调用同一个 `usb_cmd_process_line()` 做解析：

```c
#if WIFI_BRIDGE_ENABLE
static void wifi_cmd_process(void)
{
    static char cmd_line[USB_CMD_LINE_MAX_LEN];
    static uint16_t cmd_len = 0;
    static uint8_t cmd_overflow = 0;

    while (uart6_rx_available() > 0u) {
        uint8_t byte = uart6_rx_read_byte();
        if (byte == '\r') continue;
        if (byte == '\n') {
            if (cmd_overflow) {
                wifi_cmd_replyf("ERR line_too_long\r\n");  // 注意：回复走 UART6
            } else if (cmd_len > 0u) {
                cmd_line[cmd_len] = '\0';
                wifi_cmd_process_line(cmd_line);  // 共用解析逻辑，但回复走 UART6
            }
            cmd_len = 0u;
            cmd_overflow = 0u;
            continue;
        }
        // ... 与 usb_cmd_process 相同的字节累积逻辑
    }
}
#endif
```

#### 改动 3：回复路由 — `wifi_cmd_replyf()` 和 `wifi_cmd_process_line()`

当前 `usb_cmd_replyf()` 硬编码调用 `CDC_Transmit_FS()`。从 UART6 收到的命令，回复必须走 UART6 而不是 USB，否则 USB 端会收到它没请求的回复，干扰 PC 端串口解析。

推荐做法：
- 新写 `wifi_cmd_replyf()`，内部用 `HAL_UART_Transmit(&huart6, ...)` 阻塞发送（见改动 4 解释）
- 复制 `usb_cmd_process_line()` 为 `wifi_cmd_process_line()`，内部所有 `usb_cmd_replyf()` 调用替换为 `wifi_cmd_replyf()`
- 或者更优雅：给 `usb_cmd_process_line()` 加一个函数指针参数 `reply_fn`，USB 路传 `usb_cmd_replyf`，WiFi 路传 `wifi_cmd_replyf`

#### 改动 4：USART6 TX 带宽问题 — 遥测用阻塞发送

带宽计算：50Hz 遥测，每帧约 200-400 字节，115200 baud 下传输约 17-35ms，几乎占满 20ms 周期。如果遥测用 DMA TX，命令回复也要用 USART6 TX，两者会竞争 DMA 通道。

推荐做法：遥测和命令回复都用阻塞式 `HAL_UART_Transmit(&huart6, buf, len, timeout)`。理由：
- 20ms 周期内发完一帧绰绰有余（阻塞等待即可）
- 避免 DMA TX 竞争问题
- 命令回复很短（< 100 字节），阻塞发送延迟可忽略
- `usb_task` 本身运行在 FreeRTOS 任务中，阻塞不影响其他任务

#### 改动 5：遥测双路输出（在 `usb_emit_firewater_frame()` 中）

```c
// 现有 USB CDC 发送保留
CDC_Transmit_FS((uint8_t *)frame_buf, frame_len);

#if WIFI_BRIDGE_ENABLE
// 额外发送到 USART6 → ESP32（阻塞式）
HAL_UART_Transmit(&huart6, (uint8_t *)frame_buf, frame_len, 50);
#endif
```

#### 改动 6：usb_task 主循环调用 wifi_cmd_process()

```c
void usb_task(void const *argument) {
    // ... 现有初始化 ...
    while (1) {
        // 遥测输出（已含双路）
        usb_emit_firewater_frame(now_ms);
        // USB 命令处理（保持不动）
        usb_cmd_process();
#if WIFI_BRIDGE_ENABLE
        // WiFi 命令处理（独立缓冲区）
        wifi_cmd_process();
#endif
        osDelay(USB_DEBUG_TASK_PERIOD_MS);
    }
}
```

### 任务三：PC 端 MCP Server 改动

不做过度抽象。现有 `serial_manager.py` 中 `capture()` 和 `send_command()` 都是独立函数，每次调用都开关串口。WiFi 场景下 UDP 监听和 TCP 连接的生命周期不同，强行套 Transport 接口反而别扭。

推荐做法：直接在 `serial_manager.py` 里新增两个函数：

```python
def wifi_capture(ip, duration_sec, channels=""):
    """UDP socket 监听 0.0.0.0:8401 接收遥测帧"""
    import socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", 8401))
    sock.settimeout(0.1)
    # 每个 UDP 包 = 一行 CSV，解析逻辑复用 parse_line()
    # 采集 duration_sec 秒后关闭 socket
    # 返回 list[dict]，与 capture() 格式一致

def wifi_send_command(ip, cmd):
    """TCP 连接 ip:8402 发送命令"""
    import socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sock.settimeout(2.0)
    sock.connect((ip, 8402))
    sock.sendall((cmd + "\n").encode())
    # 等待回复行（OK/ERR/DUMP 开头）
    # DUMP 命令持续收集直到 "DUMP END"
    # 返回回复文本
```

`server.py` 分发逻辑：所有 MCP 工具的 `port` 参数检测 `wifi://` 前缀：

```python
def is_wifi_port(port: str) -> bool:
    return port.startswith("wifi://")

def parse_wifi_ip(port: str) -> str:
    return port.replace("wifi://", "")

# 在每个工具函数中：
if is_wifi_port(port):
    ip = parse_wifi_ip(port)
    frames = wifi_capture(ip, duration_sec, channels)
else:
    frames = capture(port, duration_sec, channels)
```

`list_ports()` 增加提示：如已连接 RM_PID 热点，可用 `wifi://192.168.4.1`

---

## 实施顺序

1. 先做 ESP32 固件，用 PC 串口助手模拟 STM32 测试双向透传
2. 再改 STM32 端，烧录后用串口助手连 UART2 4-pin 口验证
3. 最后改 MCP Server，`port="wifi://192.168.4.1"` 测试
4. 端到端联调：`capture_data` + `set_pid` 验证无线调参闭环

## 关键注意事项

1. ESP32 WiFi 省电必须关闭（`WIFI_PS_NONE`），否则延迟飙到 200ms+
2. TCP 必须设置 `TCP_NODELAY`，否则 Nagle 算法导致命令延迟 40-250ms
3. UART 行缓冲要够大（≥1024），单帧最大 1024 字节
4. USART6 TX 遥测和命令回复都用阻塞式 `HAL_UART_Transmit()`，避免 DMA 竞争
5. `usb_cmd_process()` 和 `wifi_cmd_process()` 必须各自有独立的 static 行缓冲区
6. 命令回复必须路由到正确的通道：USB 来的命令回复走 USB，UART6 来的回复走 UART6
7. ESP32 行分类：以 `OK `/`ERR `/`DUMP` 开头走 TCP，其余走 UDP
8. `WIFI_BRIDGE_ENABLE == 0` 时所有代码必须回退到原有裁判系统行为，零副作用
9. `USART6_IRQHandler` 在 `referee_usart_task.c` 中，不在 `usart.c` 中
10. UDP 丢帧在 50Hz 场景下可接受（<1%），不需要重传机制
