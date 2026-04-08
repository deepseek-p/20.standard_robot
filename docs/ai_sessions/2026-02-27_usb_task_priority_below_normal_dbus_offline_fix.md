# AI 会话记录：usb_task 优先级下调修复 WiFi 模式 DBUS 误离线
日期：2026-02-27

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-27_usb_telem_mode_mutex_drop_split.md`
- 数据来源：用户口述实机现象（`TELEM_MODE_WIFI` 间歇蜂鸣告警，`TELEM_MODE_USB` 正常）
- 关键帧/时间段：本轮未新增抓包帧，基于模式对比和代码路径定位
- 状态：`Failed -> Open`
- 差异：上轮处理“遥测输出互斥 + drop 分流”，本轮处理“WiFi 阻塞发送导致的调度挤压”
- 下一步：下调 `USBTask` 优先级后进行 5 分钟 WiFi 实机回归

## 1. 目标

- 修复 `TELEM_MODE_WIFI` 下 DBUS 误判离线引发的蜂鸣告警。
- 仅通过任务优先级调整解决，不改离线阈值、不改发送方式、不改任务栈。

## 2. 改动文件列表

- `Src/freertos.c`
- `docs/03_TASK_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-27_usb_task_priority_below_normal_dbus_offline_fix.md`

## 3. 关键改动摘要

- `Src/freertos.c`
  - `osThreadDef(USBTask, usb_task, osPriorityNormal, 0, 512);`
  - 修改为：
    `osThreadDef(USBTask, usb_task, osPriorityBelowNormal, 0, 512);`
- `docs/03_TASK_MAP.md`
  - 更新 `USBTask` 优先级映射为 `osPriorityBelowNormal`。
  - 新增 2026-02-27 补充说明（WiFi 阻塞发送下的调度影响）。
- `docs/risk_log.md`
  - 新增“WiFi 模式 DBUS 误离线告警”风险项，记录触发条件、缓解措施与验证计划。
- `docs/ai_sessions/CURRENT_STATE.md`
  - 追加本次改动状态与待验证项。
- `docs/INDEX.md`
  - 追加本会话索引入口。

## 4. 关键决策与理由

- 决策 1：仅调整 `USBTask` 优先级，不改 `detect_task` 参数。
  - 理由：问题来自调度竞争，不是离线判定阈值本身错误；改阈值会放宽真实离线检测灵敏度。
- 决策 2：保留 `HAL_UART_Transmit` 阻塞发送不动。
  - 理由：本轮目标是最小改动快速止损，避免同时引入异步发送重构风险。
- 备选方案与放弃理由：
  - 方案 A：缩短/拆分 WiFi 帧。放弃，因为会影响遥测字段完整性与上位机兼容。
  - 方案 B：把 UART1 发送改 DMA 非阻塞。放弃，因为涉及较大链路重构，不符合本轮“仅一处代码修复”约束。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增阻塞，仍为原 `HAL_UART_Transmit` 阻塞路径。
- CPU 影响：`USBTask` 调度优先级下降，`detect_task`（Normal）抢占确定性提升。
- 栈影响：无变化（`USBTask` 栈保持 `512`）。
- 帧率/周期：`USB_DEBUG_TASK_PERIOD_MS` 与遥测帧逻辑不变。

## 6. 风险点

- 若 WiFi 实际阻塞段超过一个时间片且系统高负载，仍可能出现偶发检测延后，需要实机长跑确认。
- `USBTask` 优先级下调后，极端情况下遥测发送抖动可能略有增加，但不影响命令通道功能。

## ⚠ 未验证假设

- 假设：将 `USBTask` 从 `Normal` 降到 `BelowNormal` 后，`detect_task` 周期执行可稳定满足 30ms DBUS 离线窗口。
- 未验证原因：当前会话未执行实机烧录与 5 分钟 WiFi 连续运行测试。
- 潜在影响：若假设不成立，仍会出现间歇 DBUS 误离线蜂鸣，影响遥控可用性与联调效率。
- 后续验证计划：
  - 负责人：@Codex / 操作手协同
  - 方法：`TELEM_MODE_WIFI` 连续运行 5 分钟，观察蜂鸣器与遥控链路；同时记录 `detect_task` 执行周期抖动。
  - 触发条件：烧录本次固件后立即执行，若仍误报则采集在线遥测帧和任务时序证据。

## 7. 验证方式与结果

- 串口：未执行实机验证。
- USB：未执行实机验证。
- 示波器：未执行。
- 上位机：未执行。
- 长跑：未执行（计划 WiFi 模式 5 分钟）。
- 编译：本会话未在本机完成 Keil/armcc 构建验证。

## 8. 回滚方式（如何恢复）

- 代码回滚点：`Src/freertos.c` 将 `USBTask` 优先级改回 `osPriorityNormal`。
- 参数回滚点：无运行时参数变更。
- 运行时保护：回滚后优先在 `TELEM_MODE_USB` 验证遥控稳定，再切回 WiFi 模式复现对比。

## 9. 附件/证据

- 关键代码位置：`Src/freertos.c`（`osThreadDef(USBTask, ...)`）
- 关联风险：`docs/risk_log.md` 新增 2026-02-27 `Src/freertos.c / usb_task / detect_task` 条目
