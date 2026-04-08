# AI 会话记录：临时禁用 TRIGGER_MOTOR 离线检测
日期：2026-03-03

## 0. 上轮验收结论
- 上轮会话：`docs/ai_sessions/2026-03-02_shoot_single_fire_reverse_grid_fix.md`
- 数据来源：代码审阅 + 用户口述（拨弹电机已物理拆除维修）
- 状态：`In Progress`（本次仅做临时安全改动，待电机装回后回退）

## 1. 目标
- 临时关闭拨弹电机离线报警，避免硬件拆除期间系统持续报错。
- 严格限定改动边界：仅 `application/detect_task.c` 增加一行禁用语句。

## 2. 改动文件列表
- `application/detect_task.c`

## 3. 关键改动摘要
- 在 `detect_init()` 末尾（`error_list[VT03_TOE].solve_data_error_fun = NULL;` 之后）新增：
  - `error_list[TRIGGER_MOTOR_TOE].enable = 0;`
- 同位置添加临时注释，明确“电机维修后需删除该行恢复检测”。
- 未修改 `detect_task.h`、未修改其它 TOE 条目、未修改 `set_item` 数组。

## 4. 关键决策与理由
- 决策：使用 `enable=0` 关闭 `TRIGGER_MOTOR_TOE` 检测，而不是调大离线时间阈值。
- 理由：硬件已物理拆除，阈值调参不能消除持续离线本质；直接禁用可最小化告警噪声且改动最小。
- 放弃方案：不改代码仅忽略蜂鸣/日志。原因是会持续污染现场诊断信息，掩盖其它真实故障。

## 5. 实时性影响（必须写）
- 未新增阻塞调用（无 `osDelay/vTaskDelay` 变化）。
- 未新增任务、未改变任务优先级、未改变中断路径。
- 每周期检测逻辑仅减少一个 TOE 分支判断，CPU 影响可忽略。

## 6. 风险点
- 风险 1：拨弹电机装回后若忘记删除该行，会导致真实离线故障无法被 detect 发现。
- 风险 2：联调阶段可能误以为拨弹链路健康，实际只是在检测层被屏蔽。

## ⚠ 未验证假设
- 假设：当前阶段不依赖 `TRIGGER_MOTOR_TOE` 的在线状态参与其它安全联锁分支。
- 未验证原因：本轮未进行全链路实机回归，只执行了定点代码改动。
- 潜在影响：若存在隐式联锁依赖，可能造成“检测静默但功能异常”。
- 后续验证计划：电机装回当天先删除该行，再执行检测回归（上电 30s + 发射链路测试 0~8）。

## 7. 验证方式与结果
- 代码级验证：已确认目标位置出现且仅出现一处 `error_list[TRIGGER_MOTOR_TOE].enable = 0;`。
- 编译验证：本会话未执行 Keil/armcc 编译（待本地 F7）。
- 实机验证：本会话未执行（硬件处于拆卸维修态）。

## 8. 回滚方式（如何恢复）
- 回滚点：删除 `application/detect_task.c` 中临时行 `error_list[TRIGGER_MOTOR_TOE].enable = 0;`。
- 恢复后动作：重新编译烧录并确认 `TRIGGER_MOTOR_TOE` 可正常上报在线/离线状态。

## 9. 附件/证据
- 代码位置：`application/detect_task.c`（`detect_init()` 末尾）
- 关联风险项：`docs/risk_log.md`（2026-03-03 新增条目）
