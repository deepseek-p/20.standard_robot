# 2026-02-20_usb_debug_telemetry_framework

## 1. 目标

- 本次要解决的问题：
  - `usb_task` 输出改为 FireWater 固定字段格式，供 VOFA+ 长期复用。
  - 修复“仅接 USB 时偶发卡死”问题（LED 固定、蜂鸣器持续）。
- 预期可观测结果：
  - VOFA+ 可以直接按 CSV 数值帧读取。
  - 在 USB 尚未完成配置时不会因为空指针导致 HardFault。

## 2. 改动文件列表

- `application/usb_task.h`
- `application/usb_task.c`
- `Src/usbd_cdc_if.c`
- `Src/freertos.c`
- `docs/03_TASK_MAP.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/2026-02-20_usb_debug_telemetry_framework.md`

## 3. 关键改动摘要

- `application/usb_task.h`
  - 增加可复用调试框架配置：`USB_DEBUG_TASK_PERIOD_MS`、`USB_DEBUG_FRAME_PERIOD_MS`、`USB_DBG_CH_*`。
  - 保留通道掩码接口：`usb_debug_set_channel_mask`、`usb_debug_get_channel_mask`。
- `application/usb_task.c`
  - 输出改为 FireWater 固定字段 CSV：每行纯数字，结尾 `\r\n`。
  - 包含 HEALTH/GIMBAL/CHASSIS/RC/EVENT 五类信息，保留 `drop` 计数。
  - 发送策略保持非阻塞：`CDC_Transmit_FS` busy 时仅累加 `drop`。
  - 将帧缓冲提高到 `1024`，并在格式化溢出时丢弃该帧（不再发送截断帧）。
- `Src/usbd_cdc_if.c`
  - 在 `CDC_Transmit_FS` 中新增 `hUsbDeviceFS.pClassData == NULL` 保护。
  - USB 未配置阶段直接返回 `USBD_FAIL`，避免空指针访问导致 HardFault。
- `Src/freertos.c`
  - `USBTask` 栈由 `128` 提升到 `512`（CMSIS 参数），降低格式化输出引起的栈风险。

## 4. 关键决策与理由

- 决策 1：采用 FireWater 固定字段，不使用自定义 `DBG,k=v`。
  - 理由：VOFA+ 原生支持，接入成本低，后续复现问题效率更高。
- 决策 2：在 CDC 发送底层做空指针保护，而不是只在 `usb_task` 上层判断。
  - 理由：底层保护更稳健，所有调用路径都可受益。
- 备选方案与放弃理由：
  - 方案：仅靠 `usb_task` 控制发送时机。
  - 放弃原因：无法保证未来其他模块调用 `CDC_Transmit_FS` 时同样安全。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增阻塞等待；USB 仍按 `osDelay(5ms)` 调度。
- CPU 影响：`snprintf` 开销增加，但仅发生在 `usb_task`，不在底盘/云台控制环。
- 栈影响：`USBTask` 栈已上调至 `512`，用于覆盖字符串格式化高峰。
- 帧率/周期：FireWater 固定帧周期 `20ms`。

## 6. 风险点

- 风险 1：主机未接收或 USB 忙时会持续丢包，`drop` 可能增长较快。
- 风险 2：快照为跨任务无锁读取，单帧内字段时间戳不完全一致。

## ⚠ 未验证假设

- 假设：`USBTask` 栈 `512` 在“全字段开启 + VOFA+ 长时间接收”下仍有足够裕量。
- 未验证原因：当前会话未执行板级 `uxTaskGetStackHighWaterMark` 长跑采样。
- 潜在影响：若裕量不足，极端情况下仍有触发异常的风险。
- 后续验证计划：
  - 负责人：现场联调同学。
  - 方法：VOFA+ 连续接收 10~20 分钟，同时记录 `drop` 与栈高水位。
  - 触发条件：下一轮实机 USB 联调。

## 7. 验证方式与结果

- 串口：已通过串口助手观察 FireWater 原始帧，52 列格式正确。
- USB：
  - 代码级验证：`CDC_Transmit_FS` 已增加空指针保护。
  - 实机验证：仅接 USB 线条件下，上电重启 10 次，未复现卡死（LED/蜂鸣器异常未出现）。
  - 实机验证：`seq` 列持续单调递增，说明发送任务持续运行。
- 示波器：本轮未执行（USB 问题可通过软件观测闭环判定）。
- 上位机：
  - VOFA+ 已可按 FireWater 直接读取并解析数据列。
- 长跑：
  - 尚未完成“全通道 10 分钟以上 + 栈高水位”专项验证，保留待验证状态。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - `application/usb_task.c/h` 回退到旧版输出。
  - `Src/usbd_cdc_if.c` 移除 `pClassData` 保护（不建议）。
  - `Src/freertos.c` 将 `USBTask` 栈恢复为旧值。
- 参数回滚点：
  - 将 `USB_DEBUG_OUTPUT_ENABLE` 设为 `0` 可快速关闭 USB 调试输出。
- 运行时保护：
  - 回滚后先做“零摇杆 + 零负载”上电检查，再进入动态测试。

## 9. 附件/证据

- 代码差异：`git diff -- application/usb_task.h application/usb_task.c Src/usbd_cdc_if.c Src/freertos.c`
- 风险记录：`docs/risk_log.md`
- 任务与模块映射：`docs/03_TASK_MAP.md`、`docs/04_MODULE_MAP.md`
