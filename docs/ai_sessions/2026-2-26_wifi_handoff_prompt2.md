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
3. WiFi 模式：AP 模式，SSID `TX`，密码 `2188630464`
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

1. WiFi AP 模式：SSID `TX`，密码 `2188630464`，调用 `esp_wifi_set_ps(WIFI_PS_NONE)` 禁用省电
2. UART：`Serial2.begin(115200, SERIAL_8N1, 16, 17)`，`Serial2.setRxBufferSize(4096)`
3. 遥测转发（UART RX → UDP TX）：从 Serial2 逐行读取（`\n` 分隔，每行最大 1024 字节），每行一个 UDP 包广播到 255.255.255.255:8401
4. 命令转发（TCP:8402 ↔ UART TX/RX）：TCP Server 监听 8402，设置 `TCP_NODELAY`，PC 命令原样转发 UART，STM32 回复原样回传 TCP

5. UART 行分类（关键）：STM32 发来的数据混合了遥测帧和命令回复，ESP32 需要按行分类：
   - 以 `OK `、`ERR `、`DUMP` 开头的行 → 走 TCP 回传给 PC
   - 其余所有行（数字开头的遥测帧）→ 走 UDP 广播
   - 这样即使帧格式变化也不会误分类

6. TCP 连接管理：
   - TCP Server 只接受一个客户端连接，新连接进来时断开旧的（多客户端同时发命令会导致 STM32 端命令交错）
   - PC 端每次命令用短连接（连接→发送→等回复→关闭），与现有 `serial_manager` 的"每次开关串口"模式一致

7. UDP 广播地址：
   - 建议用子网广播 `192.168.4.255` 而非受限广播 `255.255.255.255`，部分系统对受限广播处理不一致
   - 或者更稳健：PC 端先发一个 UDP "注册包" 到 ESP32:8401，ESP32 记住 PC 的 IP 后定向单播发送

### 任务二：STM32 端改动

用编译开关 `#define WIFI_BRIDGE_ENABLE 1`（建议放在 `usb_task.c` 顶部或公共头文件）控制所有改动。`WIFI_BRIDGE_ENABLE == 0` 时代码行为必须与改动前完全一致。

#### 改动 0（前置）：USART6 初始化链路（关键，不改会冲突）

USART6 的初始化分两步，你必须理解这个链路：
1. `MX_USART6_UART_Init()`（`Src/usart.c:75-91`）— 配置 HAL 句柄，115200 8N1。**保留不动**。
2. `usart6_init()`（`bsp/boards/bsp_usart.c:54-110`）— 由 `referee_usart_task()` 在 `referee_usart_task.c:67` 调用。这个函数做了三件事：
   - `SET_BIT(CR3, USART_CR3_DMAR | USART_CR3_DMAT)` 使能 DMA 收发请求
   - `__HAL_UART_ENABLE_IT(&huart6, UART_IT_IDLE)` 使能 IDLE 中断
   - 配置 DMA 双缓冲接收（M0AR/M1AR/DBM）

WiFi 模式下 `usart6_init()` **绝对不能被调用**，否则 DMA 双缓冲 + IDLE 中断会和 RXNE 环形缓冲区冲突。

改动方式：

**a) `referee_usart_task.c` 中屏蔽 `usart6_init()` 调用：**
```c
void referee_usart_task(void const * argument)
{
#if WIFI_BRIDGE_ENABLE
    // WiFi 模式：不初始化裁判系统，不解包，任务空转
    while(1) { osDelay(1000); }
#else
    init_referee_struct_data();
    fifo_s_init(&referee_fifo, referee_fifo_buf, REFEREE_FIFO_BUF_LENGTH);
    usart6_init(usart6_buf[0], usart6_buf[1], USART_RX_BUF_LENGHT);
    while(1)
    {
        referee_unpack_fifo_data();
        osDelay(10);
    }
#endif
}
```

注意：不要在 `freertos.c` 里不创建 `referee_usart_task` 任务，因为那个文件是 CubeMX 生成的，改了容易被覆盖。用任务入口早退的方式最安全。

