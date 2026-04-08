/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       chassis_power_control.c
  * @brief      chassis power control via motor power model
  * @note       Ports only the power prediction model and current limiting
  *             logic. This version intentionally excludes any supercap branch.
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#include "chassis_power_control.h"

#include "arm_math.h"

#include "detect_task.h"
#include "referee.h"

static fp32 motor_power_estimate(fp32 current, fp32 speed_rpm)
{
    return MOTOR_TORQUE_COEFF * current * speed_rpm
         + MOTOR_K2 * speed_rpm * speed_rpm
         + MOTOR_A * current * current
         + MOTOR_CONSTANT;
}

static fp32 quadratic_solve_current(fp32 target_power, fp32 speed_rpm, fp32 original_out)
{
    fp32 b = MOTOR_TORQUE_COEFF * speed_rpm;
    fp32 c = MOTOR_K2 * speed_rpm * speed_rpm + MOTOR_CONSTANT - target_power;
    fp32 discriminant = b * b - 4.0f * MOTOR_A * c;
    fp32 sqrt_disc = 0.0f;
    fp32 new_current;

    if (discriminant < 0.0f)
    {
        return original_out;
    }

    arm_sqrt_f32(discriminant, &sqrt_disc);

    if (original_out >= 0.0f)
    {
        new_current = (-b + sqrt_disc) / (2.0f * MOTOR_A);
    }
    else
    {
        new_current = (-b - sqrt_disc) / (2.0f * MOTOR_A);
    }

    return fp32_constrain(new_current, -MAX_MOTOR_CAN_CURRENT, MAX_MOTOR_CAN_CURRENT);
}

void chassis_power_control(chassis_move_t *chassis_power_control)
{
    uint8_t i;
    uint8_t robot_id = get_robot_id();
    fp32 chassis_power = 0.0f;
    fp32 chassis_power_buffer = 0.0f;
    fp32 total_power = 0.0f;
    fp32 power_limit = 0.0f;
    fp32 effective_limit = 0.0f;
    fp32 motor_power[4];

    if (toe_is_error(REFEREE_TOE) ||
        robot_id == RED_ENGINEER || robot_id == BLUE_ENGINEER || robot_id == 0)
    {
        return;
    }

    get_chassis_power_and_buffer(&chassis_power, &chassis_power_buffer);
    (void)chassis_power;
    power_limit = (fp32)get_chassis_power_limit();
    effective_limit = power_limit - PID_calc(&chassis_power_control->buffer_pid,
                                             chassis_power_buffer,
                                             BUFFER_ENERGY_SETPOINT);
    effective_limit = fp32_constrain(effective_limit, EFFECTIVE_POWER_LIMIT_MIN, power_limit);

    for (i = 0; i < 4; i++)
    {
        fp32 current = chassis_power_control->motor_speed_pid[i].out;
        fp32 speed_rpm = (fp32)chassis_power_control->motor_chassis[i].chassis_motor_measure->speed_rpm;

        motor_power[i] = motor_power_estimate(current, speed_rpm);
        if (motor_power[i] > 0.0f)
        {
            total_power += motor_power[i];
        }
    }

    chassis_power_control->power_est = total_power;
    chassis_power_control->effective_limit = effective_limit;

    if (total_power > effective_limit && total_power > 0.0f)
    {
        fp32 power_scale = effective_limit / total_power;

        for (i = 0; i < 4; i++)
        {
            fp32 target_power;
            fp32 speed_rpm;

            if (motor_power[i] <= 0.0f)
            {
                continue;
            }

            target_power = motor_power[i] * power_scale;
            if (target_power < 0.0f)
            {
                continue;
            }
            speed_rpm = (fp32)chassis_power_control->motor_chassis[i].chassis_motor_measure->speed_rpm;
            chassis_power_control->motor_speed_pid[i].out = quadratic_solve_current(
                target_power,
                speed_rpm,
                chassis_power_control->motor_speed_pid[i].out);
        }
    }

    if (chassis_power_buffer < BUFFER_EMERGENCY_THRESHOLD)
    {
        fp32 emergency_scale = fp32_constrain(chassis_power_buffer / BUFFER_EMERGENCY_THRESHOLD, 0.0f, 1.0f);
        for (i = 0; i < 4; i++)
        {
            chassis_power_control->motor_speed_pid[i].out *= emergency_scale;
        }
    }
}
