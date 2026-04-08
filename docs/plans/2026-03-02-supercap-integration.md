# 超级电容主控端集成 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 在主控 STM32F407 上集成超级电容 CAN 通信模块，实现功率增强和离线回退。

**Architecture:** 新建 `supercap.c/h` 封装 CAN 收发与状态管理；修改 `CAN_receive.c` 路由 ID 0x051；修改 `detect_task` 增加离线检测；修改 `chassis_power_control.c` 利用超级电容返回的增强功率限制，离线时零改动回退到原始 80W 逻辑。

**Tech Stack:** STM32F407 HAL, FreeRTOS (CMSIS-RTOS v1), CAN 1Mbps, C (armcc/Keil)

**CAN 协议（与超级电容控制器匹配）：**
- 主控 → 超级电容: ID `0x061`, 8 bytes
  - byte 0: bit0=enableDCDC, bit1=systemRestart
  - byte 1-2: feedbackRefereePowerLimit (uint16, 30~250W)
  - byte 3-4: feedbackRefereeEnergyBuffer (uint16, 0~300J)
  - byte 5-7: reserved
- 超级电容 → 主控: ID `0x051`, 8 bytes
  - byte 0: errorCode (bit flags)
  - byte 1-4: chassisPower (float, 当前可用功率 W)
  - byte 5-6: chassisPowerLimit (uint16, 功率上限 W)
  - byte 7: capEnergy (0~255, 255=满电)

---

### Task 1: 新建 supercap.h 头文件

**Files:**
- Create: `application/supercap.h`

**Step 1: 创建头文件**

```c
#ifndef SUPERCAP_H
#define SUPERCAP_H

#include "struct_typedef.h"

/* CAN IDs — 与超级电容控制器 Communication.cpp 匹配 */
#define SUPERCAP_TX_ID  0x061   /* 主控 → 超级电容 */
#define SUPERCAP_RX_ID  0x051   /* 超级电容 → 主控 */

/* 超级电容错误码 bit flags (来自 PowerManager.hpp) */
#define SUPERCAP_ERR_UNDER_VOLTAGE      0x01
#define SUPERCAP_ERR_OVER_VOLTAGE       0x02
#define SUPERCAP_ERR_BUCK_BOOST         0x04
#define SUPERCAP_ERR_SHORT_CIRCUIT      0x08
#define SUPERCAP_ERR_HIGH_TEMP          0x10
#define SUPERCAP_ERR_NO_POWER_INPUT     0x20
#define SUPERCAP_ERR_CAPACITOR          0x40
#define SUPERCAP_ERR_OUTPUT_DISABLED    0x80

/* 超级电容接收数据 (ID 0x051, 8 bytes, __packed) */
typedef __packed struct
{
    uint8_t  error_code;        /* byte 0 */
    float    chassis_power;     /* byte 1-4: 当前可用底盘功率 (W) */
    uint16_t chassis_power_limit; /* byte 5-6: 底盘功率上限 (W) */
    uint8_t  cap_energy;        /* byte 7: 电容能量 0~255 */
} supercap_measure_t;

/**
 * @brief  CAN 接收回调中调用，解析 ID 0x051 数据
 * @param  rx_data: 8 bytes CAN 数据
 */
extern void supercap_can_rx_handler(const uint8_t *rx_data);

/**
 * @brief  向超级电容发送控制命令 (ID 0x061)
 *         在 chassis_task 主循环中每 2ms 调用一次
 * @param  enable_dcdc: 1=使能输出, 0=关闭
 * @param  power_limit: 裁判系统功率限制 (W)
 * @param  energy_buffer: 裁判系统缓冲能量 (J)
 */
extern void supercap_send_cmd(uint8_t enable_dcdc, uint16_t power_limit, uint16_t energy_buffer);

/**
 * @brief  获取超级电容测量数据只读指针
 */
extern const supercap_measure_t *get_supercap_measure_point(void);

/**
 * @brief  超级电容是否在线且无致命错误
 * @return 1=可用, 0=不可用
 */
extern bool_t supercap_is_available(void);

#endif
```

**Step 2: 确认文件创建成功**

检查 `application/supercap.h` 存在且内容完整。

---

### Task 2: 新建 supercap.c 实现文件

**Files:**
- Create: `application/supercap.c`

**Step 1: 创建实现文件**

