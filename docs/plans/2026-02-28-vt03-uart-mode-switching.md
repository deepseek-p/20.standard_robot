# VT03 图传链路接入 + UART 三模式切换 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 实现 VT03 图传模块的 21 字节遥控帧解析，通过编译宏在三种 UART 分配模式间切换，使调试和比赛场景无缝切换。

**Architecture:** 新增 `vt03_link` 模块，包含帧状态机和 CRC-16 校验，解析结果写入现有 `rc_ctrl` 全局变量（复用 `RC_ctrl_t`）。通过 `UART_MODE` 宏控制 USART1/USART6 的波特率和中断处理函数绑定。VT03 接收统一使用 RXNE 逐字节中断 + 状态机解析，与 UART 实例解耦。

**Tech Stack:** STM32F407 HAL + 裸寄存器混合, armcc (Keil MDK), FreeRTOS CMSIS-RTOS v1

---

## 模式定义

| 宏值 | USART6 (3-pin 丝印UART1) | USART1 (4-pin 丝印UART2) | 场景 |
|------|--------------------------|--------------------------|------|
| `UART_MODE_DEBUG_WIFI` (0) | 裁判系统 115200 | ESP32 WiFi 115200 | 日常调参 |
| `UART_MODE_DEBUG_VT03` (1) | 图传链路 921600 | ESP32 WiFi 115200 | 测试 VT03 键鼠 |
| `UART_MODE_COMPETITION` (2) | 裁判系统 115200 | 图传链路 921600 | 比赛 |

---

### Task 1: 创建 UART 模式配置头文件

**Files:**
- Create: `application/uart_mode.h`

**Step 1: 编写头文件**

```c
#ifndef UART_MODE_H
#define UART_MODE_H

#include "usb_task.h"   /* for TELEM_OUTPUT_MODE / TELEM_MODE_WIFI */

/* ---- UART 分配模式 (三选一) ---- */
#define UART_MODE_DEBUG_WIFI   0   /* USART6=裁判115200  USART1=ESP32 115200  */
#define UART_MODE_DEBUG_VT03   1   /* USART6=VT03 921600 USART1=ESP32 115200  */
#define UART_MODE_COMPETITION  2   /* USART6=裁判115200  USART1=VT03  921600  */

#ifndef CURRENT_UART_MODE
#define CURRENT_UART_MODE  UART_MODE_DEBUG_WIFI   /* ← 编译时改这里 */
#endif

/* ---- 派生开关 ---- */

/* USART6 跑 VT03 ? */
#define USART6_VT03   (CURRENT_UART_MODE == UART_MODE_DEBUG_VT03)
/* USART6 跑裁判系统 ? */
#define USART6_REFEREE (!USART6_VT03)

/* USART1 跑 VT03 ? */
#define USART1_VT03   (CURRENT_UART_MODE == UART_MODE_COMPETITION)
/* USART1 跑 ESP32 WiFi ? */
#define USART1_WIFI   (!USART1_VT03)

/* 任一 UART 跑 VT03 则启用 VT03 模块编译 */
#define VT03_ENABLE   (USART6_VT03 || USART1_VT03)

/* ---- 编译期冲突检测 ---- */
/* 比赛模式下 USART1 已被 VT03 占用, 不能同时跑 WiFi 遥测 */
#if (CURRENT_UART_MODE == UART_MODE_COMPETITION) && (TELEM_OUTPUT_MODE == TELEM_MODE_WIFI)
  #error "COMPETITION mode requires TELEM_OUTPUT_MODE != TELEM_MODE_WIFI (USART1 is used by VT03)"
#endif

#endif /* UART_MODE_H */
```

**Step 2: 编译验证**

Run: Keil Build (F7)
Expected: 0 Error, 0 Warning（仅头文件，不产生目标代码）

> 注意：如果 `CURRENT_UART_MODE` 设为 2 且 `TELEM_OUTPUT_MODE` 为 `TELEM_MODE_WIFI`，将触发 `#error`。需同时将 `usb_task.h` 中的 `TELEM_OUTPUT_MODE` 改为 `TELEM_MODE_USB` 或 `TELEM_MODE_NONE`。

**Step 3: Commit**

```bash
git add application/uart_mode.h
git commit -m "feat: add UART mode selection header for VT03/WiFi/referee switching"
```

---

### Task 2: 创建 VT03 帧解析模块

**Files:**
- Create: `application/vt03_link.h`
- Create: `application/vt03_link.c`

**Step 1: 编写头文件 `vt03_link.h`**

