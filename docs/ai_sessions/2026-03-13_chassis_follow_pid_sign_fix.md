# 底盘跟随 PID 方向修复及参数调优

**日期**: 2026-03-13
**分支**: `codex/optimize-robot-handling`
**Commit**: `786435a`
**AI**: Claude Opus 4.6

---

## 1. 目标

- 修复 MID 模式（FOLLOW_GIMBAL_YAW）底盘跟随行为异常：底盘不收敛到云台朝向，大角度偏差时摆来摆去
- 修复底盘平移方向（前后/左右）反转
- 预期可观测结果：MID 模式推杆前进时车向前走，底盘自动对齐云台方向

## 2. 改动文件列表

- `application/chassis_task.c` — PID 符号修正、刹车逻辑删除、vx 方向恢复
- `application/chassis_task.h` — 跟随 PID 默认值更新
- `application/gimbal_task.h` — 新增 `YAW_OFFSET_CORRECTION`

## 3. 关键改动摘要

### chassis_task.c

| 位置 | 改动 | 原因 |
|------|------|------|
| Line 528 | `wz_set = PID_calc(...)` 去掉前面的 `-` 号 | **根因修复**：YAW_TURN=0 时 `relative_angle` 不取反，`-PID_calc()` 导致 P 项驱离对齐位置 |
| Line 485 | `*vx_set = ...out` 恢复原始（无取反） | 旧 `-` 是补偿 PID 方向错误导致的 rel≈π 旋转矩阵翻转，修正 PID 后不再需要 |
| Line 447 | `vy_set_channel = vy_channel * -CHASSIS_VY_RC_SEN` 保持原样 | 这是 DJI 硬件惯例的 Y 轴取反，与 PID bug 无关 |
| Line 46-48, 530-559 | 删除 `BRAKE_START/END/MIN` 宏和全部刹车逻辑 | PID 方向修正后刹车逻辑不再需要；旧逻辑在 rel≈±π 时 brake_factor=0 造成死锁 |

### chassis_task.h

```c
// Before:
#define CHASSIS_FOLLOW_GIMBAL_PID_KP 8.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_KI 0.5f
#define CHASSIS_FOLLOW_GIMBAL_PID_KD 0.0f

// After:
#define CHASSIS_FOLLOW_GIMBAL_PID_KP 2.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_KI 0.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_KD 100.0f
```

原 Kp=8.0 是在 PID 方向错误时调的，P 和 I 互相抵消才"稳定"。修正方向后 Kp=8.0 太激进，遥测调参确定 Kp=2.0 稳定。

Kd=100 是因为离散 PID 的 `Dbuf[0] = error[0] - error[1]`，2ms 周期下每周期误差变化量极小（~0.0001），Kd 需要 100+ 才有阻尼效果。

### gimbal_task.h

```c
#define YAW_OFFSET_CORRECTION  351
```

yaw 编码器的 flash 偏移量存在固定偏差，手动对齐测量后加上此校正值，使 DOWN 模式 `rel ≈ 0.006`（基本对准）。

## 4. 关键决策与理由

### 决策 1：去掉 `wz_set` 前的 `-` 号（而非改 YAW_TURN 或取反 relative_angle）

**理由**：

PID_calc 内部 `error = set - ref = 0 - rel = -rel`：
- 当 rel > 0（底盘偏左），error < 0 → Pout < 0 → wz_set < 0 → 顺时针 → rel 减小 ✓
- 加 `-` 号后方向反转，P 项驱离对齐位置

改 `YAW_TURN` 或 `relative_angle` 取反虽然也能修复，但会连锁影响云台侧使用 relative_angle 的逻辑。直接去掉 `-` 号是最小改动。

### 决策 2：完全删除刹车逻辑（而非修补）

**理由**：

- 刹车逻辑是为了解决 rel≈±π 时 PID 来回摆的问题，但 PID 方向正确后不会出现这种情况
- 旧逻辑有 `brake_factor=0` 死锁 bug（rel 在刹车区间时所有速度被乘以 0）
- 先加了 BRAKE_MIN=0.15 floor，再加了 direction lock，越改越复杂越不稳定
- 彻底删除是最干净的方案

**放弃方案**：

- 修补刹车逻辑（加 floor、加方向锁）：每次修补都引入新问题，方向锁在 PID 方向错误的前提下无法正确工作
- 参考 HUST 代码的 wrap-around 处理：HUST 没有刹车逻辑，因为他们的 PID 方向本来就是对的

### 决策 3：Kp=2.0, Ki=0, Kd=100（而非继续用旧参数）

**理由**：通过 MCP WiFi 遥测远程调参验证。Kp=8.0 方向修正后立即暴力振荡。逐步降到 Kp=2.0 稳定。Ki=0.05 配合 Kd=100 仍有微振荡，暂时 Ki=0 保证稳定。

## 5. 实时性影响（必须写）

