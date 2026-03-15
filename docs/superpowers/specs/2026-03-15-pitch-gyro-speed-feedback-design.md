# Pitch ENCODE 模式速度反馈切换为陀螺仪

**日期**: 2026-03-15
**状态**: Draft
**分支**: `codex/optimize-robot-handling`

---

## 1. 问题

Pitch 轴在底盘加减速时剧烈晃动（pp=0.21~0.43 rad, 12~25°），严重影响瞄准。

### 根因链路

1. 底盘加速 → 惯性力矩作用于 pitch
2. 编码器速度反馈（差分 + LPF α=0.05）感知到"伪速度"（编码器在体坐标系下，底盘加速导致相对位移）
3. 速度环 PID 产生错误的 I_pid
4. 自适应前馈 `b_hat += gamma * I_pid` 跟踪了加速度扰动
5. 加速结束后 b_hat 被拉偏，需要数秒恢复 → pitch 长时间晃动

### 次要问题（已解决但相关）

速度环 Ki=10 与前馈 gamma 耦合产生 ~1Hz 自激振荡。当前用 Ki=0 压制，但 Ki=0 削弱了速度环的稳态补偿能力。根因同样是编码器 LPF 的 ~20ms 延迟。

## 2. 方案

将 ENCODE 模式的 pitch 速度反馈从编码器差分改为 IMU 陀螺仪（`INS_gyro[Y]`）。

### 为什么陀螺仪解决问题

- 陀螺仪测量惯性系角速度，底盘线加速度不影响角速度测量
- BMI088 内部已有数字滤波器，不需要 α=0.05 的重 LPF → 无 ~20ms 延迟
- 延迟消除后 Ki 可以恢复正常工作

### 前提条件

- C 板（BMI088 IMU）**必须装在云台上**（跟着 pitch 和 yaw 一起转动）
- 用户已确认：当前 C 板装在云台上，满足条件
- 更换 IMU 安装位置时只要**朝向不变**，代码不需要修改（陀螺仪测角速度，与位置无关）
- **旋转矩阵依赖**：`INS_gyro[Y]` 经过 `BMI088_BOARD_INSTALL_SPIN_MATRIX` 旋转后输出（INS_task.c:43-46）。当前矩阵使 `INS_gyro[Y] = -raw_gyro[X]`。如果更换 IMU 安装朝向，必须同步更新旋转矩阵，否则 pitch 速度反馈方向错误

### 不变的部分

- 位置环：编码器相对角度（ENCODE 模式核心优势，无需绝对角标定）
- 位置环 PID：Kp=24, Ki=0, Kd=0, max_out=10, max_iout=0.5
- 自适应前馈：保留全部逻辑和参数（上供弹系统需要跟踪弹药质量/重心变化）
- Yaw 轴：完全不受影响
- GYRO 模式和 RAW 模式：不受影响

## 3. 改动详情

### 3.1 gimbal_task.c — `gimbal_feedback_update()`

**位置**：ENCODE 模式 pitch 速度反馈赋值处（约 line 922-926）

改前：
```c
#if PITCH_TURN
    feedback_update->gimbal_pitch_motor.motor_gyro = -pitch_ecd_speed_filtered;
#else
    feedback_update->gimbal_pitch_motor.motor_gyro = pitch_ecd_speed_filtered;
#endif
```

改后：
```c
// ENCODE 模式: 位置用编码器, 速度用陀螺仪 (免疫底盘加速度)
feedback_update->gimbal_pitch_motor.motor_gyro =
    *(feedback_update->gimbal_INT_gyro_point + INS_GYRO_Y_ADDRESS_OFFSET);
```

**符号约定**：不需要取反。GYRO 模式（line 930）用同样的表达式且不取反，两种模式共用同一个速度 PID（`gimbal_motor_gyro_pid`），符号约定一致。PITCH_TURN=1 的取反是编码器方向特有的。

**编码器 LPF 代码**（`pitch_ecd_speed_filtered` 等）：保留不删。未来需要回退时改一行即可。

### 3.2 gimbal_task.h — 速度环 Ki 默认值

改前：
```c
#define PITCH_SPEED_PID_KI        0.0f    // 当前已固化的 Ki=0
```

改后：
```c
#define PITCH_SPEED_PID_KI        0.0f    // 保持 Ki=0, 通过 MCP 在线调参确认稳定值后再固化
```

**注意**：Ki 不在代码中直接恢复。烧录后通过 MCP 在线调参从 Ki=5 起步逐步增加，确认稳定后再固化到代码。避免首次上电就带着未验证的 Ki。