```c
#ifndef VT03_LINK_H
#define VT03_LINK_H

#include "struct_typedef.h"
#include "remote_control.h"
#include "uart_mode.h"

#if VT03_ENABLE

#define VT03_FRAME_LEN      21u   /* SOF(2) + data(17) + CRC16(2) */
#define VT03_SOF_1           0xA9u
#define VT03_SOF_2           0x53u
#define VT03_CH_VALUE_OFFSET 1024u

/* VT03 扩展字段 (DBUS 没有的) */
typedef struct
{
    uint8_t mode_sw;     /* 2-bit 模式开关 (值域 0~3, 注意不一定与 DBUS 的 1/2/3 对应) */
    uint8_t pause;       /* 1-bit 暂停键 */
    uint8_t fn_1;        /* 1-bit 功能键1 */
    uint8_t fn_2;        /* 1-bit 功能键2 */
    uint8_t trigger;     /* 1-bit 扳机键 */
    uint8_t mouse_mid;   /* 2-bit 鼠标中键 */
} vt03_ext_t;

extern vt03_ext_t vt03_ext;

/* 供 ISR 调用：将一个字节送入状态机 */
void vt03_parse_byte(uint8_t byte);

/* 供 detect_task 调用（可选，目前 detect_task 不使用 data_is_error 回调） */
uint8_t vt03_data_is_error(void);

#endif /* VT03_ENABLE */
#endif /* VT03_LINK_H */
```

**Step 2: 编写源文件 `vt03_link.c`**

