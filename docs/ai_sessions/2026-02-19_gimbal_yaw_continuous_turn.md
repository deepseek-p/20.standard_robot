# 2026-02-19_gimbal_yaw_continuous_turn

## 1. 目标

- 本次要解决的问题：在硬件已移除 yaw 限位并加装滑环的前提下，软件取消 yaw 机械限位夹紧，支持无限旋转。
- 预期可观测结果：`GIMBAL_ABSOLUTE_ANGLE` 与 `GIMBAL_RELATIVE_ANGLE` 两种模式下，yaw 不再在原机械角边界停住；云台校准流程跳过 yaw 步骤，避免校准卡死。

## 2. 改动文件列表

- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `application/gimbal_behaviour.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`

## 3. 关键改动摘要

- `application/gimbal_task.h`：新增 `GIMBAL_YAW_CONTINUOUS_TURN` 编译开关，默认开启。
- `application/gimbal_task.c`：
  - `gimbal_absolute_angle_limit` / `gimbal_relative_angle_limit` 增加 `is_yaw` 参数。
  - `gimbal_set_control` 中 yaw 与 pitch 分别传入 `is_yaw=1/0`。
  - 连续旋转开关开启时，yaw 在两种模式下不再执行 `max_relative_angle/min_relative_angle` 夹紧，仅做 `rad_format`。
  - `set_cali_gimbal_hook` 连续旋转模式下强制 yaw 运行区间为 `[-PI, PI]`，忽略传入 yaw 限位数据。
  - `cmd_cali_gimbal_hook` 在校准结束分支连续旋转模式下强制 `*max_yaw=PI`、`*min_yaw=-PI`，并仅对 pitch 应用冗余角修正。
- `application/gimbal_behaviour.c`：
  - 在 `gimbal_cali_control` 中，连续旋转模式下进入 yaw 校准步骤时直接跳到 `GIMBAL_CALI_END_STEP`，并清零当周期 yaw/pitch 输出。
- `docs/04_MODULE_MAP.md`：补充 yaw 连续旋转开关与校准跳步行为。
- `docs/risk_log.md`：新增滑环连续旋转相关高风险条目。
- `docs/INDEX.md`：新增本次会话记录入口。

## 4. 关键决策与理由

- 决策 1：绝对角与相对角模式都取消 yaw 机械限位夹紧。
  - 理由：需求明确为 yaw 无限旋转，且硬件已支持滑环。
- 决策 2：保留 `INIT_YAW_SET` 回中逻辑。
  - 理由：不改变现有上电/解锁操作习惯，缩小行为改动面。
- 决策 3：连续旋转模式下跳过 yaw 校准步骤，仅保留 pitch 校准。
  - 理由：无机械限位条件下 yaw 校准依赖“停转判定”不再可靠，存在卡死风险。
- 决策 4：保留旧结构体与 flash 布局，不做迁移。
  - 理由：降低改动风险，保证可快速回退。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增/删除 `osDelay`、`vTaskDelay`、任务等待机制。
- CPU 影响：仅新增少量条件分支判断，位于现有 1ms 控制路径，计算量可忽略。
- 栈影响：未新增大对象、未新增递归；未观测到需要额外栈扩容的证据。
- 帧率/周期：未改任务周期、优先级、调度顺序；控制链路时序不变。

## 6. 风险点

- 风险 1：操作员持续高速大幅输入可能超出滑环长期工况，出现温升与寿命风险。
- 风险 2：现场维护若沿用旧认知触发云台校准，可能误解“yaw 不再校准”为异常。
- 风险 3：若该固件刷入未装滑环机体，会出现线缆缠绕风险。

## ⚠ 未验证假设

- 假设内容：当前机体 yaw 已正确安装滑环，允许连续旋转且线束管理满足长期工况；`GIMBAL_YAW_CONTINUOUS_TURN=1` 与现有电机/PID 参数组合在全温区可稳定工作。
- 为什么当前无法验证：本次为代码落地与文档交接，未接入实机、示波器与上位机链路，无法完成硬件闭环验证。
- 潜在影响：若假设不成立，可能出现电流异常、温升过快、长期可靠性下降或绕线风险。
- 后续验证计划：由实机调试负责人执行 6.2/6.3 场景（绝对/相对连续旋转、反向切换、校准流程、10min 长跑、上位机与示波器记录），验证通过后关闭 `docs/risk_log.md` 对应风险条目。

## 7. 验证方式与结果

- 串口：已定义验证项（检查控制链连续性与异常输出）；本次未实机执行。
- USB：已定义验证项（观察 `YAW_GIMBAL_MOTOR_TOE` 状态稳定性）；本次未实机执行。
- 示波器：已定义验证项（yaw 电流跨边界无异常尖峰）；本次未实机执行。
- 上位机：已定义验证项（yaw 角度/电流曲线无平台夹紧）；本次未实机执行。
- 长跑：已定义验证项（连续旋转与往返各 10min 温升/告警检查）；本次未实机执行。

## 8. 回滚方式（如何恢复）

- 代码回滚点：将 `application/gimbal_task.h` 中 `GIMBAL_YAW_CONTINUOUS_TURN` 改为 `0`，恢复原 yaw 机械限位与校准逻辑。
- 参数回滚点：如需恢复旧校准行为，保留原 `max_yaw/min_yaw` 使用路径并重新执行云台校准流程。
- 运行时保护（例如先发零电流）：回滚后首次上电保持遥控输入为零，确认云台模式切换正常后再恢复动作。

## 9. 附件/证据

- 日志路径：无（本次未连接实机）。
- 截图/波形：无（本次未连接示波器/上位机）。
- 相关风险条目：`docs/risk_log.md`（2026-02-19，`gimbal_task` / `gimbal_behaviour`）。
