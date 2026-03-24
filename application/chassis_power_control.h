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

/* Model-based limiter target power. Keep a margin below the hard rule limit. */
#define CHASSIS_MODEL_POWER_LIMIT       100.0f

/* Fallback total-current clamp when referee data is unavailable. */
#define NO_JUDGE_TOTAL_CURRENT_LIMIT    64000.0f

/* Buffer-energy safety thresholds. */
#define BUFFER_DERATING_THRESH          10.0f
#define BUFFER_EMERGENCY_THRESH         2.0f

/* M3508 + ESC fitted power model coefficients. */
#define MOTOR_TORQUE_COEFF              1.99688994e-6f
#define MOTOR_A_COEFF                   1.23e-07f
#define MOTOR_K2                        1.453e-07f
#define MOTOR_CONST_TERM                4.081f

extern void chassis_power_control(chassis_move_t *chassis_power_control);

#endif