```c
#include "vt03_link.h"

#if VT03_ENABLE

#include "detect_task.h"
#include <string.h>

/* ---- 通道校验阈值 (与 remote_control.c 中 RC_CHANNAL_ERROR_VALUE 保持一致) ---- */
#define VT03_CHANNAL_ERROR_VALUE 700

/* ---- CRC-16 表 (与 VT03 官方示例代码一致, init = 0xFFFF) ---- */
static const uint16_t crc16_tab[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static uint16_t vt03_crc16(const uint8_t *p, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    while (len--)
    {
        crc = (uint16_t)(crc >> 8) ^ crc16_tab[(crc ^ (uint16_t)(*p++)) & 0x00FFu];
    }
    return crc;
}

/* ---- 状态机 ---- */
enum { VT03_WAIT_SOF1 = 0, VT03_WAIT_SOF2, VT03_RECV_DATA };

static uint8_t  vt03_state = VT03_WAIT_SOF1;
static uint8_t  vt03_buf[VT03_FRAME_LEN];
static uint8_t  vt03_idx = 0;

/* ---- 外部数据 ---- */
extern RC_ctrl_t rc_ctrl;          /* remote_control.c 中定义 */
vt03_ext_t vt03_ext;

/* ---- 局部辅助 ---- */
static int16_t vt03_abs(int16_t value)
{
    return (value > 0) ? value : (int16_t)(-value);
}

/* ---- 帧解析 → 填充 rc_ctrl ---- */
static void vt03_frame_decode(const uint8_t *frame)
{
    /* CRC-16 校验: 计算前 19 字节, 与末尾 2 字节比较 (little-endian) */
    uint16_t calc = vt03_crc16(frame, VT03_FRAME_LEN - 2u);
    uint16_t recv = (uint16_t)frame[19] | ((uint16_t)frame[20] << 8);
    if (calc != recv)
        return;

    /* --- 解析 64-bit 打包位域 (bytes 2~9, little-endian) --- */
    /*
     * 位域布局 (共 61 bits, bit0 在 byte[2] 的 LSB):
     *   [0:10]   ch_0     11 bits
     *   [11:21]  ch_1     11 bits
     *   [22:32]  ch_2     11 bits
     *   [33:43]  ch_3     11 bits
     *   [44:45]  mode_sw   2 bits
     *   [46]     pause     1 bit
     *   [47]     fn_1      1 bit
     *   [48]     fn_2      1 bit
     *   [49:59]  wheel    11 bits
     *   [60]     trigger   1 bit
     */
    const uint8_t *d = &frame[2];
    uint64_t pk = 0;
    for (int i = 0; i < 8; i++)
        pk |= ((uint64_t)d[i]) << (i * 8);

    rc_ctrl.rc.ch[0] = (int16_t)((pk >>  0) & 0x7FF) - (int16_t)VT03_CH_VALUE_OFFSET;
    rc_ctrl.rc.ch[1] = (int16_t)((pk >> 11) & 0x7FF) - (int16_t)VT03_CH_VALUE_OFFSET;
    rc_ctrl.rc.ch[2] = (int16_t)((pk >> 22) & 0x7FF) - (int16_t)VT03_CH_VALUE_OFFSET;
    rc_ctrl.rc.ch[3] = (int16_t)((pk >> 33) & 0x7FF) - (int16_t)VT03_CH_VALUE_OFFSET;

    vt03_ext.mode_sw = (uint8_t)((pk >> 44) & 0x03);
    vt03_ext.pause   = (uint8_t)((pk >> 46) & 0x01);
    vt03_ext.fn_1    = (uint8_t)((pk >> 47) & 0x01);
    vt03_ext.fn_2    = (uint8_t)((pk >> 48) & 0x01);

    rc_ctrl.rc.ch[4] = (int16_t)((pk >> 49) & 0x7FF) - (int16_t)VT03_CH_VALUE_OFFSET;
    vt03_ext.trigger  = (uint8_t)((pk >> 60) & 0x01);

    /*
     * mode_sw 映射到 s[0]。
     * 注意: VT03 mode_sw 值域 0~3, 而 DBUS 拨杆值域 1/2/3 (RC_SW_UP/DOWN/MID)。
     * 如果 VT03 的 mode_sw 编码与 DBUS 不同, 需要在此加映射表。
     * 当前直接赋值, 上层代码可能需要适配。
     */
    rc_ctrl.rc.s[0] = (char)vt03_ext.mode_sw;
    rc_ctrl.rc.s[1] = RC_SW_DOWN;  /* VT03 无第二拨杆, 设为 DOWN(安全值) 避免 RC_data_is_error 误判 */

    /* --- 鼠标 (bytes 10~15, 即 d[8]~d[13]) --- */
    rc_ctrl.mouse.x = (int16_t)(d[8]  | ((uint16_t)d[9]  << 8));
    rc_ctrl.mouse.y = (int16_t)(d[10] | ((uint16_t)d[11] << 8));
    rc_ctrl.mouse.z = (int16_t)(d[12] | ((uint16_t)d[13] << 8));

    /* 鼠标按键 (byte 16, 即 d[14]) */
    uint8_t mb = d[14];
    rc_ctrl.mouse.press_l = mb & 0x03u;
    rc_ctrl.mouse.press_r = (mb >> 2) & 0x03u;
    vt03_ext.mouse_mid    = (mb >> 4) & 0x03u;

    /* --- 键盘 (bytes 17~18, 即 d[15]~d[16]) --- */
    rc_ctrl.key.v = (uint16_t)(d[15] | ((uint16_t)d[16] << 8));

    /* 通知在线检测 */
    detect_hook(VT03_TOE);
}

/* ---- 公开 API: ISR 每收到一字节调用一次 ---- */
void vt03_parse_byte(uint8_t byte)
{
    switch (vt03_state)
    {
    case VT03_WAIT_SOF1:
        if (byte == VT03_SOF_1)
        {
            vt03_buf[0] = byte;
            vt03_state = VT03_WAIT_SOF2;
        }
        break;

    case VT03_WAIT_SOF2:
        if (byte == VT03_SOF_2)
        {
            vt03_buf[1] = byte;
            vt03_idx = 2;
            vt03_state = VT03_RECV_DATA;
        }
        else if (byte == VT03_SOF_1)
        {
            /* 连续 0xA9, 保持等 SOF2 */
            vt03_buf[0] = byte;
        }
        else
        {
            vt03_state = VT03_WAIT_SOF1;
        }
        break;

    case VT03_RECV_DATA:
        vt03_buf[vt03_idx++] = byte;
        if (vt03_idx >= VT03_FRAME_LEN)
        {
            vt03_frame_decode(vt03_buf);
            vt03_state = VT03_WAIT_SOF1;
        }
        break;

    default:
        vt03_state = VT03_WAIT_SOF1;
        break;
    }
}

/*
 * VT03 专用数据校验。
 * 不能复用 RC_data_is_error(): 该函数检查 s[1]==0 会永远触发错误。
 * 此处只检查通道范围。
 */
uint8_t vt03_data_is_error(void)
{
    if (vt03_abs(rc_ctrl.rc.ch[0]) > VT03_CHANNAL_ERROR_VALUE)
        goto error;
    if (vt03_abs(rc_ctrl.rc.ch[1]) > VT03_CHANNAL_ERROR_VALUE)
        goto error;
    if (vt03_abs(rc_ctrl.rc.ch[2]) > VT03_CHANNAL_ERROR_VALUE)
        goto error;
    if (vt03_abs(rc_ctrl.rc.ch[3]) > VT03_CHANNAL_ERROR_VALUE)
        goto error;
    return 0;

error:
    rc_ctrl.rc.ch[0] = 0;
    rc_ctrl.rc.ch[1] = 0;
    rc_ctrl.rc.ch[2] = 0;
    rc_ctrl.rc.ch[3] = 0;
    rc_ctrl.rc.ch[4] = 0;
    rc_ctrl.rc.s[0] = RC_SW_DOWN;
    rc_ctrl.rc.s[1] = RC_SW_DOWN;
    rc_ctrl.mouse.x = 0;
    rc_ctrl.mouse.y = 0;
    rc_ctrl.mouse.z = 0;
    rc_ctrl.mouse.press_l = 0;
    rc_ctrl.mouse.press_r = 0;
    rc_ctrl.key.v = 0;
    return 1;
}

#endif /* VT03_ENABLE */
```