### 3.3 不需要修改的文件

- `INS_task.c/h`：`get_gyro_data_point()` 和 `INS_gyro[]` 已存在
- `pid.c`：PID 结构不变
- `usb_task.c`：`pitch_speed` 的 MCP target 映射已指向 `gimbal_motor_gyro_pid`，在线调参正常工作
- 前馈代码（gimbal_task.c:1276-1320）：不改。`motor_gyro` 变量换了数据源，前馈的速度门控和适应逻辑自动适配

## 4. 数据流对比

### 改前（编码器速度）
```
CAN RX → ecd → diff → raw_speed → LPF(α=0.05) → ±negate → motor_gyro
                                    ↑ ~20ms 延迟
                                    ↑ 受底盘加速度影响
```

### 改后（陀螺仪速度）
```
SPI → BMI088 gyro → INS_gyro[Y] → motor_gyro
                     ↑ <1ms 延迟
                     ↑ 免疫底盘线加速度
```

两种情况下 motor_gyro 的下游完全相同：
```
motor_gyro → speed PID → I_pid → I_total = I_pid + b_hat → current_set
                                  ↑
                          adaptive FF: b_hat += gamma * I_pid (当 |speed| < 0.5)
```

## 5. 风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| 陀螺仪符号与编码器不一致 | GYRO 模式已验证 INS_gyro[Y] 与速度 PID 兼容，无需取反 |
| 陀螺仪高频振动噪声 | 实测后按需加轻 LPF（α=0.3~0.5，延迟 2~3ms），比编码器 LPF 轻 10 倍 |
| Ki 恢复后新的振荡模式 | 通过 MCP 在线调参，从 Ki=5 起步逐步增加，确认后再固化 |
| 前馈行为变化 | 稳态完全相同（speed=0 → I_pid=0 → 不适应）；动态更好（不被加速度骗） |
| IMU 松动/朝向变化 | 运维问题，换位置但保持朝向不变则无影响；换朝向须更新旋转矩阵 |
| 供弹振动耦合到陀螺仪 | 拨弹/供弹机械振动可能引入高频扰动（编码器有重 LPF 天然滤除）。验证计划 7.5 覆盖 |
| 陀螺仪零偏漂移 | BMI088 典型零偏 ~0.017 rad/s，远小于前馈速度门控 0.5 rad/s，影响可忽略 |

## 6. 回退方案

将改后的陀螺仪行恢复为编码器行（已保留代码），同时将 Ki 改回 0。一行代码即可回退。

## 7. 验证计划

### 7.1 静态验证

1. 烧录后 MID 模式，不操控
2. MCP 采集 `pitch_cur, pitch_rel, ff_b_hat`，10 秒
3. 预期：pitch_cur pp < 200，pitch_rel pp ≈ 0（与 Ki=0 基线相当或更好）

### 7.2 加减速验证（核心）

1. MID 模式，推杆前后加减速
2. MCP 采集 `pitch_cur, pitch_rel, ff_b_hat, vx_set`，15 秒
3. **关键指标**：pitch_rel pp（之前 0.21~0.43 rad），目标 < 0.05 rad
4. ff_b_hat pp 应大幅减小（之前 1400~2100，目标 < 200）

### 7.3 Ki 调参

1. MCP 在线调 `pitch_speed Ki`，从 5 起步
2. 观察 pitch_cur 是否出现振荡
3. 确认 Ki 能消除稳态位置误差

### 7.4 前馈跟踪验证

1. 手动改变 pitch 角度（遥控拨杆）
2. 观察 ff_b_hat 是否跟踪角度变化
3. 确认前馈在不同角度下正常工作

### 7.5 射击振动验证

1. MID 模式，连续射击（摩擦轮 + 拨弹）
2. MCP 采集 `pitch_cur, pitch_rel, ff_b_hat`，10 秒
3. 对比不射击时的基线，观察供弹机械振动是否引入新的高频扰动
4. 如果 pitch_cur 高频分量明显增大，考虑加轻 LPF（α=0.3~0.5）

## 8. 改动量汇总

| 文件 | 改动 |
|------|------|
| `gimbal_task.c` | 1 处：ENCODE 模式 pitch 速度反馈赋值（~3 行） |
| `gimbal_task.h` | 无改动（Ki=0 保持，通过 MCP 在线调参后再固化） |
| **总计** | **1 处，~3 行代码** |
