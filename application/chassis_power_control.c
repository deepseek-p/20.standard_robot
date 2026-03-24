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
         + MOTOR_A_COEFF * current * current
         + MOTOR_CONST_TERM;
}

static fp32 quadratic_solve_current(fp32 target_power, fp32 speed_rpm, fp32 original_out)
{
    fp32 b = MOTOR_TORQUE_COEFF * speed_rpm;
    fp32 c = MOTOR_K2 * speed_rpm * speed_rpm + MOTOR_CONST_TERM - target_power;
    fp32 discriminant = b * b - 4.0f * MOTOR_A_COEFF * c;
    fp32 sqrt_disc = 0.0f;
    fp32 new_current;

    if (discriminant < 0.0f)
    {
        return original_out;
    }

    arm_sqrt_f32(discriminant, &sqrt_disc);

    if (original_out >= 0.0f)
    {
        new_current = (-b + sqrt_disc) / (2.0f * MOTOR_A_COEFF);
    }
    else
    {
        new_current = (-b - sqrt_disc) / (2.0f * MOTOR_A_COEFF);
    }

    return fp32_constrain(new_current, -MAX_MOTOR_CAN_CURRENT, MAX_MOTOR_CAN_CURRENT);
}

void chassis_power_control(chassis_move_t *chassis_power_control)
{
    uint8_t i;
    uint8_t robot_id = get_robot_id();
    fp32 chassis_power = 0.0f;
    fp32 chassis_power_buffer = 0.0f;
    fp32 total_current = 0.0f;
    fp32 total_power = 0.0f;
    fp32 max_power = CHASSIS_MODEL_POWER_LIMIT;
    fp32 motor_power[4];

    if (toe_is_error(REFEREE_TOE) ||
        robot_id == RED_ENGINEER || robot_id == BLUE_ENGINEER || robot_id == 0)
    {
        for (i = 0; i < 4; i++)
        {
            fp32 out = chassis_power_control->motor_speed_pid[i].out;
            total_current += (out >= 0.0f) ? out : -out;
        }

        if (total_current > NO_JUDGE_TOTAL_CURRENT_LIMIT)
        {
            fp32 scale = NO_JUDGE_TOTAL_CURRENT_LIMIT / total_current;
            for (i = 0; i < 4; i++)
            {
                chassis_power_control->motor_speed_pid[i].out *= scale;
            }
        }
        return;
    }

    get_chassis_power_and_buffer(&chassis_power, &chassis_power_buffer);
    (void)chassis_power;

    if (chassis_power_buffer < BUFFER_EMERGENCY_THRESH)
    {
        for (i = 0; i < 4; i++)
        {
            chassis_power_control->motor_speed_pid[i].out = 0.0f;
        }
        return;
    }

    if (chassis_power_buffer < BUFFER_DERATING_THRESH)
    {
        max_power *= chassis_power_buffer / BUFFER_DERATING_THRESH;
    }

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

    if (total_power > max_power && total_power > 0.0f)
    {
        fp32 power_scale = max_power / total_power;

        for (i = 0; i < 4; i++)
        {
            fp32 target_power;
            fp32 speed_rpm;

            if (motor_power[i] <= 0.0f)
            {
                continue;
            }

            target_power = motor_power[i] * power_scale;
            speed_rpm = (fp32)chassis_power_control->motor_chassis[i].chassis_motor_measure->speed_rpm;
            chassis_power_control->motor_speed_pid[i].out = quadratic_solve_current(
                target_power,
                speed_rpm,
                chassis_power_control->motor_speed_pid[i].out);
        }
    }
}
