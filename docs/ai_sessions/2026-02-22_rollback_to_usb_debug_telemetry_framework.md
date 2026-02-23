# 2026-02-22_rollback_to_usb_debug_telemetry_framework

## 0. 根因路径三问（按 00_AI_HANDOFF_RULES 6.1）

1. 当前故障实际执行路径是什么？
- 依据你提供的 USB FireWater 帧（例如 `83203,4158,3477,...`）解码可见：
  - `pitch_mode=1`（列 24）
  - `rc_ch3=0`（列 43）、`mouse_y=0`（列 47）
  - `pitch_gyro_set=1279`（列 38）、`pitch_cur=13732`（列 39）
- 说明故障发生在“无 pitch 输入 + 绝对角控制分支”，对应 `application/gimbal_task.c` 的 pitch 绝对角控制链路。

2. 本次改动命中的路径是什么？
- 按你的回退要求，本次撤销了 2/20~2/22 云台/底盘控制侧追加的防御改动，控制链路回到 USB 框架刚修好时的基线逻辑。
- 同时保留 USB 遥测必需的快照接口（`get_gimbal_debug_snapshot/get_chassis_debug_snapshot`），保证 USB 框架可编译可运行。

3. 如何用数据证明本次改动生效？
- 代码面：`gimbal/chassis` 回到基线控制逻辑；USB 四文件保持 FireWater + CDC 空指针保护 + USBTask 栈 512。
- 运行面：继续输出 52 列 FireWater；重点观测列 `23/24/33/34/37/38/39/43/47`，确认模式/指令链路与回退前一致。

## 1. 目标

- 本次要解决的问题：
  - 按你的指令，将项目回退到 `2026-02-20_usb_debug_telemetry_framework` 的“USB刚修好”状态。
  - 停止继续叠加近期云台/底盘防御性改动。
- 预期可观测结果：
  - USB FireWater 调试能力保持可用。
  - 控制逻辑回到 USB 基线阶段，便于重新定位 pitch 下冲根因。

## 2. 改动文件列表

- `application/usb_task.h`
- `application/usb_task.c`
- `Src/usbd_cdc_if.c`
- `Src/freertos.c`
- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `application/chassis_task.h`
- `application/chassis_task.c`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-02-22_rollback_to_usb_debug_telemetry_framework.md`

## 3. 关键改动摘要

- 控制侧回退：
  - `gimbal/chassis` 控制逻辑回到基线版本，不保留 2/20~2/22 期间追加的防御分支。
- USB 侧保留：
  - 保留 FireWater 固定帧、CDC 空指针保护、USBTask 栈提升。
- 兼容性补齐：
  - 由于 USB 框架依赖调试快照，补齐并保留：
    - `get_gimbal_debug_snapshot(...)`
    - `get_chassis_debug_snapshot(...)`
  - 仅用于遥测取样，不改变控制算法。

## 4. 关键决策与理由

- 决策 1：采用“控制逻辑回退 + USB框架保留”的最小回退。
  - 理由：满足你“回到 USB 刚修好状态”的目标，同时避免把 USB 调试能力一起撤销。
- 决策 2：保留快照接口。
  - 理由：`usb_task` 当前依赖该接口导出 52 列，直接删除会导致编译失败。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增阻塞路径。
- CPU 影响：与 USB 基线一致，主要开销仍在 `usb_task` 的 `snprintf`。
- 栈影响：`USBTask` 维持 `512`。
- 帧率/周期：维持 `USB_DEBUG_TASK_PERIOD_MS=5`、`USB_DEBUG_FRAME_PERIOD_MS=20`。

## 6. 风险点

- 风险 1：pitch 下冲根因尚未闭环，回退后问题可能继续复现。
- 风险 2：当前 `git` 历史没有“2/20 代码快照提交点”，本次采用“语义回退”而非提交点回滚。

## ⚠ 未验证假设

- 假设：当前保留的快照接口与 USB 基线行为一致，不会引入新控制副作用。
- 未验证原因：本次仅完成代码回退与文档同步，未做新的实机长跑。
- 潜在影响：若快照接口与现场分支存在差异，可能影响列值一致性但不影响主控制闭环。
- 后续验证计划：
  - 负责人：@Codex / 现场联调同学
  - 方法：ATTI/GPS 各静止 3 秒 + “轻推 pitch 机构”复现实验，记录 FireWater 指定列。
  - 触发条件：下一轮上电联调。

## 7. 验证方式与结果

- 串口：待你实机确认（本轮未在本机串口复测）。
- USB：代码路径已保留（FireWater + CDC 空指针保护）。
- 示波器：本轮未执行。
- 上位机：VOFA+ 字段协议保持不变（52 列）。
- 长跑：本轮未执行。

## 8. 回滚方式（如何恢复）

- 若要恢复到“回退前”的控制防御版本：
  - 需要按 2/20~2/22 各会话记录重新逐项恢复对应代码分支。
- 若要关闭 USB 调试输出：
  - 将 `USB_DEBUG_OUTPUT_ENABLE` 设为 `0`。

## 9. 附件/证据

- 关键输入数据：本会话中你提供的 ATTI/GPS 静止与手动扰动日志。
- 相关文档：
  - `docs/ai_sessions/2026-02-20_usb_debug_telemetry_framework.md`
  - `docs/risk_log.md`