**b) 新写 `wifi_uart6_init()`，在 `usb_task()` 初始化阶段调用：**
```c
#if WIFI_BRIDGE_ENABLE
static void wifi_uart6_init(void)
{
    // 防御性清除 DMA 请求位（正常流程下 usart6_init 未被调用所以本来就没开，
    // 但显式清除可防止初始化顺序被意外改动时出问题）
    CLEAR_BIT(huart6.Instance->CR3, USART_CR3_DMAR);
    CLEAR_BIT(huart6.Instance->CR3, USART_CR3_DMAT);
    // 使能 RXNE 中断（逐字节接收到环形缓冲区）
    __HAL_UART_ENABLE_IT(&huart6, UART_IT_RXNE);
}
#endif
```

在 `usb_task()` 中调用：
```c
void usb_task(void const *argument) {
    MX_USB_DEVICE_Init();
#if WIFI_BRIDGE_ENABLE
    wifi_uart6_init();
#endif
    error_list_usb_local = get_error_list_point();
    // ... 主循环
}
```

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

#### 改动 3：回复路由 — 函数指针方案（推荐）

当前 `usb_cmd_replyf()`（`usb_task.c:875-906`）硬编码调用 `CDC_Transmit_FS()`，且有 retry 循环（最多 50 次 `osDelay(1)`）。从 UART6 收到的命令，回复必须走 UART6 而不是 USB，否则 USB 端会收到它没请求的回复，干扰 PC 端串口解析。

推荐做法 — 函数指针，零代码复制：

```c
// 回复函数类型定义
typedef void (*cmd_reply_fn)(const char *format, ...);

// 新增 WiFi 回复函数
#if WIFI_BRIDGE_ENABLE
static void wifi_cmd_replyf(const char *format, ...)
{
    char line[USB_CMD_REPLY_MAX_LEN];
    va_list args;
    int len;

    if (format == NULL) return;
    va_start(args, format);
    len = vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if ((len <= 0) || (len >= (int)sizeof(line))) return;

    // 阻塞发送到 UART6，timeout 50ms 足够
    HAL_UART_Transmit(&huart6, (uint8_t *)line, (uint16_t)len, 50);
}
#endif

// 修改 usb_cmd_process_line 签名，加 reply_fn 参数
static void usb_cmd_process_line(char *line, cmd_reply_fn reply_fn);

// USB 路调用：usb_cmd_process_line(cmd_line, usb_cmd_replyf)
// WiFi 路调用：usb_cmd_process_line(cmd_line, wifi_cmd_replyf)
```

`usb_cmd_process_line()` 内部所有 `usb_cmd_replyf(...)` 调用改为 `reply_fn(...)`。同一个 FreeRTOS 任务里顺序执行，`strtok()` 全局状态不会冲突。

同理，`usb_cmd_dump_all()` 也需要接受 `reply_fn` 参数并传递下去。

#### 改动 4：USART6 TX 带宽问题 — 遥测用阻塞发送

带宽实际计算：
- 115200 baud = 11520 字节/秒 = 每字节约 86.8μs
- 典型帧 300-400 字节 × 86.8μs ≈ **26-35ms**，超过 20ms 周期
- 最大帧 1024 字节 × 86.8μs ≈ 89ms

实际影响：`usb_task` 有效循环周期从 20ms 拉长到 ~45-55ms（20ms osDelay + 26-35ms 阻塞发送），遥测实际帧率从 50Hz 降到约 **18-22Hz**。命令响应延迟最多 35ms。**对调参场景完全够用。**

如果后续觉得帧率太低，可以提波特率到 460800（ESP32 和 STM32 都支持），300 字节只需 6.5ms，完全在 20ms 内，可恢复 50Hz。ESP32 端改 `Serial2.begin(460800, ...)`，STM32 端改 `huart6.Init.BaudRate = 460800`。

推荐做法：遥测和命令回复都用阻塞式 `HAL_UART_Transmit(&huart6, buf, len, timeout)`。理由：
- 避免 DMA TX 竞争问题（遥测和命令回复共用 USART6 TX）
- 命令回复很短（< 100 字节），阻塞发送延迟可忽略
- `usb_task` 本身运行在 FreeRTOS 任务中，阻塞不影响其他任务
- 简单可靠，先跑通再优化

