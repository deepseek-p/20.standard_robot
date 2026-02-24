# 2026-02-24 chassis selfprotect frame rotation compensation

## 1. 目标

- 本次要解决的问题：UP（HUST_SelfProtect）小陀螺模式下，平移指令随底盘自旋而画圆，无法保持“枪口方向直线平移”手感。
- 预期可观测结果：UP 模式持续自旋时，推前/推侧的实际运动方向不再随底盘角度漂移，平移方向相对云台朝向稳定。

## 2. 改动文件列表

- `application/chassis_behaviour.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-24_chassis_selfprotect_frame_rotation_compensation.md`

## 3. 关键改动摘要

- `application/chassis_behaviour.c`：
- 在 `chassis_hust_self_protect_control()` 中，`chassis_rc_to_control_vector()` 后新增 `relative_angle` 的 `sin/cos` 坐标旋转，将云台/操作手坐标系 `vx/vy` 变换到底盘坐标系。
- 保留原有 `CHASSIS_VECTOR_NO_FOLLOW_YAW` 映射、`vx/vy` 缩放和 `wz` 固定自旋逻辑不变。
- `docs/04_MODULE_MAP.md`：
- 同步标注 `CHASSIS_HUST_SELF_PROTECT` 增加了“平移坐标补偿（基于 `relative_angle`）”。
- `docs/risk_log.md`：
- 新增 UP 小陀螺“平移画圆”风险条目，状态设为 `In Progress`，待实机四场景回归。
- `docs/INDEX.md`：
- 新增本会话记录入口。

## 4. 关键决策与理由

- 决策 1：只改行为函数，不改模式映射与 `chassis_task` 主链路。
- 理由：问题在 UP 行为输出坐标系不匹配，最小修复面是 `chassis_hust_self_protect_control()` 内部完成坐标变换。
- 决策 2：复用 FOLLOW 分支同源旋转公式。
- 理由：已有实战路径，语义一致，降低新增数学错误风险。
- 备选方案与放弃理由：
- 备选 A：改 `CHASSIS_VECTOR_NO_FOLLOW_YAW` 分支做统一旋转。
- 放弃理由：会影响所有 no-follow 行为，不符合“只修 UP”约束。
- 备选 B：把 UP 改成 FOLLOW 模式。
- 放弃理由：会改变小陀螺控制语义，与当前模式设计不一致。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增/删除 `osDelay`、`vTaskDelay` 或等待机制。
- CPU 影响：每控制周期新增一次 `sin/cos` 与常数级乘加，负载轻微上升。
- 栈影响：无任务栈配置变化，无大对象新增。
- 帧率/周期：任务周期与调度时序不变。

## 6. 风险点

- 风险 1：`relative_angle` 方向符号若与实机安装方向不一致，会出现平移轴镜像或反向。
- 风险 2：高速自旋叠加平移时，若地面附着差异大，仍可能看到轻微轨迹偏移（非控制链路问题）。

## ⚠ 未验证假设

- 假设：当前 `relative_angle` 的方向定义与 UP 模式机械安装方向一致，所用旋转符号无需再取反。
- 未验证原因：本次仅完成代码与文档落地，尚未拿到改后 UP 回归数据。
- 潜在影响：若符号不一致，UP 平移会出现“左/右交换”或“前后镜像”。
- 后续验证计划：执行 UP 四场景（静止自旋、自旋+推前、自旋+推侧、MID/UP 切挡）并回传 VOFA+ 数据；负责人 `@Codex`。

## 7. 验证方式与结果

- 串口：已执行（用户回传改后数据段）。
- USB：已执行（VOFA+ 观测）。
- 示波器：未执行。
- 上位机：已执行（VOFA+）。
- 长跑：未执行。

### 7.1 上轮验收结论（补录）

- 结论：部分通过，未闭环。
- 现象：`vx_set/vy_set` 已随 `relative_angle` 变化，说明坐标补偿链路生效；但世界坐标系下合速度方向仍随时间旋转，轨迹“圈变小但未消除”。
- 根因：`sin_yaw` 使用了 `sin(-relative_angle)`，方向符号与目标变换不一致，导致未完全抵消底盘自旋。

## 8. 回滚方式（如何恢复）

- 代码回滚点：`application/chassis_behaviour.c` 中 `chassis_hust_self_protect_control()` 删除 `sin/cos` 旋转段，恢复为直接缩放 `vx/vy`。
- 参数回滚点：本次无参数宏修改。
- 运行时保护：建议先架空验证，再低速落地验证。

## 9. 附件/证据

- 需求与现象来源：本轮用户描述“UP 推前进画圆”。
- 数据路径：`data.md`（后续补充改后数据）。
- 相关风险条目：`docs/risk_log.md` 2026-02-24 新增条目。
