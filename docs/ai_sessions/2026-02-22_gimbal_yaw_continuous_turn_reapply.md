# 2026-02-22_gimbal_yaw_continuous_turn_reapply

## 0. 改码前三问（6.1）

1. 当前故障的实际执行路径是什么？
- 结合已回传日志与现象，当前版本主要问题是“yaw 在软件限位逻辑与校准流程仍按有机械限位假设运行”：
  - 控制路径仍经过 `gimbal_absolute_angle_limit()`/`gimbal_relative_angle_limit()` 的 yaw 夹紧；
  - 校准路径仍进入 `GIMBAL_CALI_YAW_MAX_STEP/MIN_STEP`，无机械限位时存在长时间寻限位风险。

2. 本次修改命中的是哪条路径？
- 直接命中上述两条路径：
  - 在 `application/gimbal_task.c` 对 yaw 角度限位函数做连续旋转旁路；
  - 在 `application/gimbal_behaviour.c` 的 `gimbal_cali_control()` 中，连续旋转模式下跳过 yaw 校准两步并直达结束。

3. 如何用数据证明本次修改生效？
- 关键观测量与预期：
  - `yaw_relative_set`/`yaw_absolute_set`：持续同向输入时不再被 min/max 夹紧平台化；
  - `gimbal_cali.step`：校准时从 pitch 两步后直接进入 `GIMBAL_CALI_END_STEP`，不再停留 yaw max/min；
  - `yaw_given_current`：跨越原边界附近无突变停顿。

## 1. 目标

- 恢复 yaw 连续旋转软件适配（匹配已移除机械限位 + 已安装滑环硬件）。
- 保持 INIT 回中行为不变。
- 校准流程改为“仅 pitch 两步，跳过 yaw 两步”。
- 保留单宏回退能力（`GIMBAL_YAW_CONTINUOUS_TURN`）。

## 2. 改动文件列表

- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `application/gimbal_behaviour.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-22_gimbal_yaw_continuous_turn_reapply.md`

## 3. 关键改动摘要

- `application/gimbal_task.h`
  - 新增 `#define GIMBAL_YAW_CONTINUOUS_TURN 1`，作为连续旋转总开关。

- `application/gimbal_task.c`
  - `gimbal_absolute_angle_limit` / `gimbal_relative_angle_limit` 签名改为带 `is_yaw`。
  - `gimbal_set_control()` 中 yaw 调用传 `1`，pitch 调用传 `0`。
  - 连续旋转开关开启时，yaw 在两种模式下只做 `rad_format(set + add)`，不做 min/max 夹紧。
  - `set_cali_gimbal_hook()`：连续旋转模式下忽略输入 yaw 限位，运行时强制 yaw 为 `[-PI, PI]`。
  - `cmd_cali_gimbal_hook()` 结束分支：保留 `calc_gimbal_cali()` 链路，但连续模式下覆盖输出 `*max_yaw=PI`、`*min_yaw=-PI`，冗余角仅作用于 pitch。

- `application/gimbal_behaviour.c`
  - `gimbal_cali_control()`：连续旋转模式下若进入 yaw max/min 步，直接置 `step=GIMBAL_CALI_END_STEP`，并输出 `yaw=0/pitch=0`、清 `cali_time`。
  - `GIMBAL_CALI_END_STEP` 分支显式置零输出，避免残留原始电流指令。

## 4. 关键决策与理由

- 决策 1：采用编译期开关控制连续旋转。
  - 理由：可快速回退到旧行为，便于混编不同硬件条件。

- 决策 2：不改数据结构与通信字段，仅在运行时覆盖 yaw 校准输出。
  - 理由：避免 flash/协议兼容成本，缩小本轮改动面。

- 备选方案与放弃理由：
  - 方案：完全删除 yaw 校准状态与字段。
  - 放弃原因：影响现有校准链路与兼容性，回退成本高。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增阻塞、无新增 `osDelay/vTaskDelay`。
- CPU 影响：仅新增少量条件分支判断，可忽略。
- 栈影响：无新增大对象与深层调用，任务栈需求不变。
- 帧率/周期：不改任务周期与控制环节拍。

## 6. 风险点

- 风险 1：未装滑环机体误刷该版本会出现绕线风险。
- 风险 2：滑环机体在长时间高速连续旋转下可能出现温升/寿命风险。
- 风险 3：维护人员对“yaw 校准已跳过”的预期不一致，可能误判现场状态。

## ⚠ 未验证假设

- 假设：当前硬件滑环在目标工况下可承受连续旋转，不会因温升触发退化。
- 未验证原因：本轮为代码与文档落地回合，尚未完成新的 10 分钟长跑验证。
- 潜在影响：长时间任务中可能出现温升上升、可靠性下降。
- 后续验证计划：
  - 负责人：现场联调同学；
  - 方法：ATTI/GPS 下分别执行同向连续旋转与往返旋转各 10 分钟，记录电流与温升；
  - 触发条件：下一轮实机联调窗口。

## 7. 验证方式与结果

- 串口：待实机复测（关注 `gimbal_cali.step` 与 yaw 设定值连续性）。
- USB：待复测（通过 FireWater 观察 yaw set/feedback、cali step）。
- 示波器：待复测（边界跨越时 yaw 电流是否有尖峰）。
- 上位机：待复测（无夹紧平台段、无长时间 yaw 校准寻限位）。
- 长跑：待复测（连续旋转与往返旋转各 10 分钟）。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 将 `GIMBAL_YAW_CONTINUOUS_TURN` 改为 `0`；
  - 若需彻底回退，可恢复本次会话前 `gimbal_task.c` 与 `gimbal_behaviour.c`。
- 参数回滚点：
  - 保留原 yaw/pitch 校准数据结构，不需要 flash 迁移。
- 运行时保护：
  - 回滚后先在零输入下上电，确认 yaw 限位恢复，再进入动态动作。

## 9. 附件/证据

- 相关风险条目：`docs/risk_log.md`
- 模块职责更新：`docs/04_MODULE_MAP.md`
- 规则入口：`docs/00_AI_HANDOFF_RULES.md`
