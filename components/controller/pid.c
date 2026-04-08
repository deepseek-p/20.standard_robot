/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       pid.c/h
  * @brief      pid实现函数，包括初始化，PID计算函数，
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. 完成
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */

#include "pid.h"
#include "main.h"

#define LimitMax(input, max)   \
    {                          \
        if (input > max)       \
        {                      \
            input = max;       \
        }                      \
        else if (input < -max) \
        {                      \
            input = -max;      \
        }                      \
    }

/**
  * @brief          pid struct data init
  * @param[out]     pid: PID struct data point
  * @param[in]      mode: PID_POSITION: normal pid
  *                 PID_DELTA: delta pid
  * @param[in]      PID: 0: kp, 1: ki, 2:kd
  * @param[in]      max_out: pid max out
  * @param[in]      max_iout: pid max iout
  * @retval         none
  */
/**
  * @brief          pid struct data init
  * @param[out]     pid: PID结构数据指针
  * @param[in]      mode: PID_POSITION:普通PID
  *                 PID_DELTA: 差分PID
  * @param[in]      PID: 0: kp, 1: ki, 2:kd
  * @param[in]      max_out: pid最大输出
  * @param[in]      max_iout: pid最大积分输出
  * @retval         none
  */
void PID_init(pid_type_def *pid, uint8_t mode, const fp32 PID[3], fp32 max_out, fp32 max_iout)
{
    if (pid == NULL || PID == NULL)
    {
        return;
    }
    pid->mode = mode;
    pid->Kp = PID[0];
    pid->Ki = PID[1];
    pid->Kd = PID[2];
    pid->max_out = max_out;
    pid->max_iout = max_iout;
    pid->Dbuf[0] = pid->Dbuf[1] = pid->Dbuf[2] = 0.0f;
    pid->error[0] = pid->error[1] = pid->error[2] = pid->Pout = pid->Iout = pid->Dout = pid->out = 0.0f;
}

/**
  * @brief          pid calculate 
  * @param[out]     pid: PID struct data point
  * @param[in]      ref: feedback data 
  * @param[in]      set: set point
  * @retval         pid out
  */
/**
  * @brief          pid计算
  * @param[out]     pid: PID结构数据指针
  * @param[in]      ref: 反馈数据
  * @param[in]      set: 设定值
  * @retval         pid输出
  */
fp32 PID_calc(pid_type_def *pid, fp32 ref, fp32 set)
{
    if (pid == NULL)
    {
        return 0.0f;
    }

    pid->error[2] = pid->error[1];
    pid->error[1] = pid->error[0];
    pid->set = set;
    pid->fdb = ref;
    pid->error[0] = set - ref;
    if (pid->mode == PID_POSITION)
    {
        pid->Pout = pid->Kp * pid->error[0];
        pid->Iout += pid->Ki * pid->error[0];
        pid->Dbuf[2] = pid->Dbuf[1];
        pid->Dbuf[1] = pid->Dbuf[0];
        pid->Dbuf[0] = (pid->error[0] - pid->error[1]);
        pid->Dout = pid->Kd * pid->Dbuf[0];
        LimitMax(pid->Iout, pid->max_iout);
        pid->out = pid->Pout + pid->Iout + pid->Dout;
        LimitMax(pid->out, pid->max_out);
    }
    else if (pid->mode == PID_DELTA)
    {
        pid->Pout = pid->Kp * (pid->error[0] - pid->error[1]);
        pid->Iout = pid->Ki * pid->error[0];
        pid->Dbuf[2] = pid->Dbuf[1];
        pid->Dbuf[1] = pid->Dbuf[0];
        pid->Dbuf[0] = (pid->error[0] - 2.0f * pid->error[1] + pid->error[2]);
        pid->Dout = pid->Kd * pid->Dbuf[0];
        pid->out += pid->Pout + pid->Iout + pid->Dout;
        LimitMax(pid->out, pid->max_out);
    }
    return pid->out;
}

