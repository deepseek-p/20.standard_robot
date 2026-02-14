# 20.standard_robot 当前状态交接

> 目标：下一个 AI 只看此文档，即可恢复到当前可用状态。

## 当前状态
- 底盘遥控正常。
- 云台已锁定跟随底盘角度，Yaw/Pitch 不响应遥控杆。
- 云台 CAN ID 已互换：Yaw=0x206，Pitch=0x205。

## 已改文件与要点

### 1) `application/CAN_receive.h`

```c
CAN_PIT_MOTOR_ID = 0x205,
CAN_YAW_MOTOR_ID = 0x206,
```

### 2) `application/CAN_receive.c`
- `HAL_CAN_RxFifo0MsgPendingCallback` 中，yaw/pitch/trigger 用独立 `case` 处理，不再用 `StdId-0x201` 推算云台索引。
- `CAN_cmd_gimbal` 中，按 `CAN_YAW_MOTOR_ID/CAN_PIT_MOTOR_ID` 自动映射 0x205/0x206 电流槽位。

### 3) `application/gimbal_behaviour.c`
- 新增开关：
```c
#define GIMBAL_LOCK_TO_CHASSIS 1
```
- `gimbal_behavour_set`：
  - 在 `GIMBAL_LOCK_TO_CHASSIS=1` 时，DBUS 在线则强制 `GIMBAL_RELATIVE_ANGLE`，DBUS 掉线则 `GIMBAL_ZERO_FORCE`。
  - 禁用 `ZERO_FORCE -> GIMBAL_INIT` 自动过渡（避免上电后再跑 init 摆动）。
- `gimbal_relative_angle_control`：
  - 在 `GIMBAL_LOCK_TO_CHASSIS=1` 时输出
```c
*yaw = 0.0f;
*pitch = 0.0f;
```
  - 即云台相对角度保持不变。

### 4) `application/chassis_task.c/h`
- 全向解算参数：`MOTOR_DISTANCE_TO_CENTER = 0.25f`（L=250mm）。
- 运动解算在 `chassis_vector_to_mecanum_wheel_speed`。
- 反解在 `chassis_feedback_update`。

## 恢复到当前状态的最小检查清单
1. `CAN_receive.h` 确认 ID：Yaw=0x206，Pitch=0x205。
2. `CAN_receive.c` 确认：
   - Rx 回调对 yaw/pitch/trigger 是独立 `case`。
   - `CAN_cmd_gimbal` 含 ID 映射分支。
3. `gimbal_behaviour.c` 确认：
   - `#define GIMBAL_LOCK_TO_CHASSIS 1`
   - 强制 `GIMBAL_RELATIVE_ANGLE`、屏蔽遥控增量、关闭 init 过渡。

## 下一步调参
建议直接调 `application/chassis_task.h` 底盘速度环：
- `M3505_MOTOR_SPEED_PID_KP`
- `M3505_MOTOR_SPEED_PID_KI`
- `M3505_MOTOR_SPEED_PID_KD`
