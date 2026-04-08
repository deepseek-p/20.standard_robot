# AI 会话记录：Shoot HUST 控制核心替换

日期：`2026-03-07`

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-03-06_shoot_state_machine_align_hust.md`
- 数据来源：MCP 采集 + 板端 CSV（`data/tune_2026-03-06_shoot_hust_acceptance_live_005_ready_static_after_fix.csv`、`...007_empty_single_5shots_hust_finish.csv`）
- 关键结论：
  - `READY_BULLET` 静态持仓已恢复稳定（`trigger_cur` 可回到 0）
  - 空载单发链路仍未闭环通过，恢复段电流和 overshoot 相对基线偏大
- 状态映射：`Failed -> Open`
- 差异：本轮不再继续叠加旧 executor 修补，而是按计划直接切换到 HUST_Infantry_2023 控制核心
- 下一步：完成核心替换并准备重新 Keil 编译/板端验收

## 1. 目标

- 将 `shoot` 控制核心替换为 HUST 方案：
  - 速度环反馈/目标统一为 `rpm`
  - 连发路径跳过位置环，直接速度环驱动
  - 反转路径简化为“退一格”
- 保持外部操作接口不变（遥控/键鼠/VT03 行为入口不改）。

## 2. 改动文件列表

- `application/shoot.h`
- `application/shoot.c`
- `MDK-ARM/standard_robot.uvprojx`
- `docs/02_ARCHITECTURE.md`
- `docs/03_TASK_MAP.md`
- `docs/04_MODULE_MAP.md`
- `docs/INDEX.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/risk_log.md`
- `docs/ai_sessions/2026-03-07_shoot_hust_core_replacement.md`

## 3. 关键改动摘要

- `application/shoot.h`
  - 枚举精简为 5 态：`STOP/READY_FRIC/READY_BULLET/BULLET/CONTINUE_BULLET`
  - 拨弹速度环参数改为 HUST rpm 单位参数组（`TRIGGER_SPD_KP=6.2, KI=3.2`）
  - 删除旧堵转状态字段与 `SHOOT_DONE` 相关遗留字段
  - `TRIGGER_CAN_CURRENT_LIMIT` 对齐为 `10000`
- `application/shoot.c`
  - 删除 `shoot_logic` 依赖，改为内联 HUST 风格模式转换
  - `shoot_feedback_update()` 速度滤波改为直接使用 `speed_rpm`（不再乘 `MOTOR_RPM_TO_SPEED`）
  - `SHOOT_CONTINUE_BULLET` 路径改为速度环直驱（`3500/4500 rpm`）
  - 保留原输入解析段（到 `shoot_control.reverse_req = reverse_req;`）
- `MDK-ARM/standard_robot.uvprojx`
  - 移除 `shoot_logic.c` 编译单元

## 3.1 控制环路改动前三问（规则 6.1）

1. 当前故障执行路径是什么？  
   来自上轮 CSV 证据：问题集中在 `single_fire -> READY_BULLET` 恢复阶段，表现为恢复段 `trigger_cur` 长时间高幅、`|trigger_ecd_fdb-trigger_ecd_set|` 偏大。
2. 本次修改命中哪条路径？  
   直接替换触发执行核心（单发、连发、反转、持仓）并统一 rpm 语义，命中恢复阶段控制律本体，而非继续叠加外围防御宏。
3. 如何证明修改生效？  
   预期在板端复验看到：`READY_BULLET` 静态电流回零、单发后恢复段电流峰值下降、`bullet_cnt` 单次触发仅 `+1`、连发速度档位在 `3500/4500 rpm`。

## 4. 关键决策与理由

- 决策：按计划“整文件替换”而不是增量 patch。  
  理由：旧实现仍混合了多轮 executor 补丁，局部回退风险高，且已存在状态语义漂移；整替更可控、更接近目标基线。
- 决策：暂不改 `usb_task.c`。  
  理由：计划明确禁止修改，且当前遥测字段已覆盖本轮关键观察量（mode/current/speed/set/fdb/setpoint/heat/bullet）。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增 `osDelay/vTaskDelay`、无新增阻塞式调用。
- CPU 影响：`gimbal_task` 1ms 路径仍为位置/速度双 PID 常数级计算，整体复杂度同量级。
- 栈影响：未新增大栈对象，未改任务栈配置。
- 帧率/周期：未修改任务周期和 CAN 发送节拍。

## 6. 风险点

- Keil 工程尚未在当前环境完成编译验证，存在集成层编译风险。
- 机械侧方向/阻力与 HUST 目标机差异可能导致参数仍需板端细调。
- `tests/shoot_logic_test.c` 仍引用已删除模块，若后续纳入自动化构建会失败（当前不在 MDK 工程内）。

## ⚠ 未验证假设

- 假设：当前机器拨弹方向、减速比、编码器符号与 HUST 参数组可直接兼容。  
- 未验证原因：本环境无 Keil/烧录链路，未完成上板动态复验。  
- 潜在影响：可能出现单发方向错误、恢复段电流过高或连发速度偏离目标。  
- 后续验证计划：在 Keil `Rebuild All` 通过后，按计划执行空载 12 项验收，再决定是否进入实弹验证。

## 7. 验证方式与结果

- 代码一致性（本地）：
  - `rg` 检查：`shoot_logic`、`CONTINUE_TRIGGER_SPEED`、`READY_TRIGGER_SPEED`、`SHOOT_DONE` 等旧符号已从工程主路径移除（仅注释文本和 `tests/` 残留）。
  - `uvprojx` 检查：`shoot_logic.c` 文件项已删除。
- Keil 编译：当前环境未发现 `UV4.exe`，未执行 `Rebuild All`（待目标机环境补验）。
- 板端验证：未执行（待烧录）。

## 8. 回滚方式（如何恢复）

- 代码回滚点：回退 `application/shoot.h`、`application/shoot.c` 到本轮前版本，并在 `MDK-ARM/standard_robot.uvprojx` 重新加入 `shoot_logic.c`。
- 参数回滚点：恢复旧 `TRIGGER_SPD_*` 参数组及旧模式枚举语义。
- 运行时保护：回滚/重刷阶段先保持 `arm_enable=0`，确认 `SHOOT_STOP` 下 `given_current=0` 再进入联调。

## 9. 附件/证据

- 计划文档：`docs/plans/2026-03-07-shoot-hust-core-replacement-impl.md`
- 上轮数据：`data/tune_2026-03-06_shoot_hust_acceptance_live_005_ready_static_after_fix.csv`
- 上轮数据：`data/tune_2026-03-06_shoot_hust_acceptance_live_007_empty_single_5shots_hust_finish.csv`
- 相关风险条目：`docs/risk_log.md`（2026-03-07 新增条目）
