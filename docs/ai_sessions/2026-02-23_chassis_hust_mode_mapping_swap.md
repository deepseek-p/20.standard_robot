# 2026-02-23 底盘 HUST 模式映射替换（ATTI/GPS）

## 1. 目标

- 本次要解决的问题：将遥控 `s0` 挡位语义改为 `GPS(s0=UP)=HUST_SelfProtect`、`ATTI(s0=MID)=HUST_Act`，替换官方默认挡位行为。
- 预期可观测结果：`s0=UP` 时底盘进入常值旋转的小陀螺行为；`s0=MID` 时底盘恢复跟随云台的正常机动行为。

## 2. 改动文件列表

- `application/chassis_behaviour.h`
- `application/chassis_behaviour.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-23_chassis_hust_mode_mapping_swap.md`

## 3. 关键改动摘要

- `application/chassis_behaviour.h`：新增 `CHASSIS_HUST_SELF_PROTECT` 行为枚举和 3 个参数宏（旋转角速度、`vx/vy` 缩放）。
- `application/chassis_behaviour.c`：新增 `chassis_hust_self_protect_control(...)` 控制函数；`s0=UP` 映射为 `CHASSIS_HUST_SELF_PROTECT`，`s0=MID` 映射为 `CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW`（作为 HUST_Act）。
- `application/chassis_behaviour.c`：为 `CHASSIS_HUST_SELF_PROTECT` 绑定 `CHASSIS_VECTOR_NO_FOLLOW_YAW` 控制模式，并在行为分发中接入新控制函数。
- `docs/04_MODULE_MAP.md`：同步 `chassis_behaviour` 的当前 `s0` 挡位语义。
- `docs/risk_log.md`：新增本次模式切换相关风险条目。

## 4. 关键决策与理由

- 决策 1：不改 `chassis_task` 主控链路，仅在 `chassis_behaviour` 做最小侵入替换。  
  理由：保持官方底盘解算和 PID 结构稳定，降低回归风险。
- 决策 2：`HUST_Act` 直接复用 `CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW`。  
  理由：该模式本身就是底盘跟随云台行为，和“Act”目标一致，不需要新增重复控制器。
- 备选方案与放弃理由：将 `SelfProtect` 做成独立任务或重写 `chassis_task`。  
  放弃理由：改动面过大，且当前诉求仅为挡位功能替换。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增 `osDelay`/`vTaskDelay`，无阻塞路径变化。
- CPU 影响：仅新增一次行为分支判断和少量标量运算（`vx/vy` 缩放 + `wz` 常值选择），开销可忽略。
- 栈影响：未新增大对象或深调用链，预计无明显栈压力变化。
- 帧率/周期：未改 `chassis_task` 周期（2ms）与调度关系。

## 6. 风险点

- `SelfProtect` 为持续旋转模式，若参数偏激进可能带来电流峰值和温升风险。
- ATTI/GPS 语义与官方默认不同，操作手若沿用旧习惯可能误切入小陀螺。

## ⚠ 未验证假设

- 假设：当前遥控器 `s0` 物理挡位和代码 `switch_is_up/switch_is_mid` 语义一致（UP=GPS，MID=ATTI）。
- 未验证原因：本次会话仅完成代码与文档落地，未接实机进行拨挡联测。
- 潜在影响：若遥控映射与预期相反，实机会出现“挡位功能颠倒”。
- 后续验证计划：上电后依次测试 `s0=UP/MID/DOWN`，同步观察 `chassis_behaviour_mode`、`chassis_mode`、`wz_set`，并记录 3 段日志（静止、平移、切挡）。

## 7. 验证方式与结果

- 代码级验证：已检查 `chassis_behaviour_mode_set` 映射与 `chassis_behaviour_control_set` 分发链闭合。
- 枚举/宏验证：已确认 `CHASSIS_HUST_SELF_PROTECT` 及参数宏在头文件定义并被实现引用。
- 实机验证：未执行（当前会话无硬件在线）。

## 8. 回滚方式（如何恢复）

- 代码回滚点：还原 `application/chassis_behaviour.c` 与 `application/chassis_behaviour.h` 到改动前版本。
- 参数回滚点：保留行为不变时，可先下调 `CHASSIS_HUST_SELF_PROTECT_WZ` 或将 `VX/VY` 缩放改小。
- 运行时保护（例如先发零电流）：首次上电联调先在抬轮状态验证，确认无异常再落地。

## 9. 附件/证据

- 日志路径：本次未产出实机日志。
- 截图/波形：本次未产出示波器/上位机截图。
- 相关风险条目：`docs/risk_log.md:10`、`docs/risk_log.md:11`。
