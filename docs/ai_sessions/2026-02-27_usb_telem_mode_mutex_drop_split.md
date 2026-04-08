# AI 会话记录：usb_task 遥测三模式互斥 + drop_cnt 双通道拆分
日期：2026-02-27

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-26_wifi_bridge_migrate_to_usart1.md`
- 数据来源：口述问题反馈（WiFi 模式下 USB 未连接导致 drop 飙升，且 USB/WiFi 双发浪费 CPU）
- 关键帧/时间段：无新增采样帧；本轮依据现网现象与代码路径定位
- 状态：`Partial -> In Progress`
- 差异：从“WiFi 模式双发遥测”调整为“三模式互斥输出 + 双通道独立 drop 计数”
- 下一步：在 Keil 中分别验证 `TELEM_OUTPUT_MODE=0/1/2` 三种配置编译与板端行为

## 1. 目标

- 遥测输出切换为互斥模式：`USB / WiFi / 关闭`。
- `drop` 计数拆分为 USB 与 WiFi 独立计数，避免 WiFi 模式下 drop 字段失真。
- 不改变 FireWater 帧格式、字段数量与顺序，不改变命令处理逻辑。

## 2. 改动文件列表

- `application/usb_task.h`
- `application/wifi_bridge.h`
- `application/usb_task.c`
- `docs/02_ARCHITECTURE.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-27_usb_telem_mode_mutex_drop_split.md`

## 3. 关键改动摘要

- `application/usb_task.h`
  - 删除 `USB_DEBUG_OUTPUT_ENABLE`。
  - 新增：
    - `TELEM_MODE_NONE`
    - `TELEM_MODE_USB`
    - `TELEM_MODE_WIFI`
    - `TELEM_OUTPUT_MODE`（默认 USB）
- `application/wifi_bridge.h`
  - 新增 `#include "usb_task.h"`。
  - `WIFI_BRIDGE_ENABLE` 改为自动派生：
    `#define WIFI_BRIDGE_ENABLE (TELEM_OUTPUT_MODE == TELEM_MODE_WIFI)`。
- `application/usb_task.c`
  - 新增 `wifi_debug_drop_cnt`（仅 WiFi 编译启用）。
  - 主循环帧发送 gate 改为：
    `#if (TELEM_OUTPUT_MODE != TELEM_MODE_NONE)`。
  - FireWater 第 2 列 `drop` 改为 WiFi 模式输出 `wifi_debug_drop_cnt`，其它模式输出 `usb_debug_drop_cnt`。
  - 帧过长丢弃计数按模式分流到对应 drop 计数器。
  - 发送逻辑由“双发”改为互斥：
    - USB 模式：仅 CDC
    - WiFi 模式：仅 UART1
    - NONE 模式：不发送

## 4. 关键决策与理由

- 决策 1：`WIFI_BRIDGE_ENABLE` 与 `TELEM_OUTPUT_MODE` 绑定。
  理由：避免多处宏配置不一致导致行为歧义。
- 决策 2：WiFi 模式 drop 字段显示 `wifi_debug_drop_cnt`。
  理由：该字段应反映主输出通道真实丢帧，而非另一路未连接通道状态。
- 决策 3：保留 `MX_USB_DEVICE_Init()` 与 USB 命令链路。
  理由：即使关闭遥测输出，USB CDC 仍承担 `SET/GET/DUMP` 命令通道。

## 5. 实时性影响（必须写）

- 阻塞变化：
  - WiFi 模式仍保留 UART1 阻塞发送（与旧 WiFi 模式一致）。
  - USB 模式不再无意义尝试 UART1 遥测发送。
- CPU 影响：
  - WiFi 模式去除 CDC 遥测发送尝试，降低一条通道的发送开销。
  - USB 模式与旧 `WIFI_BRIDGE_ENABLE=0` 行为一致。
- 栈影响：无新增大对象。
- 帧率/周期：`USB_DEBUG_FRAME_PERIOD_MS` 与任务周期不变。

## 6. 风险点

- 若上位机仍按“旧 WiFi 双发假设”调试，可能误判通道状态。
- `TELEM_MODE_NONE` 下遥测关闭，若未明确告知操作手，可能被误认为系统无数据输出故障。
- 未在本机完成 Keil 编译验证，存在编译器差异风险（armcc 条件编译路径）。

## ⚠ 未验证假设

- 假设：`TELEM_OUTPUT_MODE=0/1/2` 三种配置在 Keil armcc 下均可无告警编译。
  - 未验证原因：当前环境缺少 `UV4/armcc` 命令行工具链，无法本机编译。
  - 潜在影响：可能存在仅在某一模式路径暴露的条件编译告警/错误。
  - 后续验证计划：在 Keil 工程中逐一切换宏并执行 `F7`，记录 warning/error 为 0 的构建日志。
- 假设：WiFi 模式下 `drop` 字段切换为 `wifi_debug_drop_cnt` 后，上位机脚本无需改字段定义。
  - 未验证原因：本轮未进行 MCP/VOFA+ 实机联调。
  - 潜在影响：若脚本额外依赖 drop 语义而非位置，可能出现误读。
  - 后续验证计划：WiFi 模式下断开 USB，仅保留 UART1，检查 drop 增长是否与 UART1 发送失败一致。

## 7. 验证方式与结果

- 串口：未执行实机验证。
- USB：完成代码静态校验；未执行板端联调。
- 示波器：未执行。
- 上位机：未执行。
- 长跑：未执行。
- 编译：
  - 已尝试本机命令行编译验证，未发现 `UV4.exe/uvision.exe/armcc/armlink/fromelf` 工具，故未完成三模式实际编译。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - `usb_task.h` 恢复 `USB_DEBUG_OUTPUT_ENABLE` 宏与旧 gate。
  - `wifi_bridge.h` 恢复 `WIFI_BRIDGE_ENABLE 1` 固定配置。
  - `usb_task.c` 恢复双发路径与单 drop 计数。
- 参数回滚点：
  - 将 `TELEM_OUTPUT_MODE` 设回 `TELEM_MODE_USB` 可最小风险回退到单 USB 输出。
- 运行时保护：
  - 回滚后先检查 USB 命令链路可用，再确认 FireWater 输出与 drop 增长符合预期。

## 9. 附件/证据

- 关联改动：
  - `application/usb_task.h`
  - `application/wifi_bridge.h`
  - `application/usb_task.c`
- 关联风险：
  - `docs/risk_log.md`（2026-02-27 新增 usb/wifi 遥测模式条目）