#### 改动 5：遥测双路输出（在 `usb_emit_firewater_frame()` 中）

```c
// 现有 USB CDC 发送保留（注意：USB 未连接时 CDC_Transmit_FS 返回 BUSY/FAIL，不影响后续逻辑）
CDC_Transmit_FS((uint8_t *)usb_buf, (uint16_t)len);

#if WIFI_BRIDGE_ENABLE
// 额外发送到 USART6 → ESP32（阻塞式）
// timeout 100ms：典型帧 300-400 字节在 115200 baud 下需 26-35ms，留余量
HAL_UART_Transmit(&huart6, (uint8_t *)usb_buf, (uint16_t)len, 100);
#endif
```

注意 `usb_buf` 的生命周期：`CDC_Transmit_FS()` 是非阻塞的（把指针交给 USB 外设后台 DMA），紧接着 `HAL_UART_Transmit()` 阻塞 26-35ms 期间 `usb_buf` 内容不变，所以没有竞争问题。但如果未来改成 DMA TX 发 UART6，需要注意 `usb_buf` 被两个 DMA 通道同时读的风险。

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

### 必须遵守

1. **USART6 初始化链路**：WiFi 模式下 `usart6_init()`（`bsp/boards/bsp_usart.c:54`）绝对不能被调用，它会启动 DMA 双缓冲 + IDLE 中断，和 RXNE 环形缓冲区冲突。用 `referee_usart_task` 入口早退 + `wifi_uart6_init()` 替代（见改动 0）
2. **`wifi_uart6_init()` 必须显式清 DMAT/DMAR 位**：虽然正常流程下 `usart6_init()` 未调用所以这两位本来没开，但防御性清除可防止初始化顺序被意外改动时出问题
3. `USART6_IRQHandler` 在 `referee_usart_task.c` 中，不在 `usart.c` 中
4. `usb_cmd_process()` 和 `wifi_cmd_process()` 必须各自有独立的 static 行缓冲区，不能共享
5. 命令回复必须路由到正确的通道：USB 来的命令回复走 USB，UART6 来的回复走 UART6。用函数指针方案（改动 3）
6. `WIFI_BRIDGE_ENABLE == 0` 时所有代码必须回退到原有裁判系统行为，零副作用
7. ESP32 行分类：以 `OK `/`ERR `/`DUMP` 开头走 TCP，其余走 UDP
8. USART6 TX 遥测和命令回复都用阻塞式 `HAL_UART_Transmit()`，避免 DMA 竞争

### 性能相关

9. ESP32 WiFi 省电必须关闭（`WIFI_PS_NONE`），否则延迟飙到 200ms+
10. TCP 必须设置 `TCP_NODELAY`，否则 Nagle 算法导致命令延迟 40-250ms
11. UART 行缓冲要够大（≥1024），单帧最大 1024 字节
12. 115200 baud 阻塞发送会使遥测帧率从 50Hz 降到约 18-22Hz，对调参够用。如需恢复 50Hz，提波特率到 460800
13. UDP 丢帧在此场景下可接受（<1%），不需要重传机制

### 容易踩的坑

14. **USB 未连接时 CDC 重试延迟**：`usb_cmd_replyf()` 内部有 retry 循环（最多 50 次 `osDelay(1)`），USB 没插时 `CDC_Transmit_FS()` 返回 BUSY 会白等 50ms。WiFi 调试时如果不插 USB，遥测帧的 CDC 发送建议改为 fire-and-forget（不重试），或检测 USB 连接状态再决定是否发送
15. **ESP32 TCP 只允许一个客户端**：多客户端同时发命令会导致 STM32 端命令交错
16. **ESP32 UDP 广播地址**：建议用子网广播 `192.168.4.255` 而非受限广播 `255.255.255.255`
17. **NVIC 优先级**：USART6 IRQ 优先级为 5，WiFi 版 IRQ Handler 里纯寄存器操作没问题，但不要在里面调用任何 FreeRTOS API（如 `xQueueSendFromISR`）
18. **帧率降低后采样率影响**：PC 端 `analyze_oscillation` 的频率检测精度会下降（奈奎斯特频率从 25Hz 降到约 10Hz），高频振荡可能检测不到
