# 2026-02-24 usb cdc online pid tuning

## 1. 目标

- 在现有 USB FireWater 遥测基础上，新增 RX 文本命令解析，实现运行时在线读写底盘/云台 PID。
- 保持中断回调轻量：`CDC_Receive_FS` 只入队，不做解析。
- 保持现有遥测逻辑不变，命令回复与遥测共用 CDC，忙时丢弃回复不阻塞控制链路。

## 2. 改动文件列表

- `Src/usbd_cdc_if.c`
- `Inc/usbd_cdc_if.h`
- `application/usb_task.c`
- `application/gimbal_task.h`
- `application/gimbal_task.c`
- `application/chassis_task.h`
- `application/chassis_task.c`
- `docs/ai_sessions/2026-02-24_usb_cdc_online_pid_tuning.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/04_MODULE_MAP.md`
- `docs/INDEX.md`

## 3. 关键改动摘要

- `Src/usbd_cdc_if.c` / `Inc/usbd_cdc_if.h`
- 新增 256B RX 环形缓冲（`usb_rx_buf` + `head/tail`）。
- 在 `CDC_Receive_FS` 中按字节入队，溢出丢弃，保持回调快速返回。
- 导出 `usb_rx_available()` 与 `usb_rx_read_byte()` 给任务上下文读取。

- `application/usb_task.c`
- 在 `usb_task` 主循环中，`osDelay` 前新增 `usb_cmd_process()`。
- 新增命令解析：
- `SET <target> <param> <value>`
- `GET <target> <param>`
- `DUMP`
- 新增 target 映射：
- `pitch_speed/pitch_angle/pitch_encode/yaw_speed/yaw_angle/yaw_encode/chassis_follow/chassis_wheel`
- 对 `chassis_wheel` 的 `SET` 同步写 4 个轮速 PID；`GET`/`DUMP` 读第 1 个轮 PID。
- 参数范围限制：
- `Kp/Ki/Kd: [0, 50000]`
- `max_out/max_iout: [0, 30000]`
- 回复格式：
- 成功：`OK ...`
- 失败：`ERR ...`
- 导出：`DUMP ...` + `DUMP END`

- `application/gimbal_task.h` / `application/gimbal_task.c`
- 新增可写控制对象访问函数：`get_gimbal_control_point()`。

- `application/chassis_task.h` / `application/chassis_task.c`
- 新增可写控制对象访问函数：`get_chassis_control_point()`。

## 4. 关键决策与理由

- 决策 1：命令解析放在 `usb_task`，不放在 USB 回调。
- 理由：回调处于 USB 中断上下文，必须最小化执行时间，避免影响实时性。

- 决策 2：与遥测共用 `CDC_Transmit_FS`，忙时丢弃命令回复。
- 理由：符合现有非阻塞传输策略，避免命令通道反向阻塞控制任务。

- 决策 3：通过新增可写 getter 暴露控制对象，不改现有 const getter。
- 理由：保持原有只读链路兼容，在线调参路径与现有读快照路径解耦。

## 5. 实时性影响（必须写）

- 阻塞变化：无新增阻塞调用；命令处理仅在 `usb_task` 任务上下文执行。
- CPU 影响：`usb_task` 增加字符串解析与 `snprintf`，频率受 `USB_DEBUG_TASK_PERIOD_MS=5` 限制。
- 栈影响：`usb_task` 新增命令缓冲与格式化局部变量，未修改任务栈配置。
- 帧率/周期：遥测周期与控制任务周期均未改动。

## 6. 风险点

- 风险 1：在线调参可写入不合适参数，造成控制不稳定。
- 风险 2：命令回复与遥测共用 CDC，链路拥塞时回复可能被丢弃，影响交互体验。
- 风险 3：当前命令无鉴权，默认任何接入 CDC 的上位机都可改参数。

## ⚠ 未验证假设

- 假设：USB CDC 链路在实机上可稳定同时承载 20ms 遥测和调参命令交互。
- 未验证原因：本轮仅完成代码落地与静态走查，未做实机连机压测。
- 潜在影响：若拥塞严重，`OK/ERR/DUMP` 反馈会丢失，调参效率下降。
- 后续验证计划：由 `@Codex` 在实机执行 4 组测试（单条 `SET/GET`、连续 `SET`、整表 `DUMP`、边遥测边调参），记录回复丢失率与 `drop` 增长。

## 7. 验证方式与结果

- 代码检查：
- 已核对 target 到 PID 实体映射与字段大小写差异（`pid_type_def` 的 `Kp/Ki/Kd` 与 `gimbal_PID_t` 的 `kp/ki/kd`）。
- 已核对 `chassis_wheel` 写入四轮 PID。
- 已核对中断回调只做入队，不做解析。

- 编译/实机：
- 本轮未执行 Keil 编译与上板联调（需你本地编译烧录后验证）。

### 7.1 上轮验收结论（本轮先做）

- 上轮会话：`2026-02-24_pitch_speed_loop_limit_cycle_tune.md`
- 数据来源：用户口述（“MID 模式已基本调好”），本轮未新增 VOFA+ 文件。
- 关键帧/时间段：无新增帧号。
- 状态：`Blocked`（映射 `Blocked - 需数据`，仅对“新增数据验收”这一项）。
- 差异：存在口述结论，但无本轮新增量化数据回填。
- 下一步：继续当前 USB 在线调参功能验收，同时补 VOFA+ 量化数据到 `data/`。

## 8. 回滚方式（如何恢复）

- 回滚点 1：撤销 `usb_task.c` 中命令解析调用与相关静态函数。
- 回滚点 2：撤销 `usbd_cdc_if.*` 中 RX 环形缓冲和导出函数。
- 回滚点 3：撤销 `get_gimbal_control_point()` 与 `get_chassis_control_point()` 新增接口。
- 运行保护：回滚前保持现有遥测输出链路不动，优先确认 `CDC_Transmit_FS` 正常。

## 9. 附件/证据

- 代码差异：`git diff`（本轮改动文件）。
- 关联风险：`docs/risk_log.md` 新增 USB 在线调参风险条目。