**Step 3: 将 `vt03_link.c` 加入 Keil 工程**

在 Keil 项目中 Application 组右键 → Add Existing Files → 选择 `application/vt03_link.c`。

**Step 4: 编译验证**

Run: Keil Build (F7)
Expected: 编译错误 — `VT03_TOE` 未定义、`RC_SW_DOWN` 需要 `remote_control.h`（下一个 Task 解决 VT03_TOE）

**Step 5: Commit (暂不编译通过)**

```bash
git add application/vt03_link.h application/vt03_link.c
git commit -m "feat: add VT03 frame parser module with CRC-16 and state machine"
```

---

### Task 3: 在 detect_task 中注册 VT03_TOE

**Files:**
- Modify: `application/detect_task.h` — 在 `errorList` 枚举中添加 `VT03_TOE`
- Modify: `application/detect_task.c` — 在 `error_list` 初始化表中添加 VT03 条目

**Step 1: 修改 detect_task.h**

在 `errorList` 枚举中，`OLED_TOE` 之后、`ERROR_LIST_LENGHT` 之前插入 `VT03_TOE`。

当前代码 (`detect_task.h` 第 79~81 行):
```c
    OLED_TOE,
    ERROR_LIST_LENGHT,
};
```

改为:
```c
    OLED_TOE,
    VT03_TOE,        /* VT03 图传遥控帧在线检测 */
    ERROR_LIST_LENGHT,
};
```

> **关键**: VT03_TOE 必须加在枚举末尾（OLED_TOE 之后），不能插在中间！否则会改变 OLED_TOE 之后所有枚举值，导致 `set_item` 数组索引错位。

**Step 2: 修改 detect_task.c**

在 `detect_init()` 函数中，`set_item` 数组当前有 16 行（对应 DBUS_TOE=0 到 OLED_TOE=15）。在末尾追加 VT03_TOE 行。

当前代码 (`detect_task.c` 第 260~278 行):
```c
    uint16_t set_item[ERROR_LIST_LENGHT][3] =
        {
            {30, 40, 15},   //SBUS
            {10, 10, 11},   //motor1
            {10, 10, 10},   //motor2
            {10, 10, 9},    //motor3
            {10, 10, 8},    //motor4
            {2, 3, 14},     //yaw
            {2, 3, 13},     //pitch
            {10, 10, 12},   //trigger
            {10, 10, 7},    //fric1 motor
            {10, 10, 7},    //fric2 motor
            {2, 3, 7},      //board gyro
            {5, 5, 7},      //board accel
            {40, 200, 7},   //board mag
            {100, 100, 5},  //referee
            {10, 10, 7},    //rm imu
            {100, 100, 1},  //oled
        };
```

改为（追加最后一行）:
```c
    uint16_t set_item[ERROR_LIST_LENGHT][3] =
        {
            {30, 40, 15},   //SBUS
            {10, 10, 11},   //motor1
            {10, 10, 10},   //motor2
            {10, 10, 9},    //motor3
            {10, 10, 8},    //motor4
            {2, 3, 14},     //yaw
            {2, 3, 13},     //pitch
            {10, 10, 12},   //trigger
            {10, 10, 7},    //fric1 motor
            {10, 10, 7},    //fric2 motor
            {2, 3, 7},      //board gyro
            {5, 5, 7},      //board accel
            {40, 200, 7},   //board mag
            {100, 100, 5},  //referee
            {10, 10, 7},    //rm imu
            {100, 100, 1},  //oled
            {100, 3, 7},    //vt03 (offline=100ms, online=3ms, priority=7)
        };
```

> **格式说明**: `{离线判定ms, 上线判定ms, 优先级}`. VT03 帧周期 ~14ms, 100ms 离线阈值足够宽容。