/**
  * @brief          pid out clear
  * @param[out]     pid: PID struct data point
  * @retval         none
  */
/**
  * @brief          pid 输出清除
  * @param[out]     pid: PID结构数据指针
  * @retval         none
  */
void PID_clear(pid_type_def *pid)
{
    if (pid == NULL)
    {
        return;
    }

    pid->error[0] = pid->error[1] = pid->error[2] = 0.0f;
    pid->Dbuf[0] = pid->Dbuf[1] = pid->Dbuf[2] = 0.0f;
    pid->out = pid->Pout = pid->Iout = pid->Dout = 0.0f;
    pid->fdb = pid->set = 0.0f;
}

void PID_enhanced_init(pid_enhanced_t *pid, fp32 kp, fp32 ki, fp32 kd, fp32 max_out, fp32 max_iout,
                       fp32 dead_zone, fp32 I_L, fp32 I_U, fp32 RC_DF)
{
    if (pid == NULL)
    {
        return;
    }

    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    pid->max_out = max_out;
    pid->max_iout = max_iout;
    pid->dead_zone = dead_zone;
    pid->I_L = I_L;
    pid->I_U = I_U;
    pid->RC_DF = RC_DF;

    if (pid->RC_DF < 0.0f)
    {
        pid->RC_DF = 0.0f;
    }
    else if (pid->RC_DF > 1.0f)
    {
        pid->RC_DF = 1.0f;
    }

    if (pid->I_U < pid->I_L)
    {
        pid->I_U = pid->I_L;
    }

    PID_enhanced_clear(pid);
}

fp32 PID_enhanced_calc(pid_enhanced_t *pid, fp32 ref, fp32 set)
{
    fp32 err;
    fp32 err_last;
    fp32 abs_err;
    fp32 i_scale = 0.0f;
    fp32 raw_d;

    if (pid == NULL)
    {
        return 0.0f;
    }

    pid->error[1] = pid->error[0];
    pid->error[0] = set - ref;
    pid->set = set;
    pid->fdb = ref;

    err = pid->error[0];
    err_last = pid->error[1];

    if (((err >= 0.0f) ? err : -err) < pid->dead_zone)
    {
        err = 0.0f;
    }

    if (((err_last >= 0.0f) ? err_last : -err_last) < pid->dead_zone)
    {
        err_last = 0.0f;
    }

    pid->Pout = pid->Kp * err;

    abs_err = (err >= 0.0f) ? err : -err;
    if (abs_err < pid->I_L)
    {
        i_scale = 1.0f;
    }
    else if ((abs_err < pid->I_U) && (pid->I_U > pid->I_L))
    {
        i_scale = 1.0f - (abs_err - pid->I_L) / (pid->I_U - pid->I_L);
    }

    if (i_scale > 0.0f)
    {
        pid->Iout += pid->Ki * (err + err_last) * 0.5f * i_scale;
    }

    LimitMax(pid->Iout, pid->max_iout);

    raw_d = pid->Kd * (pid->out - pid->out_last);
    pid->Dout = raw_d * pid->RC_DF + pid->Dout_last * (1.0f - pid->RC_DF);
    pid->Dout_last = pid->Dout;

    pid->out_last = pid->out;
    pid->out = pid->Pout + pid->Iout + pid->Dout;
    LimitMax(pid->out, pid->max_out);

    return pid->out;
}

void PID_enhanced_clear(pid_enhanced_t *pid)
{
    if (pid == NULL)
    {
        return;
    }

    pid->set = 0.0f;
    pid->fdb = 0.0f;
    pid->out = 0.0f;
    pid->out_last = 0.0f;
    pid->Pout = 0.0f;
    pid->Iout = 0.0f;
    pid->Dout = 0.0f;
    pid->Dout_last = 0.0f;
    pid->error[0] = 0.0f;
    pid->error[1] = 0.0f;
}