```c
#include "supercap.h"
#include "main.h"
#include "detect_task.h"

extern CAN_HandleTypeDef hcan1;

static supercap_measure_t supercap_measure;
static CAN_TxHeaderTypeDef supercap_tx_header;
static uint8_t supercap_tx_data[8];

void supercap_can_rx_handler(const uint8_t *rx_data)
{
    /* 直接内存拷贝，结构体与线上格式一致 (__packed) */
    memcpy(&supercap_measure, rx_data, sizeof(supercap_measure_t));
    detect_hook(SUPERCAP_TOE);
}

void supercap_send_cmd(uint8_t enable_dcdc, uint16_t power_limit, uint16_t energy_buffer)
{
    uint32_t send_mail_box;
    supercap_tx_header.StdId = SUPERCAP_TX_ID;
    supercap_tx_header.IDE   = CAN_ID_STD;
    supercap_tx_header.RTR   = CAN_RTR_DATA;
    supercap_tx_header.DLC   = 0x08;

    /* byte 0: bit0=enableDCDC, bit1=systemRestart(0) */
    supercap_tx_data[0] = enable_dcdc & 0x01;
    /* byte 1-2: feedbackRefereePowerLimit (uint16, little-endian) */
    supercap_tx_data[1] = (uint8_t)(power_limit & 0xFF);
    supercap_tx_data[2] = (uint8_t)(power_limit >> 8);
    /* byte 3-4: feedbackRefereeEnergyBuffer (uint16, little-endian) */
    supercap_tx_data[3] = (uint8_t)(energy_buffer & 0xFF);
    supercap_tx_data[4] = (uint8_t)(energy_buffer >> 8);
    /* byte 5-7: reserved */
    supercap_tx_data[5] = 0;
    supercap_tx_data[6] = 0;
    supercap_tx_data[7] = 0;

    HAL_CAN_AddTxMessage(&hcan1, &supercap_tx_header, supercap_tx_data, &send_mail_box);
}

const supercap_measure_t *get_supercap_measure_point(void)
{
    return &supercap_measure;
}

bool_t supercap_is_available(void)
{
    /* 在线 && 无致命错误 && 输出已使能 */
    if (toe_is_error(SUPERCAP_TOE))
        return 0;
    /* bit7 = output disabled */
    if (supercap_measure.error_code & SUPERCAP_ERR_OUTPUT_DISABLED)
        return 0;
    /* 致命错误 mask: 短路、过压、欠压、DCDC故障 */
    if (supercap_measure.error_code & (SUPERCAP_ERR_SHORT_CIRCUIT | SUPERCAP_ERR_OVER_VOLTAGE | SUPERCAP_ERR_UNDER_VOLTAGE | SUPERCAP_ERR_BUCK_BOOST))
        return 0;
    return 1;
}
```

**Step 2: 确认编译所需的 `#include <string.h>` 或 `memcpy` 可用**

本项目通过 `main.h` 间接包含了 `string.h`，无需额外添加。

---

### Task 3: detect_task 增加 SUPERCAP_TOE

**Files:**
- Modify: `application/detect_task.h` (errorList enum)
- Modify: `application/detect_task.c` (set_item 数组)

**Step 1: 在 detect_task.h 的 errorList 中添加 SUPERCAP_TOE**

在 `VT03_TOE` 后、`ERROR_LIST_LENGHT` 前插入：

```c
    VT03_TOE,              // VT03 ?????????
    SUPERCAP_TOE,          // 超级电容控制器 (CAN1 ID 0x051)
    ERROR_LIST_LENGHT,
```

**Step 2: 在 detect_task.c 的 set_item 数组末尾添加超级电容参数**

在 `{200, 3, 7}` (vt03) 后添加：

```c
            {200, 3, 7},    //vt03 (offline=200ms, online=3ms, priority=7)
            {500, 100, 3},  //supercap (offline=500ms, online=100ms, priority=3)
```

超级电容通信频率 500Hz (2ms)，设 500ms 离线阈值容忍短暂丢包。优先级 3（低），离线不影响安全（自动回退到原始限功率）。

**Step 3: 确认 ERROR_LIST_LENGHT 自动 +1，无需其他修改**

---

### Task 4: CAN_receive.c 路由超级电容接收

**Files:**
- Modify: `application/CAN_receive.c` (hcan1 switch-case)

