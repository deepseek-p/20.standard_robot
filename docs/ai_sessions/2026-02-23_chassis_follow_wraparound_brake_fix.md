# 2026-02-23 chassis follow wraparound brake fix

## 1. 目标

- 本次要解决的问题：MID 跟随模式下底盘出现 `wz_set` 周期性正负翻转、`relative_angle` 在 `±pi` 邻域往返、四轮电流大幅拍击的极限环振荡。
- 预期可观测结果：`wz_set` 不再在 `±4000` 间周期翻转，`relative_angle` 向 0 收敛，不再在 `±pi` 边界回绕打转。

## 2. 改动文件列表

- `application/chassis_task.c`
- `application/chassis_task.h`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-23_chassis_follow_wraparound_brake_fix.md`

## 3. 关键改动摘要

- `application/chassis_task.h`：重设底盘跟随角 PID 参数。
  - `CHASSIS_FOLLOW_GIMBAL_PID_KP: 6.0f -> 8.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_KI: 0.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_KD: 0.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT: 4.0f -> 2.0f`
  - `CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT: 0.2f`
- `application/chassis_task.c`：
  - 新增 `#include <math.h>` 以使用 `fabsf`。
  - 将刹车阈值由局部常量改为文件级宏：
    - `CHASSIS_FOLLOW_ANGLE_BRAKE_START (PI * 0.5f)`
    - `CHASSIS_FOLLOW_ANGLE_BRAKE_END (PI * 0.85f)`
  - 在 FOLLOW 分支原 `wz_set = -PID_calc(...)` 后加入大偏差刹车逻辑：
    - `|rel| > PI*0.5` 开始线性降权；
    - `|rel| >= PI*0.85` 时输出抑制到 0；
    - 通过 `brake_factor` 对 `wz_set` 乘权，抑制冲过 `±pi` 回绕边界。
- `docs/risk_log.md`：同步记录“参数软化 + 大偏差刹车因子”已落地。
- `docs/INDEX.md`：新增本次会话索引。

## 4. 关键决策与理由

- 决策 1：不改 PID 库、不改云台侧、不改模式映射，仅在底盘 FOLLOW 输出端增加非线性保护。
- 决策 2：把问题定义为“`±pi` 回绕极限环”，不是继续线性 PID 参数微调问题。
- 决策 3：先用输出端刹车打断极限环，再依据数据决定是否进入第二步串级速度环。

### 4.1 改码前三问（对应 `docs/00_AI_HANDOFF_RULES.md` 6.1）

1. 当前故障的实际执行路径是什么？  
   在 `CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW` 路径中，`wz_set = -PID_calc(angle_pid, rel, rel_set)`，`rel` 为 `[-pi, pi]` 环绕角。日志显示 `rel` 在 `±pi` 附近回绕时 `wz_set` 一帧翻转，形成极限环。
2. 本次修改命中的是什么路径？  
   直接命中同一路径：`application/chassis_task.c` FOLLOW 分支的 `wz_set` 计算后处理；并同步调整 `application/chassis_task.h` 同一环路参数上限。
3. 如何用数据证明本次修改生效？  
   采集 `11/14/15/16/19~22/23/42/44`：若生效，`wz_set` 不再出现 `±4000` 周期翻转，`rel` 不再在 `±3090~±3140mrad` 来回跳边界而是趋向 0。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增 `osDelay/vTaskDelay` 或阻塞调用。
- CPU 影响：每个底盘周期新增常数级计算（`fabsf` + 线性缩放），负担极低。
- 栈影响：无任务栈变更。
- 帧率/周期：控制周期不变（`CHASSIS_CONTROL_TIME_MS` 仍为 2ms）。

## 6. 风险点

- 风险 1：大偏差区域输出被压制后，某些工况下“追随速度偏慢”。
- 风险 2：若云台侧持续施加较大反作用力矩，可能残留低频慢摆，需要后续串级速度环进一步抑制。

## ⚠ 未验证假设

- 假设：当前振荡主因是 `±pi` 回绕极限环，输出刹车因子可显著打断该极限环。
- 未验证原因：本次仅完成代码改动，尚未拿到改后实机 VOFA+ 帧。
- 潜在影响：若根因包含显著机械耦合/摩擦不均，单靠刹车因子改善有限。
- 后续验证计划：用户回传 MID 静止 + yaw 阶跃数据；若仍存在明显摆振，进入第二步串级速度环改造方案。

## 7. 验证方式与结果

- 串口：未执行（待实机回传）。
- USB：未执行（待 VOFA+ 回传）。
- 示波器：未执行。
- 上位机：未执行。
- 长跑：未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - `application/chassis_task.c` 删除大偏差刹车因子逻辑。
  - `application/chassis_task.h` 参数恢复到上一版本。
- 参数回滚点：`Kp/Kd/max_out` 宏。
- 运行时保护：建议先架空验证，确认无翻转后再落地低速验证。

## 9. 附件/证据

- 数据来源：`data.md`（本轮根因定位所依据的 VOFA+ 帧）。
- 风险关联：`docs/risk_log.md` 中 2026-02-23 底盘跟随风险条目。
