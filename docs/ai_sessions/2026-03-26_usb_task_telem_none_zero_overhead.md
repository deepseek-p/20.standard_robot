# 2026-03-26 `usb_task` TELEM_MODE_NONE 零开销化

## 上轮验收结论
- 上轮会话：`2026-02-27_usb_telem_mode_mutex_drop_split.md`
- 数据来源：代码静态审查 + 本轮预处理展开验证（`gcc -E`）
- 关键帧/时间段：N/A（本轮无板端采集）
- 状态：`In Progress`
- 差异：上轮 `TELEM_MODE_NONE` 仅停止发帧，本轮改为任务入口立即挂起，并裁掉静态遥测/命令资源
- 下一步：在 Keil 中完成 `TELEM_OUTPUT_MODE=0/1/2` 三模式编译与 `.map` 符号核对

## 1. 目标
- 修复 `TELEM_MODE_NONE` 下 USB 任务仍初始化并轮询的问题。
- 达成目标：NONE 模式任务零 CPU 开销、无 USB/WiFi 初始化、裁掉无用静态内存；非 NONE 模式行为不变。

## 2. 改动文件列表
- `application/usb_task.c`
- `application/usb_task.h`
- `docs/02_ARCHITECTURE.md`
- `docs/03_TASK_MAP.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-03-26_usb_task_telem_none_zero_overhead.md`

## 3. 关键改动摘要
- `application/usb_task.c`
  - `usb_task()` 增加 `#if (TELEM_OUTPUT_MODE == TELEM_MODE_NONE)` 分支：任务启动后 `vTaskSuspend(NULL)`，并进入 `osDelay(osWaitForever)` 永久等待。
  - 非 NONE 分支保留原行为，并移除 `while` 内冗余 `#if (TELEM_OUTPUT_MODE != TELEM_MODE_NONE)` 包裹。
  - 将静态变量、PID 枚举、`cmd_reply_fn`、静态函数声明、`usb_debug_set/get_channel_mask()` 以及全部 `static` 函数实现统一包进 `#if (TELEM_OUTPUT_MODE != TELEM_MODE_NONE)`。
- `application/usb_task.h`
  - 更新 `TELEM_MODE_NONE` 注释为“关闭遥测+命令，任务永久挂起，零开销”。
  - `usb_debug_set/get_channel_mask` 声明改为仅在非 NONE 模式导出。

## 4. 关键决策与理由
- 决策：保留 `usb_task` 创建点不变，仅在任务入口分支挂起。
  - 理由：满足“禁止改 `freertos.c` 任务创建”的边界，同时实现 NONE 模式零调度开销。
- 决策：通过条件编译裁剪静态数据与静态函数，而不删除实现。
  - 理由：满足“函数不删除，只用 `#if` 包裹”的要求，并直接减少 `.bss/.data` 占用。

## 5. 实时性影响（必须写）
- 阻塞变化：
  - `TELEM_MODE_NONE`：任务入口立即 `vTaskSuspend(NULL)`，不再执行 `osDelay(5)` 周期轮询。
  - 非 NONE：保持原 `osDelay(USB_DEBUG_TASK_PERIOD_MS)` 节拍不变。
- CPU 影响：
  - NONE 模式移除 USB 命令轮询与遥测格式化路径，USBTask 调度负载为 0。
- 栈影响：
  - 未改任务栈大小；但 NONE 模式不再进入大栈占用的格式化/命令解析路径。
- 帧率/周期：
  - 非 NONE 模式维持原 `20ms` 帧周期与 `5ms` 任务周期。

## 6. 风险点
- 当前环境无法执行 Keil 工程编译，尚未拿到 `0 warning / 0 error` 的目标证据。
- 当前环境无法生成并核对 `.map`，尚未用链接产物证明 `usb_buf` 等符号在 NONE 模式从 `.bss` 消失。
- `TELEM_MODE_NONE` 下命令通道同步关闭，符合本次需求，但需要用户确认现场流程不再依赖 USB 命令写参。

## ⚠ 未验证假设
- 假设内容：Keil (`armcc/armclang`) 在 `TELEM_OUTPUT_MODE=0/1/2` 下均可无告警编译通过，且 NONE 模式链接产物不包含 `usb_buf/error_list_usb_local/usb_debug_seq/usb_debug_drop_cnt` 等符号。
- 未验证原因：本地执行环境缺少 Keil/ARM 编译工具链与板端构建链路。
- 潜在影响：若工具链对条件编译路径处理存在差异，可能出现告警、链接差异或符号残留。
- 后续验证计划：
  - 负责人：@Codex（或当前板端编译负责人）
  - 方法：Keil 分别编译 `TELEM_OUTPUT_MODE=0/1/2`，检查 Error/Warning 计数与 `*.map`。
  - 触发条件：进入下一轮联调前必须完成。

## 7. 验证方式与结果
- 串口：未执行（本轮无板端连接）。
- USB：未执行（本轮无板端连接）。
- 示波器：未执行。
- 上位机：
  - 执行 `gcc -E` 预处理展开验证三模式：
    - `TELEM_OUTPUT_MODE=0`：`out_usb_task_none.i`
    - `TELEM_OUTPUT_MODE=1`：`out_usb_task_usb.i`
    - `TELEM_OUTPUT_MODE=2`：`out_usb_task_wifi.i`（附加 `-DCURRENT_UART_MODE=0` 规避竞赛模式互斥保护）
  - 结果：
    - NONE 模式展开中存在 `vTaskSuspend(NULL)`，且不存在 `MX_USB_DEVICE_Init(); / usb_cmd_process(); / wifi_cmd_process();` 调用。
    - NONE 模式展开中未检索到 `usb_buf/error_list_usb_local/usb_debug_seq/usb_debug_drop_cnt/usb_debug_channel_mask` 符号定义。
    - USB/WIFI 模式展开均保留 `MX_USB_DEVICE_Init()` 与命令处理路径。
- 长跑：未执行（待板端）。

## 8. 回滚方式（如何恢复）
- 代码回滚点：恢复 `application/usb_task.c` 与 `application/usb_task.h` 到本次改动前版本。
- 参数回滚点：无需参数回滚（仅编译期条件路径改动）。
- 运行时保护：若联调异常，临时将 `TELEM_OUTPUT_MODE` 固定为 `TELEM_MODE_USB` 回到旧运行路径。

## 9. 附件/证据
- 预处理产物：
  - `out_usb_task_none.i`
  - `out_usb_task_usb.i`
  - `out_usb_task_wifi.i`
- 相关风险条目：`docs/risk_log.md`（2026-03-26 新增 usb_task 条目）