**Step 1: 在文件头添加 include**

```c
#include "supercap.h"
```

**Step 2: 在 hcan1 的 switch 中增加 case**

在 `CAN_3508_M4_ID` case 之后、`default` 之前插入：

```c
            case SUPERCAP_RX_ID:  /* 0x051 */
            {
                supercap_can_rx_handler(rx_data);
                break;
            }
```

---

### Task 5: chassis_task.c 中发送超级电容命令

**Files:**
- Modify: `application/chassis_task.c`

**Step 1: 在文件头添加 include**

```c
#include "supercap.h"
#include "referee.h"
```

**Step 2: 在主循环的 CAN_cmd_chassis 发送之后、vTaskDelay 之前，添加超级电容命令发送**

在 `chassis_task` 的 while(1) 循环中，`vTaskDelay(CHASSIS_CONTROL_TIME_MS)` 之前插入：

```c
        /* 向超级电容发送裁判系统功率数据 */
        {
            fp32 ref_power = 0.0f, ref_buffer = 0.0f;
            get_chassis_power_and_buffer(&ref_power, &ref_buffer);
            /* s0=MID 或 UP 时使能超级电容输出，其他模式关闭 */
            uint8_t enable = (chassis_move.chassis_mode != CHASSIS_VECTOR_RAW) ? 1 : 0;
            supercap_send_cmd(enable, (uint16_t)80, (uint16_t)ref_buffer);
        }
```

注：`power_limit` 暂时硬编码 80W（与现有 POWER_LIMIT 一致）。后续可从裁判系统 robot_state 动态获取。

---

### Task 6: chassis_power_control.c 集成超级电容功率增强

**Files:**
- Modify: `application/chassis_power_control.c`

**Step 1: 添加 include**

```c
#include "supercap.h"
```

**Step 2: 修改 chassis_power_control 函数**

替换现有函数为以下逻辑（保留原始缩放算法，增加超级电容分支）：

```c
void chassis_power_control(chassis_move_t *chassis_power_control)
{
    fp32 chassis_power = 0.0f;
    fp32 chassis_power_buffer = 0.0f;
    fp32 total_current_limit = 0.0f;
    fp32 total_current = 0.0f;
    uint8_t robot_id = get_robot_id();
    if(toe_is_error(REFEREE_TOE))
    {
        total_current_limit = NO_JUDGE_TOTAL_CURRENT_LIMIT;
    }
    else if(robot_id == RED_ENGINEER || robot_id == BLUE_ENGINEER || robot_id == 0)
    {
        total_current_limit = NO_JUDGE_TOTAL_CURRENT_LIMIT;
    }
    else if(supercap_is_available())
    {
        /* ---- 超级电容在线: 使用增强功率限制 ---- */
        const supercap_measure_t *cap = get_supercap_measure_point();
        fp32 cap_power_limit = (fp32)cap->chassis_power_limit;
        fp32 cap_warning_power = cap_power_limit * 0.5f;

        get_chassis_power_and_buffer(&chassis_power, &chassis_power_buffer);

        if(chassis_power_buffer < WARNING_POWER_BUFF)
        {
            fp32 power_scale;
            if(chassis_power_buffer > 5.0f)
            {
                power_scale = chassis_power_buffer / WARNING_POWER_BUFF;
            }
            else
            {
                power_scale = 5.0f / WARNING_POWER_BUFF;
            }
            total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT * power_scale;
        }
        else
        {
            if(chassis_power > cap_warning_power)
            {
                fp32 power_scale;
                if(chassis_power < cap_power_limit)
                {
                    power_scale = (cap_power_limit - chassis_power) / (cap_power_limit - cap_warning_power);
                }
                else
                {
                    power_scale = 0.0f;
                }
                total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT + POWER_TOTAL_CURRENT_LIMIT * power_scale;
            }
            else
            {
                total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT + POWER_TOTAL_CURRENT_LIMIT;
            }
        }
    }
    else
    {
        /* ---- 原始逻辑: 裁判系统 80W 限制 ---- */
        get_chassis_power_and_buffer(&chassis_power, &chassis_power_buffer);
        if(chassis_power_buffer < WARNING_POWER_BUFF)
        {
            fp32 power_scale;
            if(chassis_power_buffer > 5.0f)
            {
                power_scale = chassis_power_buffer / WARNING_POWER_BUFF;
            }
            else
            {
                power_scale = 5.0f / WARNING_POWER_BUFF;
            }
            total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT * power_scale;
        }
        else
        {
            if(chassis_power > WARNING_POWER)
            {
                fp32 power_scale;
                if(chassis_power < POWER_LIMIT)
                {
                    power_scale = (POWER_LIMIT - chassis_power) / (POWER_LIMIT - WARNING_POWER);
                }
                else
                {
                    power_scale = 0.0f;
                }
                total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT + POWER_TOTAL_CURRENT_LIMIT * power_scale;
            }
            else
            {
                total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT + POWER_TOTAL_CURRENT_LIMIT;
            }
        }
    }

    total_current = 0.0f;
    for(uint8_t i = 0; i < 4; i++)
    {
        total_current += fabs(chassis_power_control->motor_speed_pid[i].out);
    }

    if(total_current > total_current_limit)
    {
        fp32 current_scale = total_current_limit / total_current;
        chassis_power_control->motor_speed_pid[0].out *= current_scale;
        chassis_power_control->motor_speed_pid[1].out *= current_scale;
        chassis_power_control->motor_speed_pid[2].out *= current_scale;
        chassis_power_control->motor_speed_pid[3].out *= current_scale;
    }
}
```

