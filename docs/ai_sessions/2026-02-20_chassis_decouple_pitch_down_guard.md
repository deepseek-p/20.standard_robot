# 2026-02-20_chassis_decouple_pitch_down_guard

## 1. 目标

- 本次要解决的问题：在 yaw 连续旋转版本中，先做两项最小止血改动，降低实机抽搐和 pitch 下冲风险。
- 预期可观测结果：
  - 底盘上档不再进入“跟随云台角”闭环，避免 GPS 档位出现持续自转/换向抽搐。
  - pitch 向下大幅输入时，不再出现明显的大幅来回冲撞，风险区优先收敛。

## 2. 改动文件列表

- `application/chassis_behaviour.c`
- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/2026-02-20_chassis_decouple_pitch_down_guard.md`

## 3. 关键改动摘要

- `application/chassis_behaviour.c`
  - 在 `switch_is_up(CHASSIS_MODE_CHANNEL)` 分支中增加编译期开关判断。
  - 当 `GIMBAL_YAW_CONTINUOUS_TURN = 1` 时，上档强制映射到 `CHASSIS_NO_FOLLOW_YAW`，不再进入 `CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW`。
- `application/gimbal_task.h`
  - 新增 pitch 下行保护宏：
  - `GIMBAL_PITCH_DOWN_PROTECT_ENABLE`
  - `GIMBAL_PITCH_DOWN_ADD_LIMIT`
  - `GIMBAL_PITCH_DOWN_SOFT_GUARD_ANGLE`
- `application/gimbal_task.c`
  - 在 `gimbal_absolute_angle_limit` 增加 pitch 分支下行保护：
  - 每周期向下增量限幅（防止单周期过大下冲）。
  - 使用 `min_relative_angle + guard` 作为软边界，提前收敛到安全区。
  - 在 `gimbal_relative_angle_limit` 同步加入等价保护，保证相对角模式行为一致。
  - 维持 yaw 连续旋转逻辑不变（yaw 仍为 `rad_format(set + add)` 旁路限位）。
- `docs/04_MODULE_MAP.md`
  - 更新 `gimbal_task` 行，补充“pitch 下行软件保护”职责。
  - 更新 `chassis_behaviour` 行，补充“连续 yaw 版本上档强制不跟随”。
- `docs/risk_log.md`
  - 新增 2 条 2026-02-20 风险项，分别记录“上档语义变化风险”和“pitch 软保护参数未定标风险”。

## 4. 关键决策与理由

- 决策 1：连续 yaw 版本先禁用上档跟随闭环。
  - 理由：你的实测已出现 GPS 档位底盘自转与换向抽搐，先切到 `NO_FOLLOW` 能最快隔离问题链路。
- 决策 2：pitch 先做“下行软保护”，不先改 PID。
  - 理由：当前机器是连杆驱动且下方向无机械限位，先收敛危险动作范围比直接调 PID 更安全。
- 备选方案与放弃理由：
  - 直接重调 PID：暂不采用，先保证硬件安全边界。
  - 全链路重构底盘/云台耦合：改动面过大，不符合本轮“最小止血”目标。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增 `osDelay`、无忙等、无阻塞式外设访问。
- CPU 影响：仅增加少量分支判断与加减比较，发生在既有限幅函数中，开销可忽略。
- 栈影响：仅增加函数局部标量变量，未引入大对象。
- 帧率/周期：未改任务周期、未改调度优先级、未改 CAN 帧映射。

## 6. 风险点

- 风险 1：上档改为不跟随后，操作手感与既有习惯会变化，可能引发“模式理解偏差”。
- 风险 2：pitch 下行软保护参数当前为安全优先初值，可能压缩下视角可用范围。
- 风险 3：如果现场继续高速连续旋转，滑环热与寿命风险仍需长期监测。

## ⚠ 未验证假设

- 假设内容：
  - `GIMBAL_PITCH_DOWN_ADD_LIMIT = 0.0012f` 与 `GIMBAL_PITCH_DOWN_SOFT_GUARD_ANGLE = 0.12f` 对当前连杆机构是“安全且不过度保守”的初值。
  - 连续 yaw 版本下，上档强制 `CHASSIS_NO_FOLLOW_YAW` 能显著降低 GPS 工况抽搐。
- 未验证原因：
  - 已完成多轮实机上电与 USB 遥测采样（覆盖 ATTI/GPS、静止与摇杆输入），但尚未补齐示波器波形与长跑温升证据。
- 潜在影响：
  - 参数过大：仍可能出现 pitch 下冲。
  - 参数过小：可能导致下视角不足，影响瞄准覆盖范围。
  - 上档语义变化若未传达，会造成现场误操作判断偏差。
- 后续验证计划：
  - 负责人：@Codex + 实机调试人员。
  - 方法：按既定上电脚本复测 ATTI/GPS 的静止/左摇杆/右摇杆场景，重点记录 pitch 阈值与底盘是否仍抽搐。
  - 触发条件：下一轮上电联调开始时立即执行，并同步更新 `docs/risk_log.md` 状态。

## 7. 验证方式与结果

- 串口：
  - 本轮未新增串口埋点。
- USB：
  - 已完成实机 FireWater 数据回传验证。
  - ATTI 场景下静止与摇杆输入可运行，未见“上电即抽搐”。
  - GPS 场景下问题仍复现：底盘高速旋转、云台低速反向，pitch 仍存在摆动并有下冲到底现象。
- 示波器：
  - 尚未执行；下一步重点看 pitch 电流在下行大步进时的尖峰与持续饱和区间。
- 上位机：
  - 已通过上位机/串口助手观察到 GPS 工况异常可复现，但尚未沉淀标准化曲线文件。
- 长跑：
  - 已有短时运行样本（约 5 分钟）无卡死，但功能异常仍在，暂不能判定通过。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - `application/chassis_behaviour.c` 上档分支恢复为 `CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW`。
  - `application/gimbal_task.h` 将 `GIMBAL_PITCH_DOWN_PROTECT_ENABLE` 置 `0`（或恢复旧宏参数）。
  - `application/gimbal_task.c` 去除 pitch 下行软保护分支。
- 参数回滚点：
  - 仅保留 `GIMBAL_YAW_CONTINUOUS_TURN`，清空 `GIMBAL_PITCH_DOWN_*` 保护参数。
- 运行时保护：
  - 回滚后首次上电先零输入解锁，再小幅摇杆逐步验证，不直接大幅打杆。

## 9. 附件/证据

- 代码差异：`git diff -- application/chassis_behaviour.c application/gimbal_task.h application/gimbal_task.c`
- 风险关联：`docs/risk_log.md`（2026-02-20 两条新增项）
- 模块地图关联：`docs/04_MODULE_MAP.md`