**Step 3: 编译验证**

Run: Keil Build (F7)
Expected: 0 Error（VT03_TOE 已定义，vt03_link.c 中的 `detect_hook(VT03_TOE)` 引用可通过编译）

> 注意: 当 `CURRENT_UART_MODE = 0` (DEBUG_WIFI) 时, `VT03_ENABLE = 0`, vt03_link.c 全部被 `#if` 排除, 不会产生对 `VT03_TOE` 的引用。但 `VT03_TOE` 枚举值仍然存在且 `set_item` 多了一行, 这不影响功能 — detect_task 会为 VT03_TOE 创建一个永远离线的条目, 无副作用。

**Step 4: Commit**

```bash
git add application/detect_task.h application/detect_task.c
git commit -m "feat: register VT03_TOE in detect_task for online/offline monitoring"
```

---

### Task 4: 修改 USART6 支持 VT03 模式

**Files:**
- Modify: `Src/usart.c` — `MX_USART6_UART_Init()` 根据模式设置波特率
- Modify: `application/referee_usart_task.c` — `USART6_IRQHandler()` + `referee_usart_task()` 根据模式选择路径

**Step 1: 修改 `Src/usart.c` 中的 USART6 初始化**

在文件顶部 `/* USER CODE BEGIN 0 */` 块中添加 include:

当前代码 (`Src/usart.c` 第 23~25 行):
```c
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */
```

改为:
```c
/* USER CODE BEGIN 0 */
#include "uart_mode.h"
/* USER CODE END 0 */
```

然后修改 `MX_USART6_UART_Init()` 的波特率 (`Src/usart.c` 第 78~79 行):

当前:
```c
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
```

改为:
```c
  huart6.Instance = USART6;
#if USART6_VT03
  huart6.Init.BaudRate = 921600;
#else
  huart6.Init.BaudRate = 115200;
#endif
```

**Step 2: 修改 `referee_usart_task.c` — 添加 VT03 include**

在 `referee_usart_task.c` 顶部 include 区域（第 22 行 `#include "wifi_bridge.h"` 之后）添加:

```c
#include "uart_mode.h"
#if VT03_ENABLE
#include "vt03_link.h"
#endif
```

**Step 3: 修改 `USART6_IRQHandler()`**

将 `referee_usart_task.c` 第 160~189 行的 `USART6_IRQHandler` 函数**整体替换**为:

```c
void USART6_IRQHandler(void)
{
#if USART6_VT03
    /* --- VT03 模式: RXNE 逐字节 --- */
    if (USART6->SR & USART_SR_RXNE)
    {
        uint8_t byte = (uint8_t)(USART6->DR & 0xFFu);
        vt03_parse_byte(byte);
    }
#else
    /* --- 裁判系统模式: DMA + IDLE 双缓冲 (原有代码) --- */
    if (USART6->SR & UART_FLAG_IDLE)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart6);

        static uint16_t this_time_rx_len = 0;

        if ((huart6.hdmarx->Instance->CR & DMA_SxCR_CT) == RESET)
        {
            __HAL_DMA_DISABLE(huart6.hdmarx);
            this_time_rx_len = USART_RX_BUF_LENGHT - __HAL_DMA_GET_COUNTER(huart6.hdmarx);
            __HAL_DMA_SET_COUNTER(huart6.hdmarx, USART_RX_BUF_LENGHT);
            huart6.hdmarx->Instance->CR |= DMA_SxCR_CT;
            __HAL_DMA_ENABLE(huart6.hdmarx);
            fifo_s_puts(&referee_fifo, (char *)usart6_buf[0], this_time_rx_len);
            detect_hook(REFEREE_TOE);
        }
        else
        {
            __HAL_DMA_DISABLE(huart6.hdmarx);
            this_time_rx_len = USART_RX_BUF_LENGHT - __HAL_DMA_GET_COUNTER(huart6.hdmarx);
            __HAL_DMA_SET_COUNTER(huart6.hdmarx, USART_RX_BUF_LENGHT);
            huart6.hdmarx->Instance->CR &= ~(DMA_SxCR_CT);
            __HAL_DMA_ENABLE(huart6.hdmarx);
            fifo_s_puts(&referee_fifo, (char *)usart6_buf[1], this_time_rx_len);
            detect_hook(REFEREE_TOE);
        }
    }
#endif
}
```

**Step 4: 修改 `referee_usart_task()` 任务函数**

将 `referee_usart_task.c` 第 40~53 行的 `referee_usart_task` 函数**整体替换**为:

```c
void referee_usart_task(void const *argument)
{
    (void)argument;

#if USART6_REFEREE
    /* 裁判系统模式: 初始化 FIFO + DMA 双缓冲 */
    init_referee_struct_data();
    fifo_s_init(&referee_fifo, referee_fifo_buf, REFEREE_FIFO_BUF_LENGTH);
    usart6_init(usart6_buf[0], usart6_buf[1], USART_RX_BUF_LENGHT);
#else
    /* VT03 模式: 不需要 DMA, 只启用 RXNE 中断 */
    /* NVIC 已在 HAL_UART_MspInit 中使能 (usart.c line 252), 这里只需开 RXNE */
    __HAL_UART_ENABLE_IT(&huart6, UART_IT_RXNE);
#endif

    while (1)
    {
#if USART6_REFEREE
        referee_unpack_fifo_data();
#endif
        osDelay(10);
    }
}
```

**Step 5: 编译验证**

Run: Keil Build (F7)
Expected: 0 Error, 0 Warning

**Step 6: Commit**

```bash
git add Src/usart.c application/referee_usart_task.c
git commit -m "feat: USART6 conditional VT03/referee mode via UART_MODE macro"
```

---

### Task 5: 修改 USART1 支持 VT03 模式

**Files:**
- Modify: `Src/usart.c` — `MX_USART1_UART_Init()` 根据模式设置波特率
- Modify: `application/referee_usart_task.c` — `USART1_IRQHandler()` 增加 VT03 分支
- Modify: `application/wifi_bridge.h` — `WIFI_BRIDGE_ENABLE` 受 UART 模式约束
- Modify: `Src/main.c` — USART1 VT03 模式初始化 (RXNE + NVIC)

**Step 1: 修改 wifi_bridge.h**

将 `application/wifi_bridge.h` 第 1~16 行**整体替换**为:

```c
#ifndef WIFI_BRIDGE_H
#define WIFI_BRIDGE_H

#include "struct_typedef.h"
#include "usb_task.h"
#include "uart_mode.h"

#ifndef WIFI_BRIDGE_ENABLE
/* WiFi 桥接仅在 USART1 跑 WiFi 且遥测模式选 WiFi 时启用 */
#if USART1_WIFI && (TELEM_OUTPUT_MODE == TELEM_MODE_WIFI)
#define WIFI_BRIDGE_ENABLE  1
#else
#define WIFI_BRIDGE_ENABLE  0
#endif
#endif

#if WIFI_BRIDGE_ENABLE
uint16_t uart1_rx_available(void);
uint8_t uart1_rx_read_byte(void);
#endif

#endif
```

**Step 2: 修改 `Src/usart.c` 中的 USART1 初始化**

修改 `MX_USART1_UART_Init()` 的波特率 (`Src/usart.c` 第 40~41 行):

当前:
```c
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
```

改为:
```c
  huart1.Instance = USART1;
#if USART1_VT03
  huart1.Init.BaudRate = 921600;
#else
  huart1.Init.BaudRate = 115200;
#endif
```

> 注意: `uart_mode.h` 已在 Task 4 Step 1 中被 include 到 `Src/usart.c`。

**Step 3: 修改 `USART1_IRQHandler()` 及相关代码**

在 `referee_usart_task.c` 中，将第 34~38 行的 WiFi 变量声明:

当前:
```c
#if WIFI_BRIDGE_ENABLE
uint8_t uart1_rx_buf[256];
volatile uint16_t uart1_rx_head = 0u;
volatile uint16_t uart1_rx_tail = 0u;
#endif
```

改为:
```c
#if WIFI_BRIDGE_ENABLE
uint8_t uart1_rx_buf[256];
volatile uint16_t uart1_rx_head = 0u;
volatile uint16_t uart1_rx_tail = 0u;
#endif
```
（此处不变）

然后将第 191~228 行（从 `#if WIFI_BRIDGE_ENABLE` 到 `#endif` 的 USART1 相关代码块）**整体替换**为:

