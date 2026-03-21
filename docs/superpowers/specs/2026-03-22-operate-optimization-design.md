# 操作优化设计：射速挡位 / 底盘跟随 / 小陀螺方向锁定

**日期**: 2026-03-22
**分支**: codex/optimize-robot-handling
**状态**: 设计完成，待实现

## 概述

三项操作体验优化：
1. 摩擦轮射速改为两个离散挡位（20m/s / 24m/s）
2. 底盘跟随模式加速响应（上车在线调参）
3. 小陀螺模式底盘旋转方向锁定

附带：遥测新增 `bullet_speed` 列，辅助射速标定。

## 需求 1：射速两挡位切换

### 现状

- 三个速度常量 `FRIC_SPEED_LOW/MID/HIGH`（4900/5800/7400 RPM）
- 键盘 F 降 / Shift 升，步长 100 RPM 连续调
- 键盘处理在 `shoot.c` 约 336-372 行

### 设计

**shoot.h**:
- 删除 `FRIC_SPEED_LOW`, `FRIC_SPEED_MID`, `FRIC_SPEED_HIGH`, `FRIC_SPEED_ADJUST_STEP`
- 新增：
  ```c
  #define FRIC_GEAR_COUNT      2
  #define FRIC_SPEED_GEAR_0    4900.0f   // ~20m/s，待标定
  #define FRIC_SPEED_GEAR_1    5800.0f   // ~24m/s，待标定
  ```
- 键盘映射：
  - `SHOOT_FRIC_INC_KEYBOARD` = Shift + F（升挡）
  - `SHOOT_FRIC_DEC_KEYBOARD` = F（降挡）

**shoot.c**:
- 新增静态变量：
  ```c
  static uint8_t fric_gear = 0;
  static const fp32 fric_speed_table[FRIC_GEAR_COUNT] = {
      FRIC_SPEED_GEAR_0, FRIC_SPEED_GEAR_1
  };
  ```
- 键盘处理改为边沿检测：
  - Shift+F 按下边沿 → `fric_gear = min(fric_gear + 1, FRIC_GEAR_COUNT - 1)`
  - F 按下边沿 → `fric_gear = max(fric_gear - 1, 0)`（需排除 Shift 同时按下的情况）
- `fric_speed_base = fric_speed_table[fric_gear]`
- 删除旧的连续步进调速逻辑

### 边沿检测要点

F 降挡时需确认 Shift 未按下，避免 Shift+F 升挡的同时也触发 F 降挡。

## 需求 2：底盘跟随加速

### 现状

- Follow PID: Kp=2.0, Ki=0.0, Kd=100.0, max_out=3.0
- 定义在 `chassis_task.h` 127-133 行

### 设计

不改代码结构。上车后用 MCP `set_pid target=chassis_follow` 在线调参，调好后固化到 `chassis_task.h` 宏定义。

预期调参方向：提高 Kp（可能 3.0~5.0），配合调整 Kd 抑制超调。

## 需求 3：小陀螺方向锁定

### 现状

- `chassis_behaviour.c:454-461`：ch[2] 摇杆决定顺/逆时针
- 用户报告：云台转动时底盘会出现方向来回切换的卡顿

### 设计

删除 ch[2] 方向判断，`wz_set` 始终为正值：

```c
// 改前（6行）
if (chassis_move_rc_to_vector->chassis_RC->rc.ch[CHASSIS_WZ_CHANNEL] < -CHASSIS_RC_DEADLINE)
{
    *wz_set = -CHASSIS_HUST_SELF_PROTECT_WZ;
}
else
{
    *wz_set = CHASSIS_HUST_SELF_PROTECT_WZ;
}

// 改后（1行）
*wz_set = CHASSIS_HUST_SELF_PROTECT_WZ;
```

## 需求 4：遥测新增 bullet_speed

### 现状

- `referee.h:142` 定义了 `bullet_speed` 字段（float, m/s）
- 当前无消费端，遥测未上报
- 遥测当前 75 列（索引 0-74）

### 设计

- 在 `usb_task.c` 遥测打包处，追加第 75 列（索引 75）
- 列名：`bullet_spd`
- 值：`bullet_speed × 100` 整数上报（centi-scaled，与功率控制列一致）
- 需从裁判系统结构体获取指针（类似 `get_shoot_data()` 或直接引用全局结构体）

## 文件改动清单

| 文件 | 改动类型 | 说明 |
|------|----------|------|
| `application/shoot.h` | 修改 | 删除旧速度宏，新增挡位宏 |
| `application/shoot.c` | 修改 | 挡位切换逻辑替换连续调速 |
| `application/chassis_behaviour.c` | 修改 | 删除 ch[2] 判断，wz_set 固定 |
| `application/usb_task.c` | 修改 | 新增 bullet_spd 遥测列 |

## 不改动的部分

- `chassis_task.h`：follow PID 参数保持现值，上车在线调
- 坐标旋转逻辑（chassis_behaviour.c:444-449）：本次不动
- 射速 RPM 具体值：占位值，上车标定后更新

## 风险

1. **RPM 占位值不准**：首次上车需用裁判 bullet_speed 遥测标定，预留 #define 方便修改
2. **F 键边沿检测**：需正确区分"F单独按下"和"Shift+F同时按下"，否则升降挡会冲突
3. **bullet_speed 更新频率**：裁判系统仅在弹丸击发后更新，遥测中大部分时间是上一发的值
