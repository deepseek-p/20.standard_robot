# 2026-02-22_pitch_no_cmd_disturbance_rate_limit

## 0. 根因定位三问（`00_AI_HANDOFF_RULES.md` 6.1）

1. 当前故障的实际执行路径是什么？
- 实机故障帧（由你提供的十六进制帧解码）：
  - `pitch_mode=1`（绝对角/陀螺模式）
  - `rc_ch3=0`、`mouse_y=0`（无 pitch 输入）
  - `pitch_abs=0`、`pitch_abs_set=84`（`abs_err=0.084rad`）
  - `pitch_gyro_set=1279`、`pitch_cur=13732`
- 对应代码路径：`application/gimbal_task.c` 的 `gimbal_motor_absolute_angle_control()` 中 `!is_yaw && gimbal_pitch_no_cmd_detect()` 分支；由于 `abs_err` 超过现阈值 `0.08`，原逻辑退出“小误差归零保护”，进入大纠偏输出，触发电流尖峰。

2. 本次修改命中的是哪条路径？
- 直接命中上述同一分支（同一函数、同一条件块），仅在该路径加入“无输入大误差纠偏限速 + 限幅触发时清速度环积分”。

3. 如何用数据证明本次修改生效？
- 继续使用同列观测：`pitch_mode`、`rc_ch3`、`mouse_y`、`pitch_abs`、`pitch_abs_set`、`pitch_gyro_set`、`pitch_cur`。
- 预期：
  - 在 `pitch_mode=1` 且无输入扰动场景下，`pitch_gyro_set` 峰值受限（软件上限 `±350`，即 `±0.35rad/s`）。
  - `pitch_cur` 峰值相对故障基线 `13732` 显著下降，不再出现“猛地下打到底”。

## 1. 目标

- 本次要解决的问题：GPS/绝对角模式无输入时，手动轻推 pitch 后出现向下猛打。
- 预期可观测结果：无输入扰动场景下，pitch 纠偏速度与电流峰值被压低，机构不再下砸。

## 2. 改动文件列表

- `application/gimbal_task.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-22_pitch_no_cmd_disturbance_rate_limit.md`

## 3. 关键改动摘要

- `application/gimbal_task.c`
  - 在 `gimbal_motor_absolute_angle_control()` 的 `!is_yaw && gimbal_pitch_no_cmd_detect()` 分支中新增：
    - 当小误差保护未命中且 `motor_gyro_set` 过大时，执行 `|motor_gyro_set| <= 0.35rad/s` 限幅；
    - 限幅触发时清理速度环 PID 积分，抑制电流突发。
  - 不新增编译开关宏，仅在现有 `GIMBAL_PITCH_ABS_NO_CMD_GUARD_ENABLE` 分支内增强逻辑。
- `docs/04_MODULE_MAP.md`
  - 补充 `gimbal_task` 新行为：无输入扰动大误差纠偏限速。
- `docs/risk_log.md`
  - 新增本次风险条目，记录故障触发条件、基线数据与缓解措施。
- `docs/INDEX.md`
  - 增加本会话索引入口。

## 4. 关键决策与理由

- 决策 1：不继续加新宏，只改现有无输入保护分支。
  - 理由：符合“禁止防御性宏无限堆积”的新规则，避免再次扩散控制面。
- 决策 2：不先动全局 PID 参数，先做路径内限速。
  - 理由：故障由特定工况触发（无输入 + 扰动 + 绝对角模式），路径内约束更精准，副作用更小。
- 备选方案与放弃理由：
  - 方案 A：直接降低 `PITCH_GYRO_ABSOLUTE_PID_KP`。放弃原因：会影响所有绝对角工况，且不是针对本次已确认路径。
  - 方案 B：无输入时直接重锚定 `absolute_angle_set = absolute_angle`。放弃原因：会削弱无输入姿态保持能力。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增 `osDelay`、`vTaskDelay`、等待机制。
- CPU 影响：仅新增常数比较与一次 `abs_limit`，位于 1ms 控制链，开销可忽略。
- 栈影响：未新增大对象，栈变化可忽略。
- 帧率/周期：未修改任务周期、优先级与调度时序。

## 6. 风险点

- 风险 1：限速后极端扰动恢复时间会变慢。
- 风险 2：若连杆机构存在强非线性/回差，仍可能出现慢速偏移，需要下一轮参数定标。

## ⚠ 未验证假设

- 假设：当前“向下猛打”主因是无输入大误差时纠偏速度过大导致电流尖峰，而非电机相序/机械卡滞故障。
- 未验证原因：本轮仅完成代码修改与日志路径对齐，尚未拿到修改后的实机同场景数据。
- 潜在影响：若根因还包含机械方向映射或装配问题，限速只能减轻冲击，不能根治偏移。
- 后续验证计划：由你按同脚本复测“GPS静止无输入 + 手推 pitch 扰动”，回传 `pitch_gyro_set/pitch_cur` 峰值与是否再下砸；若仍异常，再进入机械方向与电机映射排查。

## 7. 验证方式与结果

- 串口：未执行（本轮以 USB FireWater 帧为主）。
- USB：已完成故障帧解码并定位路径；修改后待你实机复测。
- 示波器：未执行。
- 上位机：待你在 VOFA+ 对比修改前后峰值。
- 长跑：未执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：撤销 `application/gimbal_task.c` 中 `gimbal_motor_absolute_angle_control()` 新增的限速 `else if` 分支。
- 参数回滚点：保留当前宏开关不变，仅去除本次分支即可恢复上一版行为。
- 运行时保护（例如先发零电流）：回滚后先在零输入场景确认无异常电流尖峰，再进行扰动测试。

## 9. 附件/证据

- 关键证据帧（解码后）：
  - `tick=83203, seq=4158, pitch_mode=1, rc_ch3=0, mouse_y=0, pitch_abs=0, pitch_abs_set=84, pitch_gyro_set=1279, pitch_cur=13732`
- 代码差异：
  - `git diff -- application/gimbal_task.c`
- 文档差异：
  - `git diff -- docs/INDEX.md docs/04_MODULE_MAP.md docs/risk_log.md docs/ai_sessions/2026-02-22_pitch_no_cmd_disturbance_rate_limit.md`
