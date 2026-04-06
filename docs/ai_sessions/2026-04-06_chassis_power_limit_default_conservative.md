# 2026-04-06 底盘功率限制默认切回保守版

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-03-24_power_limit_ui.md`
- 数据来源：当前分支代码状态 + 用户要求直接推送当前工作区；无新增板端验收数据
- 关键帧/时间段：无
- 状态：`Blocked - 需数据`
- 差异：本轮未改功率模型公式与反解流程，只把默认编译开关 `POWER_LIMIT_AGGRESSIVE` 从 `1` 切回 `0`，使未额外配置编译宏的构建默认走 V1 保守策略
- 下一步：分别构建 conservative/aggressive 固件，对比 `chassis_power/chassis_power_buffer/effective_limit` 与操控手感后，再决定是否在 Keil 工程里显式固化 `POWER_LIMIT_AGGRESSIVE=1`

## 1. 目标

- 让当前分支的默认编译配置回到 V1 conservative 功率限制策略，避免未验证的 aggressive 默认值直接进入后续固件构建
- 在推送前补齐本次改动的 docs 记录，使仓库状态符合 `docs/00_AI_HANDOFF_RULES.md`

## 2. 改动文件列表

- `application/chassis_power_control.h`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-04-06_chassis_power_limit_default_conservative.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/risk_log.md`

## 3. 关键改动摘要

- `application/chassis_power_control.h`
  - 将 `POWER_LIMIT_AGGRESSIVE` 的默认值从 `1` 改为 `0`
  - 未在 Keil 工程里额外定义该宏时，默认构建会回到 `BUFFER_ENERGY_SETPOINT=50J`
- `docs/ai_sessions/2026-04-06_chassis_power_limit_default_conservative.md`
  - 新增本次会话记录，说明默认模式切回保守版的原因、风险与验证缺口
- `docs/ai_sessions/CURRENT_STATE.md`
  - 补充 2026-04-06 的当前状态说明，标记默认编译路径已回到 V1 conservative
- `docs/risk_log.md`
  - 新增“默认编译宏与现场联调预期不一致”的风险项，提醒后续必须做 conservative/aggressive A/B 验证
- `docs/INDEX.md`
  - 将本次新增会话记录加入索引

## 4. 关键决策与理由

- 决策 1：只切默认编译开关，不改功率模型公式、buffer 红线和电流反解流程
  - 理由：用户当前要求是把现有工作区推上 GitHub，不是继续改功率控制算法；本轮只收口当前默认配置
- 决策 2：默认值回到保守版，但保留通过编译宏切到 aggressive 的入口
  - 理由：头文件已明确推荐用 Keil 编译宏切换模式；把默认值设为 conservative，可以降低“忘记同步工程宏就把激进版直接烧板”的风险
- 备选方案：继续保留 aggressive 为默认值
  - 放弃理由：当前没有新的板端验证证据证明 aggressive 默认值应成为发布基线，直接推送会把未验证策略固化为默认行为

## 5. 实时性影响

- 阻塞变化：无新增阻塞路径，无新增 `osDelay/vTaskDelay`
- CPU 影响：无新增计算路径；仅编译期默认分支切换
- 栈影响：无新增栈占用
- 帧率/周期：
  - 未修改任务创建、优先级和周期
  - 默认编译路径下，`chassis_task.c` 将采用 V1 的 `buffer_pid` 参数 `{2.0, 0.1, 0.0}` 与 `max_out/max_iout = 40/20`

## 6. 风险点

- 若现场当前联调已经按 aggressive 参数完成，直接使用新的默认构建会让底盘提早限功率，动力手感变保守
- 若 Keil 工程和头文件宏未同步，后续人员可能误判“当前固件到底是 conservative 还是 aggressive”

## ⚠ 未验证假设

- 假设：当前分支后续默认发布基线更适合使用 V1 conservative，而不是 V2 aggressive
- 未验证原因：
  - 本轮仅完成代码与文档收口，没有新增板端采集
  - 未对 conservative/aggressive 两套参数做同工况 A/B 对比
- 潜在影响：
  - 若 aggressive 才是更接近目标车手感的版本，切回 conservative 会导致动力响应偏弱
  - 若后续继续混用头文件默认值和工程宏，可能出现“源码看到一套、实际编出来另一套”的认知偏差
- 后续验证计划：
  - 负责人：现场联调人员 / 下轮 AI 会话
  - 方法：分别构建 `POWER_LIMIT_AGGRESSIVE=0/1` 两套固件，记录 `chassis_power`、`chassis_power_buffer`、`effective_limit`、四轮输出电流
  - 触发条件：下一次上板联调或确认发布配置前

## 7. 验证方式与结果

- 串口：未执行
- USB：未执行
- 示波器：未执行
- 上位机：
  - `git diff -- application/chassis_power_control.h`
  - 结果：确认本轮唯一代码改动为 `POWER_LIMIT_AGGRESSIVE 1 -> 0`
- 长跑：未执行

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 将 `application/chassis_power_control.h` 中 `POWER_LIMIT_AGGRESSIVE` 默认值改回 `1`
- 参数回滚点：
  - 若需要继续使用 V2 aggressive，优先在 Keil 工程中显式添加 `POWER_LIMIT_AGGRESSIVE=1`，不要只改头文件默认值
- 运行时保护：
  - 上板前核对当前固件采用的模式，并同步记录到联调说明，避免误烧错版本

## 9. 附件/证据

- 相关历史会话：`docs/ai_sessions/2026-03-24_power_limit_ui.md`
- 风险日志：`docs/risk_log.md`
- 当前状态页：`docs/ai_sessions/CURRENT_STATE.md`
