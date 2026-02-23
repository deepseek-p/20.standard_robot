# 2026-02-23 USB 调试遥测回溯后恢复

## 1. 目标

- 在代码回溯后，恢复 USB CDC 串口参数输出链路（FireWater 固定帧）。
- 保证 USB 任务、CDC 发送保护、gimbal/chassis 快照接口重新可用并可继续用于 VOFA+ 观察。

## 2. 改动文件列表

- `application/usb_task.h`
- `application/usb_task.c`
- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `application/chassis_task.h`
- `application/chassis_task.c`
- `Src/usbd_cdc_if.c`
- `Src/freertos.c`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-23_usb_debug_telemetry_restore_after_rollback.md`

## 3. 关键改动摘要

- `application/usb_task.h`：恢复 USB 调试通道掩码、任务周期、帧周期配置和对外控制接口。
- `application/usb_task.c`：从旧的纯文本状态打印恢复为 FireWater 固定字段 CSV 帧输出，包含序号、丢包计数、事件位和多模块快照数据。
- `application/gimbal_task.h`、`application/gimbal_task.c`：恢复 `gimbal_debug_snapshot_t` 与 `get_gimbal_debug_snapshot(...)`，用于 USB 帧抓取云台控制量。
- `application/chassis_task.h`、`application/chassis_task.c`：恢复 `chassis_debug_snapshot_t` 与 `get_chassis_debug_snapshot(...)`，并恢复 `chassis_relative_angle` 的运行时更新。
- `Src/usbd_cdc_if.c`：恢复 `hUsbDeviceFS.pClassData == NULL` 防护，避免 USB 尚未就绪时空指针访问。
- `Src/freertos.c`：恢复 `USBTask` 栈大小为 `512`，降低大格式化字符串输出导致的栈风险。

## 4. 关键决策与理由

- 仅恢复“USB 串口传参链路”直接依赖代码，不回灌无关控制策略改动，避免引入额外行为变化。
- 保留 FireWater 固定字段协议，避免上位机脚本和曲线映射在恢复后失配。
- CDC 空指针保护和 USBTask 栈恢复与本链路稳定性直接相关，纳入同次恢复。

## 5. 实时性影响（必须写）

- 阻塞变化：USB 发送仍为非阻塞（`USBD_BUSY` 直接计入丢包并返回），未新增阻塞等待。
- CPU 影响：恢复 `snprintf` 固定帧格式化开销，发生在 `USBTask`（5ms 轮询、20ms发帧）内，不进入高优先级控制任务。
- 栈影响：`USBTask` 栈恢复为 `512`，用于承载长格式化路径。
- 帧率/周期：恢复 `USB_DEBUG_TASK_PERIOD_MS=5` 与 `USB_DEBUG_FRAME_PERIOD_MS=20` 的双周期机制。

## 6. 风险点

- 若上位机未按固定字段顺序解析，会出现曲线错位。
- 若 USB 主机端长时间不读数据，`drop` 计数会增长，导致观测有缺帧。
- 恢复后依赖 gimbal/chassis 快照接口，后续若再次回溯其中任一文件会导致链路断开。

## ⚠ 未验证假设

- 假设：当前目标板 USB CDC 枚举行为与恢复前一致，`CDC_Transmit_FS` 在设备就绪后可稳定发送。
- 未验证原因：本次仅完成代码恢复，未连接实机进行 USB 枚举与连续采样测试。
- 潜在影响：若板级电源、线缆或主机驱动状态变化，可能出现全量丢包或无法枚举。
- 后续验证计划：在目标板上运行 10 分钟 VOFA+ 采集，观察 `seq` 单调增长与 `drop` 增速；同时切换遥控与云台模式，确认关键字段更新。

## 7. 验证方式与结果

- 代码级验证：已检查恢复符号与调用链闭合（`usb_task -> get_*_debug_snapshot -> CDC_Transmit_FS`）。
- 配置验证：已确认 `Src/freertos.c` 中 `USBTask` 栈为 `512`。
- 保护验证：已确认 `Src/usbd_cdc_if.c` 包含 `hcdc == NULL` 保护分支。
- 实机验证：未执行（当前会话无硬件连接）。

## 8. 回滚方式（如何恢复）

- 若需回退本次恢复：对上述改动文件执行 `git checkout -- <file>`（或回退到恢复前提交）。
- 若仅回退 USB 输出格式：还原 `application/usb_task.c/h` 到旧文本打印版本。
- 运行时保护：回退后首次上电建议先观察 USB 枚举与任务稳定，再接入上位机连续读取。

## 9. 附件/证据

- 证据类型：`git diff`（本次改动文件已可见）。
- 本次未新增示波器/上位机截图；待实机验证后补充到后续会话记录。
