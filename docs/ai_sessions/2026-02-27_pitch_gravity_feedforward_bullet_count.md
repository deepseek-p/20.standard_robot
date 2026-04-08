# AI 会话记录：Pitch 重力前馈补偿 + 拨弹发射计数

日期：2026-02-27

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-27_tune_pitch_encode.md`
- 数据来源：MCP 在线采集（`data/tune_2026-02-27_008.csv`、`009.csv`、`010.csv`、`012.csv`）
- 关键帧/时间段：空仓与 4/5 仓对比中，低头方向出现约 `3°` 残差，且装弹后低头维持电流显著高于空仓。
- 状态：`Partial -> In Progress`
- 差异：本轮从“现象确认”进入“控制链路改造”，新增 pitch 重力前馈与发弹计数衰减。
- 下一步：实机在线标定 `K_FIXED/K_AMMO_FULL` 并验证计数准确性。

## 1. 目标

- 在 pitch `ENCONDE` 模式电流输出叠加重力前馈，降低装弹工况低头方向稳态误差。
- 基于拨弹电机角度到位事件累计 `bullet_fired_count`，动态衰减弹丸重力补偿。
- 固化已验证的 `PITCH_ENCODE_RELATIVE_PID` 参数：`Kp=24, Ki=0, Kd=0, max_out=10, max_iout=0.5`。

## 2. 改动文件列表

- `application/shoot.h`
- `application/shoot.c`
- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `docs/02_ARCHITECTURE.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-27_pitch_gravity_feedforward_bullet_count.md`

## 3. 关键改动摘要

- `application/shoot.h`：
  - 新增 `BULLET_PER_ROUND 10` 宏。
  - 在 `shoot_control_t` 中新增 `uint16_t bullet_fired_count` 字段。
- `application/shoot.c`：
  - `shoot_init()` 中初始化 `shoot_control.bullet_fired_count = 0;`
  - `shoot_bullet_control()` 在弹丸到位（`move_flag` 清零）分支执行 `bullet_fired_count++`。
- `application/gimbal_task.h`：
  - 保持并固化 `PITCH_ENCODE_RELATIVE_PID_*` 为 `24/0/0/10/0.5`。
  - 新增 `PITCH_GRAVITY_K_FIXED`、`PITCH_GRAVITY_K_AMMO_FULL`、`PITCH_AMMO_CAPACITY`。
- `application/gimbal_task.c`：
  - 新增 `extern shoot_control_t shoot_control;`
  - 重写 `gimbal_motor_relative_angle_control()`：在 pitch 电机路径叠加
    `compensation = (K_FIXED + K_AMMO_FULL * ammo_ratio) * cos(relative_angle)`，
    并按 `PITCH_TURN` 方向处理电流符号。
- docs 同步：
  - 更新架构数据流、模块补充说明、风险日志、状态页和索引。

## 4. 关键决策与理由

- 决策 1：重力项放在 pitch `ENCONDE` 模式的电流输出端（相对角环 + 速度环之后）叠加前馈。  
  理由：不改变原有双环闭环结构，保持原调参结果，且可直接抵消负载重力矩。
- 决策 2：发射计数使用现有拨弹“单发到位”判定点递增。  
  理由：无需新增传感器或裁判系统依赖，复用现有 `set_angle` 到位逻辑即可闭环。
- 决策 3：维持 `Kp=24, Ki=0`。  
  理由：上轮实测已确认该组在空仓工况稳定，问题主要来自负载重力项而非比例参数不足。
- 备选方案与放弃理由：
  - 方案 A：继续提高 `Kp`。放弃原因：可能加剧动态工况振荡，且不能从根因抵消重力矩。
  - 方案 B：引入 `sin/cos` 分段表。放弃原因：当前算力与实时性允许直接 `arm_cos_f32` 计算，简化实现与维护。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增 `osDelay/vTaskDelay` 或阻塞 I/O。
- CPU 影响：`gimbal_task` 1ms 路径新增一次 `arm_cos_f32` 与少量标量运算，仅在 pitch 相对角控制分支执行。
- 栈影响：未新增大对象或递归；仅局部 `fp32` 变量。
- 帧率/周期：任务周期和 CAN 发送节拍不变。

## 6. 风险点

- 风险 1：`bullet_fired_count` 基于“拨弹到位”计数，遇到卡弹反转或人工复位时可能与真实剩弹不一致。
- 风险 2：`K_FIXED/K_AMMO_FULL` 初值误差会导致过补偿或欠补偿，可能引发低头方向稳态偏差或电流偏高。
- 风险 3：`PITCH_TURN` 方向符号若与机构方向不一致，会放大误差而非补偿。

## ⚠ 未验证假设

- 假设：`shoot_bullet_control()` 的到位判定与“实际发射一发”一一对应。  
  未验证原因：当前会话未做实弹连发统计对比。  
  潜在影响：剩弹估计偏差，导致前馈衰减曲线错误。  
  后续验证计划：连续发射 50 发，统计 `bullet_fired_count` 与实发数差值（目标误差 <= 1）。
- 假设：`PITCH_AMMO_CAPACITY=300`、单发质量近似恒定。  
  未验证原因：容量与弹重为先验估计，未做本机构实测称重。  
  潜在影响：`ammo_ratio` 与真实重力变化不匹配，在线标定范围增大。  
  后续验证计划：称重 + 分段装弹（空/半/满）标定并拟合补偿曲线。
- 假设：`PITCH_TURN=1` 对应当前补偿符号分支。  
  未验证原因：未执行上板方向验证。  
  潜在影响：补偿方向反向，电流与误差恶化。  
  后续验证计划：空仓静态小角度上下摆动，观察补偿开关前后误差与电流方向是否符合预期。

## 7. 验证方式与结果

- 串口：本轮未新增串口日志。
- USB：待用 MCP/VOFA+ 采集 `pitch_relative/pitch_relative_set/pitch_given_current/bullet_fired_count`。
- 示波器：本轮未执行。
- 上位机：待执行三阶段验收：
  - 空仓：标定 `K_FIXED`；
  - 装弹：标定 `K_AMMO_FULL`；
  - 连发：验证 `bullet_fired_count` 递增与补偿衰减趋势。
- 长跑：未执行（待实机 10min 连续动作与温升观测）。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 删除 `gimbal_motor_relative_angle_control()` 中 pitch 前馈分支。
  - 删除 `shoot_control_t::bullet_fired_count` 及其初始化/递增逻辑。
- 参数回滚点：
  - `PITCH_GRAVITY_K_FIXED/PITCH_GRAVITY_K_AMMO_FULL` 置零可等效关闭前馈。
  - `PITCH_ENCODE_RELATIVE_PID_*` 保持当前固化值，若需回退按历史会话恢复。
- 运行时保护：
  - 烧录回退版本前，保持拨杆在安全挡并确认 CAN 下发为零后再切换。

## 9. 附件/证据

- 数据证据（上轮）：`data/tune_2026-02-27_008.csv`、`009.csv`、`010.csv`、`012.csv`
- 关联风险条目：`docs/risk_log.md`（2026-02-27 新增行）
- 关联状态页：`docs/ai_sessions/CURRENT_STATE.md`（2026-02-27 新增节）

## 10. 控制链路三问（规则 6.1）

1. 当前故障的实际执行路径是什么？  
   - 实际工况为 `pitch_mode=ENCONDE`；链路为 `gimbal_motor_relative_angle_control()` 的角度环 + 速度环输出。  
   - 依据：上轮 MCP 对比记录（空仓 vs 4/5 仓）显示相同角度点电流需求显著增加且低头残差存在。
2. 本次修改命中的是哪条路径？  
   - 直接命中 `gimbal_motor_relative_angle_control()` 的 `pitch` 分支，在最终 `current_set` 叠加重力前馈补偿。
3. 如何用数据证明本次修改生效？  
   - 观测列：`pitch_relative`、`pitch_relative_set`、`pitch_given_current`、`bullet_fired_count`。  
   - 预期：装弹低头方向稳态误差下降，静态维持电流下降；连发后 `bullet_fired_count` 增长且补偿幅度随之减小。