核心变化：
- 在 `裁判系统离线` 和 `工程机/无ID` 判断之后，先检查 `supercap_is_available()`
- 超级电容在线时，用 `cap->chassis_power_limit` 替代硬编码的 `POWER_LIMIT (80W)`
- `warning_power` 动态调整为 `cap_power_limit * 0.5`
- 超级电容离线/错误时，走原始 80W 逻辑（零改动回退）

---

### Task 7: 将 supercap.c 添加到 Keil 工程

**Files:**
- Modify: `MDK-ARM/standard_robot.uvprojx` (手动在 Keil 中操作)

**Step 1: 在 Keil 中添加源文件**

在 Keil 的 Project 面板中，右键 `Application/User` 组 → Add Existing Files → 选择 `application/supercap.c`。

**Step 2: 编译验证**

按 F7 编译，预期 0 Error。可能出现 warnings 需要处理。

---

### Task 8: 编译修复和验证

**Step 1: 编译整个工程**

在 Keil 中 Rebuild All (F7)，检查错误和警告。

**Step 2: 常见问题排查**

- 如果 `memcpy` 未声明：在 `supercap.c` 中添加 `#include <string.h>`
- 如果 `supercap_is_available` 类型不匹配：检查 `bool_t` 定义
- 如果 `get_chassis_power_and_buffer` 未声明：确认 `referee.h` 已 include

**Step 3: Commit**

```bash
git add application/supercap.c application/supercap.h
git add application/CAN_receive.c application/detect_task.c application/detect_task.h
git add application/chassis_task.c application/chassis_power_control.c
git commit -m "feat: 集成超级电容 CAN 通信与功率增强

- 新建 supercap.c/h: CAN 收发 (0x051/0x061)、状态管理、离线检测
- CAN_receive.c: hcan1 路由 ID 0x051
- detect_task: 添加 SUPERCAP_TOE (500ms offline, priority=3)
- chassis_task: 每 2ms 发送裁判系统功率数据给超级电容
- chassis_power_control: 超级电容在线时使用增强功率限制，离线零改动回退"
```

---

## 安全设计要点

1. **零改动回退**: 超级电容离线/出错时，`chassis_power_control` 走原始 80W 逻辑，行为与未接超级电容完全一致
2. **致命错误屏蔽**: 短路、过压、欠压、DCDC 故障时 `supercap_is_available()` 返回 0，不使用其增强功率
3. **enable 控制**: RAW 模式（s0=DOWN, 零力）时向超级电容发送 enableDCDC=0，防止意外输出
4. **缓冲能量保护**: 即使超级电容增强功率，缓冲能量低于 50J 时仍执行原始的缓冲能量缩放保护

## 禁止事项

- **不要修改** 超级电容控制器端的 CAN 协议（Communication.cpp 中的 RxData/TxData 结构体）
- **不要修改** 超级电容控制器端的 PID 参数
- **不要修改** 底盘电机 PID 参数
- **不要移除** 原始 80W 限功率逻辑（它是离线回退路径）
