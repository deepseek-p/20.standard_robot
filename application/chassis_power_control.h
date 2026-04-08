/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       chassis_power_control.c/h
  * @brief      chassis power control
  * @note       Uses a motor power prediction model and quadratic back-solve
  *             to limit chassis motor output when estimated power exceeds the
  *             configured budget.
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#ifndef CHASSIS_POWER_CONTROL_H
#define CHASSIS_POWER_CONTROL_H

#include "chassis_task.h"
#include "main.h"

/*
 * 功率限制模式切换说明：
 * 1) 临时快速切换：直接把下面的 POWER_LIMIT_AGGRESSIVE 从 0 改成 1。
 * 2) 推荐工程切换：在 Keil 工程里添加编译宏 POWER_LIMIT_AGGRESSIVE=1。
 *
 * 0 = 保守版
 *     - buffer 目标值为 50J
 *     - chassis_task.c 中使用更温和的 buffer PID 参数
 *
 * 1 = 激进版
 *     - buffer 目标值为 45J
 *     - chassis_task.c 中使用更激进的 buffer PID 参数
 *
 * 注意：
 * - 头文件里的默认值和 Keil 工程里的编译宏要保持一致。
 * - 不要同时在多个地方切模式，避免编出来的固件和预期不一致。
 */
#ifndef POWER_LIMIT_AGGRESSIVE
#define POWER_LIMIT_AGGRESSIVE          0   /* 0=V1 conservative, 1=V2 aggressive */
#endif

#if POWER_LIMIT_AGGRESSIVE
#define BUFFER_ENERGY_SETPOINT          45.0f
#else
#define BUFFER_ENERGY_SETPOINT          50.0f
#endif
#define BUFFER_EMERGENCY_THRESHOLD      20.0f
#define EFFECTIVE_POWER_LIMIT_MIN       20.0f

/* M3508 + ESC fitted power model coefficients. */
#define MOTOR_TORQUE_COEFF              1.99688994e-6f
#define MOTOR_K2                        1.453e-07f
#define MOTOR_A                         1.23e-07f
#define MOTOR_CONSTANT                  4.081f

extern void chassis_power_control(chassis_move_t *chassis_power_control);

#endif
