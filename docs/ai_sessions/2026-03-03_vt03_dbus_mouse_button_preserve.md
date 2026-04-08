# AI 会话记录：VT03 不再覆盖 DBUS 鼠标按键
日期：2026-03-03

## 0. 上轮验收结论
- 上轮会话：`docs/ai_sessions/2026-03-02_shoot_single_fire_reverse_grid_fix.md`
- 数据来源：代码路径复核 + 用户口述现象（DBUS 在线时鼠标左右键失效）
- 状态：`In Progress`（已完成代码修复，待实机联机验证）

## 1. 目标
- 修复 VT03 在线时将 `rc_ctrl.mouse.press_l/press_r` 强制清零的问题。
- 保证 DBUS 在线时保留 `sbus_to_rc()` 写入的鼠标按键值；仅在 DBUS 离线时清零。

## 2. 改动文件列表
- `application/vt03_link.c`

## 3. 关键改动摘要
- 在 `vt03_frame_decode()` 中将原先无条件清零：
  - `rc_ctrl.mouse.press_l = 0;`
  - `rc_ctrl.mouse.press_r = 0;`
- 改为条件清零：
  - `if (toe_is_error(DBUS_TOE)) { press_l = 0; press_r = 0; }`
- 其余字段（`mouse.x/y/z`, `key.v`, `rc.s[]`, `rc.ch[]`）未改动。

## 4. 关键决策与理由
- 决策：使用 `toe_is_error(DBUS_TOE)` 判断 DBUS 在线状态来门控清零行为。
- 理由：VT13 无鼠标按键数据，DBUS 离线时应保持 0；但 DBUS 在线时应保留其鼠标键值，避免发射/瞄准输入被覆盖。
- 放弃方案：继续固定清零并在上层补逻辑。原因是会破坏输入源优先级，且会在多源并在线时产生副作用。

## 5. 实时性影响（必须写）
- 未新增阻塞调用，未新增任务/中断路径。
- 仅新增一次 `toe_is_error(DBUS_TOE)` 判断，CPU 开销可忽略。
- 控制周期与帧解析流程不变。

## 6. 风险点
- 若 DBUS 在线状态抖动，`press_l/press_r` 可能在边界帧出现短时切换。
- 若后续引入新的输入源优先级策略，需要统一评估 VT03/DBUS 合流顺序。

## ⚠ 未验证假设
- 假设：`vt03_frame_decode()` 调用时序晚于 `sbus_to_rc()`，因此 DBUS 在线时“保留现值”可稳定继承 DBUS 鼠标按键。
- 未验证原因：本轮未做任务级时序抓取与逻辑分析仪对时，仅完成代码修复。
- 潜在影响：若调用顺序相反，可能仍需在更上层做最终合流仲裁。
- 后续验证计划：实机同时在线测试，采集 `press_l/press_r + evt_cmd_vt03_trigger + shoot_mode`，确认点击链路闭环。

## 7. 验证方式与结果
- 代码验证：已确认 `vt03_frame_decode()` 中清零逻辑改为“仅 DBUS 离线时执行”。
- 编译验证：本会话未执行 Keil/armcc 编译（待本地 F7）。
- 实机验证：待执行
  - DBUS 在线 + VT03 在线：左/右键应有效。
  - DBUS 离线 + VT03 在线：`press_l/press_r` 应保持 0。

## 8. 回滚方式（如何恢复）
- 回滚点：将 `vt03_frame_decode()` 中 `if (toe_is_error(DBUS_TOE))` 条件块恢复为无条件清零两行。
- 回滚后影响：DBUS 鼠标键将再次被 VT03 覆盖，恢复到旧行为（不推荐）。

## 9. 附件/证据
- 代码位置：`application/vt03_link.c`（`mb = d[14];` 后）
- 关联风险项：`docs/risk_log.md`（2026-03-03 新增条目）
