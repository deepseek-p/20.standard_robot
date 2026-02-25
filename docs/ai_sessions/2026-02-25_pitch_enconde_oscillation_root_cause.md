# Pitch ENCONDE模式振荡根因分析与编码器速度反馈方案

**日期**: 2026-02-25
**状态**: 方案已确定，待实施

## 背景

Codex#4将pitch从GYRO模式切回ENCONDE模式（因AHRS.lib pitch角度输出损坏）。烧录后pitch严重振荡。

## 根因

ENCONDE模式的速度环（内环）反馈使用 `motor_gyro = INS_gyro[Y]`（IMU陀螺仪），但IMU无法感知pitch轴运动。

速度环"失明"→无法提供阻尼→Kp≥4即振荡。

**代码位置**: `gimbal_task.c:867`
```c
feedback_update->gimbal_pitch_motor.motor_gyro = *(feedback_update->gimbal_INT_gyro_point + INS_GYRO_Y_ADDRESS_OFFSET);
```

## 调参记录

| 参数组合 | 结果 |
|---------|------|
| Kp=8 Ki=0.2 Kd=0.1 speed_Kp=800 | 严重振荡，pitch_rel摆幅±0.3rad |
| Kp=2 Ki=0 speed_Kp=400 | 稳定，稳态误差0.375rad，电流3100推不动 |
| Kp=4 Ki=0 speed_Kp=400 | 缓慢漂移，大稳态误差 |
| Kp=3 Ki=0 speed_Kp=400 | 稳定，稳态误差0.37rad |
| Kp=6 Ki=0.1 max_out=10 | 振荡 |
| Kp=4 Ki=0.1 max_out=5 | 振荡 |
| Kp=2 Ki=0.05 speed_Kp=800 | 振荡（恢复speed_Kp=800即振荡） |

## 关键证据

- 遥测: pitch_gyro≈0（IMU陀螺仪读不到pitch运动）
- 角度环输出 gyro_set=-0.313rad/s，但gyro反馈≈0
- 电流不够→卡住（稳态误差）；电流够大→过冲振荡（无阻尼）

## 解决方案

ENCONDE模式下用GM6020 CAN反馈的 `speed_rpm` 替代 IMU gyro 作为pitch速度环反馈。

修改 `gimbal_task.c:867`:
```c
if (feedback_update->gimbal_pitch_motor.gimbal_motor_mode == GIMBAL_MOTOR_ENCONDE)
{
    fp32 rpm = (fp32)feedback_update->gimbal_pitch_motor.gimbal_motor_measure->speed_rpm;
#if PITCH_TURN
    feedback_update->gimbal_pitch_motor.motor_gyro = -rpm * 0.104720f;
#else
    feedback_update->gimbal_pitch_motor.motor_gyro = rpm * 0.104720f;
#endif
}
else
{
    feedback_update->gimbal_pitch_motor.motor_gyro = *(feedback_update->gimbal_INT_gyro_point + INS_GYRO_Y_ADDRESS_OFFSET);
}
```
