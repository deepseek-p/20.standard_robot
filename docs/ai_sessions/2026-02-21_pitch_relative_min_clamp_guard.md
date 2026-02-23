# 2026-02-21_pitch_relative_min_clamp_guard

## 1. 目标

- 本次要解决的问题：修复 pitch 在 `GIMBAL_MOTOR_ENCONDE` 模式下“无下压指令也被强制夹紧到最小边界”的风险，降低实机出现下砸与持续大电流的概率。
- 预期可观测结果：
  - 在 `rc_ch3=0`、`mouse_y=0` 时，pitch 不应再因为最小边界夹紧逻辑主动下压。
  - USB 调试帧中，`pitch_rel_set` 不再无指令突变到软下限。

## 2. 改动文件列表

- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `docs/INDEX.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/2026-02-21_pitch_relative_min_clamp_guard.md`

## 3. 关键改动摘要

- `application/gimbal_task.h`
  - 新增开关宏 `GIMBAL_PITCH_MIN_CLAMP_REQUIRE_DOWN_CMD`，用于控制“相对角模式最小边界夹紧是否要求下压指令”。
- `application/gimbal_task.c`
  - 在 `gimbal_relative_angle_limit()` 中调整 `target_relative_set < min_limit` 分支：
  - 当 `GIMBAL_PITCH_DOWN_PROTECT_ENABLE && GIMBAL_PITCH_MIN_CLAMP_REQUIRE_DOWN_CMD` 且当前为 pitch 轴时，仅在 `add < 0`（有明确下压指令）时才执行 `min_limit` 夹紧。
  - 当 `add >= 0`（无下压或上抬）时，不再将 setpoint 强行吸附到软下限。
- `docs/04_MODULE_MAP.md`
  - 更新 `gimbal_task` 职责说明与关键宏列表，补充本次保护逻辑。
- `docs/risk_log.md`
  - 新增 2026-02-21 风险条目，记录本次问题机理、缓解措施和复测计划。
- `docs/INDEX.md`
  - 新增本次会话记录入口。

## 4. 关键决策与理由

- 决策 1：采用“方向性最小边界夹紧”，而不是直接删除最小边界保护。
  - 理由：保留向下方向的机械安全边界，同时避免无输入状态下的误动作。
- 决策 2：使用编译期开关宏实现，默认开启。
  - 理由：便于快速回退与 A/B 对比，不破坏现有接口与数据结构。
- 备选方案与放弃理由：
  - 直接关闭 `GIMBAL_PITCH_DOWN_PROTECT_ENABLE`：会丢失向下安全保护，风险更高。
  - 立即重调 pitch PID：当前先修正逻辑触发条件，避免将参数问题与逻辑问题耦合排查。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增 `osDelay` / `vTaskDelay` / 忙等。
- CPU 影响：仅在 `gimbal_relative_angle_limit()` 增加 1 个条件分支，开销可忽略。
- 栈影响：无新增大对象，仅局部标量判断。
- 帧率/周期：未修改任务周期、优先级、CAN/USB 时序路径。

## 6. 风险点

- 风险 1：若现场实际需要“无输入时自动回吸到软下限”的行为，本改动会改变该隐式行为。
- 风险 2：若 `add` 信号存在抖动（正负快速切换），仍可能在边界附近出现细小来回。
- 风险 3：本次未触及 PID 参数，若仍存在机构侧强耦合振荡，可能还需后续调参。

## ⚠ 未验证假设

- 假设：
  - 现场“pitch 下砸/抖动”的主触发之一是相对角最小边界在无下压输入时的错误吸附。
  - 仅修改夹紧触发条件即可显著降低 `pitch_gyro_set=-10000` 与 `pitch_cur=-30000` 的持续饱和现象。
- 未验证原因：
  - 已进行实机复测并采集 GPS 数据，但现象未改善；从数据看当前主要运行在 `pitch_mode=1`（绝对角模式），而本改动作用于 `pitch_mode=2`（相对角模式）路径，存在“改动未命中主异常链路”的风险。
- 潜在影响：
  - 若主异常来源于绝对角控制环或连杆机械耦合，本次改动将不产生可见改善。
- 后续验证计划：
  - 在相同 ATTI/GPS 脚本下继续复测静止、左摇杆、右摇杆场景；
  - 重点记录 USB 列 `24/33/34/37/38/39`（`pitch_mode/pitch_abs/pitch_abs_set/pitch_gyro/pitch_gyro_set/pitch_cur`）；
  - 下一步转入绝对角控制链路排查（含 anti-windup 与机构耦合）。

## 7. 验证方式与结果

- 串口：本轮未新增串口埋点。
- USB：
  - 已完成实机 FireWater 采样回传。
  - ATTI/GPS 现象与改动前相比无明显改善（负向验证）。
  - 采样显示 `pitch_mode` 主要为 `1`（绝对角模式），与本次仅修改 `gimbal_relative_angle_limit()`（相对角模式）一致，说明本改动未命中当前主故障路径。
- 示波器：本轮未执行。
- 上位机：
  - 已有连续数据帧证据，但尚未导出标准化曲线文件。
- 长跑：
  - 本轮未执行专项长跑。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 将 `GIMBAL_PITCH_MIN_CLAMP_REQUIRE_DOWN_CMD` 置 `0`；
  - 或恢复 `gimbal_relative_angle_limit()` 的原最小边界无条件夹紧逻辑。
- 参数回滚点：
  - 保持 `GIMBAL_PITCH_DOWN_PROTECT_ENABLE` 与其他宏不变，仅回退本次新宏。
- 运行时保护：
  - 回滚后首次上电保持零输入，确认 pitch 无突跳后再进行大幅摇杆动作。

## 9. 附件/证据

- 代码差异：`git diff -- application/gimbal_task.h application/gimbal_task.c`
- 文档差异：`git diff -- docs/INDEX.md docs/04_MODULE_MAP.md docs/risk_log.md`
- 关联风险：`docs/risk_log.md`（2026-02-21 新增条目）
