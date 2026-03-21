#ifndef UART_MODE_H
#define UART_MODE_H

#include "usb_task.h"   // 需要读取 TELEM_OUTPUT_MODE / TELEM_MODE_WIFI

/*
 * ============================================================
 * UART 三模式总开关（编译期）
 *
 * 你只需要改这一行：
 *   #define CURRENT_UART_MODE  ...
 *
 * 可选模式：
 * 1) UART_MODE_DEBUG_WIFI
 *    USART6 = 裁判系统(115200)
 *    USART1 = WiFi/ESP32(115200)
 *    适合：日常 USB/WiFi 调参、不开 VT03
 *
 * 2) UART_MODE_DEBUG_VT03
 *    USART6 = VT03 图传(921600)
 *    USART1 = WiFi/ESP32(115200)
 *    适合：联调 VT03 键鼠 + 保留 WiFi 调参
 *
 * 3) UART_MODE_COMPETITION
 *    USART6 = 裁判系统(115200)
 *    USART1 = VT03 图传(921600)
 *    适合：比赛接线（裁判 + 图传）
 *
 * 注意：
 * - 比赛模式下 USART1 被 VT03 占用，不能再给 WiFi 遥测。
 * - 若 CURRENT_UART_MODE=UART_MODE_COMPETITION 且
 *   TELEM_OUTPUT_MODE=TELEM_MODE_WIFI，会在编译期报错。
 * ============================================================
 */

// ---- UART 模式枚举 ----
#define UART_MODE_DEBUG_WIFI   0   // USART6=Referee 115200, USART1=ESP32/WiFi 115200
#define UART_MODE_DEBUG_VT03   1   // USART6=VT03    921600, USART1=ESP32/WiFi 115200
#define UART_MODE_COMPETITION  2   // USART6=Referee 115200, USART1=VT03      921600

#ifndef CURRENT_UART_MODE
// ===== 当前生效模式（改这里）=====
#define CURRENT_UART_MODE UART_MODE_DEBUG_WIFI
// 例如切到比赛模式：
// #define CURRENT_UART_MODE  UART_MODE_COMPETITION
#endif

// ---- 派生开关（下游模块直接使用）----
#define USART6_VT03      (CURRENT_UART_MODE == UART_MODE_DEBUG_VT03)
#define USART6_REFEREE   (!USART6_VT03)

#define USART1_VT03      (CURRENT_UART_MODE == UART_MODE_COMPETITION)
#define USART1_WIFI      (!USART1_VT03)

#define VT03_ENABLE      (USART6_VT03 || USART1_VT03)

// ---- 编译期冲突保护 ----
#if (CURRENT_UART_MODE == UART_MODE_COMPETITION) && (TELEM_OUTPUT_MODE == TELEM_MODE_WIFI)
#error "COMPETITION mode requires TELEM_OUTPUT_MODE != TELEM_MODE_WIFI (USART1 is used by VT03)"
#endif

#endif