```c
#if USART1_VT03
void USART1_IRQHandler(void)
{
    if (USART1->SR & USART_SR_RXNE)
    {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFFu);
        vt03_parse_byte(byte);
    }
}
#elif WIFI_BRIDGE_ENABLE
void USART1_IRQHandler(void)
{
    if ((USART1->SR & USART_SR_RXNE) != 0u)
    {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFFu);
        uint16_t next = (uint16_t)((uart1_rx_head + 1u) % (uint16_t)sizeof(uart1_rx_buf));

        if (next != uart1_rx_tail)
        {
            uart1_rx_buf[uart1_rx_head] = byte;
            uart1_rx_head = next;
        }
    }
}

uint16_t uart1_rx_available(void)
{
    uint16_t head = uart1_rx_head;
    uint16_t tail = uart1_rx_tail;

    if (head >= tail)
    {
        return (uint16_t)(head - tail);
    }

    return (uint16_t)(sizeof(uart1_rx_buf) - tail + head);
}

uint8_t uart1_rx_read_byte(void)
{
    uint8_t byte;

    byte = uart1_rx_buf[uart1_rx_tail];
    uart1_rx_tail = (uint16_t)((uart1_rx_tail + 1u) % (uint16_t)sizeof(uart1_rx_buf));
    return byte;
}
#endif
```

**Step 4: 修改 `Src/main.c` — USART1 VT03 初始化**

在 `Src/main.c` 顶部添加 include。找到 `/* USER CODE BEGIN Includes */` 块（通常在文件最前面），添加:

```c
/* USER CODE BEGIN Includes */
#include "uart_mode.h"
/* USER CODE END Includes */
```

> 如果 `USER CODE BEGIN Includes` 已有内容，在其内部追加即可。

然后修改 `USER CODE BEGIN 2` 块 (`Src/main.c` 第 144~150 行)。

当前:
```c
  /* USER CODE BEGIN 2 */
    can_filter_init();
    delay_init();
    cali_param_init();
    remote_control_init();
    usart1_tx_dma_init();
  /* USER CODE END 2 */
```

改为:
```c
  /* USER CODE BEGIN 2 */
    can_filter_init();
    delay_init();
    cali_param_init();
    remote_control_init();
#if USART1_VT03
    /* VT03 on USART1: 不需要 DMA, 清除 DMAR/DMAT 后启用 RXNE 中断 */
    CLEAR_BIT(huart1.Instance->CR3, USART_CR3_DMAR);
    CLEAR_BIT(huart1.Instance->CR3, USART_CR3_DMAT);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
#else
    usart1_tx_dma_init();
#endif
  /* USER CODE END 2 */
```

> **关键**: `usart1_tx_dma_init()` 会设置 `USART_CR3_DMAR`（DMA 接收使能），这会阻止 RXNE 中断触发。VT03 模式必须跳过它。CLEAR_BIT 是防御性编程（HAL_UART_Init 不设 DMAR，但以防万一）。

**Step 5: 编译验证**

Run: Keil Build (F7)
Expected: 0 Error

**Step 6: Commit**

```bash
git add Src/usart.c application/referee_usart_task.c application/wifi_bridge.h Src/main.c
git commit -m "feat: USART1 conditional VT03/WiFi mode, auto-disable WiFi bridge in competition"
```

---

### Task 6: 处理 remote_control.c 中的 sbus_to_usart1 条件

**Files:**
- Modify: `application/remote_control.c` — 两处 `sbus_to_usart1()` 调用受模式约束

**Step 1: 添加 include**

在 `remote_control.c` 顶部 include 区域（第 29 行 `#include "wifi_bridge.h"` 之后）添加:

```c
#include "uart_mode.h"
```

**Step 2: 修改第一处 sbus_to_usart1 调用**

当前代码 (`remote_control.c` 第 190~192 行, Memory 0 分支):
```c
#if !WIFI_BRIDGE_ENABLE
                sbus_to_usart1(sbus_rx_buf[0]);
#endif
```

改为:
```c
#if !WIFI_BRIDGE_ENABLE && !USART1_VT03
                sbus_to_usart1(sbus_rx_buf[0]);
#endif
```

**Step 3: 修改第二处 sbus_to_usart1 调用**

当前代码 (`remote_control.c` 第 224~226 行, Memory 1 分支):
```c
#if !WIFI_BRIDGE_ENABLE
                sbus_to_usart1(sbus_rx_buf[1]);
#endif
```

改为:
```c
#if !WIFI_BRIDGE_ENABLE && !USART1_VT03
                sbus_to_usart1(sbus_rx_buf[1]);
#endif
```

> **两处都要改!** USART3_IRQHandler 中有两个 DMA 缓冲区翻转分支 (Memory 0 和 Memory 1)，各有一个 `sbus_to_usart1` 调用。

**Step 4: 编译验证**

Run: Keil Build (F7)
Expected: 0 Error

**Step 5: Commit**

```bash
git add application/remote_control.c
git commit -m "fix: suppress sbus_to_usart1 when USART1 is in VT03 mode"
```

---

### Task 7: 更新文档

**Files:**
- Modify: `docs/05_BOARD_PHYSICAL_MAP.md` — 更新 VT03 接入方案为已实现的三模式切换

