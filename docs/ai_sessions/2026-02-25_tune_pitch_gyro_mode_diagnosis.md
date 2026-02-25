# 调参记录: pitch_gyro_mode_diagnosis

日期: 2026-02-25

---

## 11:41:20 | AHRS.lib pitch解算失效，pitch轴必须改用ENCONDE模式

### 参数变更

**调前:**
```
pitch模式: GIMBAL_MOTOR_GYRO (GIMBAL_ABSOLUTE_ANGLE行为下)
PITCH_GYRO_ABSOLUTE_PID_KP = 5.0
PITCH_GYRO_ABSOLUTE_PID_KI = 0.3
PITCH_GYRO_ABSOLUTE_PID_KD = 0.1
PITCH_GYRO_ABSOLUTE_PID_MAX_OUT = 5.0
PITCH_GYRO_ABSOLUTE_PID_MAX_IOUT = 1.0
PITCH_SPEED_PID_KP = 800.0
PITCH_SPEED_PID_KI = 10.0
PITCH_RC_SEN = -0.000001f
```

**调后:**
```
pitch模式: GIMBAL_MOTOR_ENCONDE (GIMBAL_ABSOLUTE_ANGLE行为下)
PITCH_ENCODE_RELATIVE_PID_KP = 8.0
PITCH_ENCODE_RELATIVE_PID_KI = 0.2
PITCH_ENCODE_RELATIVE_PID_KD = 0.1
PITCH_ENCODE_RELATIVE_PID_MAX_OUT = 5.0
PITCH_ENCODE_RELATIVE_PID_MAX_IOUT = 1.0
PITCH_SPEED_PID (不变): Kp=800, Ki=10
PITCH_ENCODE_SEN = 0.01f
```

### 效果对比

1. MCP实时采集验证: 手持开发板旋转90度，pitch_abs仅变化2.6度(0.046rad)
2. 历史VOFA+数据确认: pitch_abs从未正常工作，所有数据中col33始终在-0.005~-0.007附近
3. 陀螺仪有响应(pitch_gyro峰值0.5rad/s)但AHRS解算角度严重偏小
4. 编码器pitch_rel正常: 全行程0.434rad(24.9度)
5. 推pitch全行程时IMU所有通道(pitch_abs/yaw_abs/pitch_gyro/yaw_gyro)均无明显响应
6. 摇晃整台机器人时IMU有微弱响应，确认IMU硬件正常但AHRS.lib解算异常
7. 之前GYRO模式下电机堵转在物理限位的根因: absolute_angle反馈信号无效

### 证据文件

- [data/tune_2026-02-24_001.csv](../../data/tune_2026-02-24_001.csv)

### 结论

AHRS.lib(DJI预编译黑盒)的pitch角度解算严重失真，无法用于GYRO模式控制。
Yaw轴GYRO模式不受影响(yaw_abs数据正常)。
决策: pitch轴改回ENCONDE模式，用编码器相对角度控制。
ENCONDE模式PID需要针对连杆机构重新调参:
- Kp从15降到8(连杆减速比导致增益需降低)
- 加入Ki=0.2消除稳态误差(之前Ki=0导致5.3度静差)
- 加入Kd=0.1抑制超调
后续可考虑用开源MahonyAHRS替换AHRS.lib修复IMU解算。