- **阻塞变化**：无新增/删除阻塞
- **CPU 影响**：删除了刹车逻辑中的三角函数计算和条件分支，略微减少 CPU 负载
- **栈影响**：无变化
- **帧率/周期**：chassis_task 保持 2ms 周期不变

## 6. 风险点

- **Ki=0 导致稳态误差**：当前 Kp=2.0 无积分，小角度摩擦力矩无法克服，约 7.6° 稳态偏差。需后续遥测调参加回 Ki。
- **Kd=100 在大角度阶跃时可能过度阻尼**：大角度快速转动时 D 项会产生较大反向力矩，可能使响应变慢。实际使用中跟随场景不存在大角度阶跃（云台连续转动），所以可以接受。
- **YAW_OFFSET_CORRECTION 硬编码**：该值依赖具体电机安装位置，更换电机或松动后需重新测量。

## ⚠ 未验证假设

- **假设**：Kd=100 在所有工况下稳定（目前仅测试了静止和低速平移）
- **未验证原因**：机器人关机，未进行高速平移/旋转工况测试
- **潜在影响**：高速工况下 D 项可能引入抖动
- **后续验证计划**：下次上电后在 MID 模式下进行全速平移 + 快速转向测试，观察 wz_set 遥测

---

- **假设**：Ki 可以在 0.01~0.03 范围找到稳定值（配合更大 Kd 如 200~500）
- **未验证原因**：Ki=0.05 + Kd=100 已确认振荡，更小 Ki 或更大 Kd 未测试
- **潜在影响**：如果找不到，可能需要换增强型 PID（变积分、死区等）
- **后续验证计划**：遥测逐步调参

## 7. 验证方式与结果

- **MCP WiFi 遥测**（`wifi://192.168.4.1`）：
  - Channel 14 (rel): 修正后 rel 在小范围波动（Ki=0 时约 0.13 rad ≈ 7.6°），不再出现 ±π 来回跳
  - Channel 13 (wz_set): P 项方向正确，推动 rel 向 0 收敛
  - 远程调参确定 Kp=2.0 稳定，Ki=0.05 振荡
- **实机操控**：
  - MID 模式前后左右方向全部正确
  - 底盘跟随云台方向（有小角度稳态误差）
  - 无振荡（Ki=0 时）
- **DOWN 模式校准**：rel ≈ 0.006 rad（几乎为零），YAW_OFFSET_CORRECTION=351 有效

## 8. 回滚方式（如何恢复）

- **代码回滚点**：`git revert 786435a` 或 `git checkout c825fa0 -- application/chassis_task.c application/chassis_task.h application/gimbal_task.h`
- **参数回滚点**：
  - 跟随 PID: Kp=8.0, Ki=0.5, Kd=0（chassis_task.h 旧值）
  - 注意：回滚后需同时恢复 `wz_set = -PID_calc(...)` 的负号和刹车逻辑，否则 Kp=8.0 会暴力振荡
- **运行时保护**：DOWN 模式（s0=DOWN）为零力矩，可随时切入停机

## 9. 附件/证据

- **Commit**: `786435a fix(chassis): 修复底盘跟随PID方向反转及参数调优`
- **遥测记录**：本次为实时交互调参，未保存 CSV（机器人关机前未执行 capture）
- **设计文档**：`docs/superpowers/specs/2026-03-13-robot-handling-optimization-design.md`（初版，需更新以反映根因）

## 10. 调试过程记录

本次调试经历了较曲折的路径，记录如下以避免重蹈覆辙：

### 第一轮：刹车逻辑修补（失败）

1. 发现 rel≈±π 时 `brake_factor=0` 导致所有轮速为零 → 加 `BRAKE_MIN=0.15` floor
2. Floor 解决了死锁但 PID 仍然方向错误 → rel 在 ±π 间限位循环
3. 加 direction lock（`anti_wrap_dir`）试图锁定旋转方向 → 方向来源不可靠，阈值在 rel 值附近导致抖动
4. 扩大 lock 范围用 rel 符号判定方向 → PID 在 lock 区外方向仍然错误，lock 释放后又被推回

**教训**：在根因未解决的情况下叠加补偿逻辑只会越来越复杂。

### 第二轮：发现根因（成功）

5. 参考 HUST_Infantry_2023 代码，发现其没有刹车逻辑也没有 `-PID_calc()`
6. 分析 PID 符号链路：`error = 0 - rel = -rel` → `Pout = Kp * (-rel)` → 如果加 `-` 就变成 `wz = Kp * rel`，方向错误
7. 去掉 `-` 号 + 删除全部刹车逻辑

### 第三轮：参数调整（部分成功）

8. Kp=8.0 方向修正后暴力振荡 → 遥测远程降到 Kp=2.0
9. Ki=0.05 + Kd=100 仍微振荡 → Ki=0 暂时稳定
10. vx/vy 方向因旋转矩阵不再翻转而需要调整 → 恢复原始 DJI 代码

**教训**：旧参数是在错误 PID 方向下"偶然稳定"的（P 和 I 互相抵消），修正方向后必须重新调参。