**Step 1: 更新第 8 节**

将 `docs/05_BOARD_PHYSICAL_MAP.md` 中的 VT03 推荐方案部分替换为实际实现的三模式架构描述：

- 说明 `CURRENT_UART_MODE` 宏位置 (`application/uart_mode.h`) 和三个可选值
- 说明比赛时物理接线变更（USART1 4-pin 从 ESP32 换到 VT03）
- 说明编译步骤和注意事项（COMPETITION 模式需同时改 `TELEM_OUTPUT_MODE`）

**Step 2: Commit**

```bash
git add docs/05_BOARD_PHYSICAL_MAP.md
git commit -m "docs: update VT03 integration with implemented three-mode UART switching"
```

---

## 实现顺序总结

```
Task 1 → uart_mode.h (模式宏 + 冲突检测)
Task 2 → vt03_link.h/c (帧解析 + 独立错误检测)
Task 3 → detect_task.h/c (VT03_TOE 枚举末尾 + set_item 追加行)
Task 4 → USART6 条件编译 (usart.c 波特率 + referee_usart_task.c IRQ+任务)
Task 5 → USART1 条件编译 (usart.c 波特率 + referee_usart_task.c IRQ + wifi_bridge.h + main.c 初始化)
Task 6 → remote_control.c 守卫 (两处 sbus_to_usart1)
Task 7 → 文档更新
```

每个 Task 独立可提交。Task 2 需要 Task 3 的 VT03_TOE 才能编译，可合并提交或先做 Task 3。

## 验证清单

- [ ] `CURRENT_UART_MODE = 0`：编译通过，行为与改动前完全一致（回归验证）
- [ ] `CURRENT_UART_MODE = 1`：编译通过，USART6 跑 921600，接 VT03 可收到遥控帧
- [ ] `CURRENT_UART_MODE = 2`：编译通过，USART1 跑 921600 + USART6 跑 115200
- [ ] `CURRENT_UART_MODE = 2` + `TELEM_OUTPUT_MODE = TELEM_MODE_WIFI`：触发 `#error`
- [ ] VT03 帧解析后 `rc_ctrl.rc.ch[0~3]` 范围 [-660, +660]，`rc_ctrl.key.v` 按键正确
- [ ] `detect_hook(VT03_TOE)` 正常刷新，VT03 断线后 `toe_is_error(VT03_TOE)` 返回 1
- [ ] mode_sw → s[0] 映射是否匹配云台/底盘模式切换逻辑（可能需要映射表适配）

## 已知限制与后续工作

1. **mode_sw 映射**: VT03 mode_sw (0~3) 可能与 DBUS 拨杆编码 (UP=1, MID=3, DOWN=2) 不同。上线前需用实际 VT03 硬件测试 mode_sw 值，必要时在 `vt03_frame_decode` 中添加映射表。
2. **DBUS + VT03 同时在线**: 当前实现中 DBUS (USART3) 始终活跃。DBUS 和 VT03 会竞争写入同一个 `rc_ctrl`，需要后续加优先级仲裁或互斥。
3. **mouse 按键位宽**: DBUS 的 `press_l`/`press_r` 是 1-bit (0/1)，VT03 是 2-bit (0~3)。当前直接赋值到 `uint8_t` 成员，兼容但上层代码如果只判断 0/1 可能需要适配。

---

## 执行状态（2026-02-28 by Codex）

- [x] Task 1：新增 `application/uart_mode.h`
- [x] Task 2：新增 `application/vt03_link.h` / `application/vt03_link.c`
- [x] Task 3：`detect_task.h/.c` 注册 `VT03_TOE`
- [x] Task 4：`Src/usart.c` + `application/referee_usart_task.c` 完成 USART6 条件路径
- [x] Task 5：`Src/usart.c` + `application/referee_usart_task.c` + `application/wifi_bridge.h` + `Src/main.c` 完成 USART1 条件路径
- [x] Task 6：`application/remote_control.c` 两处 `sbus_to_usart1` 加入 `!USART1_VT03` 防护
- [x] Task 7：`docs/05_BOARD_PHYSICAL_MAP.md` 更新为实施版三模式说明

补充执行项：
- [x] `MDK-ARM/standard_robot.uvprojx` 已加入 `application/vt03_link.c`
- [x] docs 同步：`02_ARCHITECTURE.md` / `03_TASK_MAP.md` / `04_MODULE_MAP.md`
- [x] ai 交接同步：`ai_sessions` / `risk_log.md` / `CURRENT_STATE.md` / `INDEX.md`
