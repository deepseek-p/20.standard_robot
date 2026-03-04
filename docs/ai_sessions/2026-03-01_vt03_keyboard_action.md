# AI 会话记录：VT03 键鼠动作集中管理（keyboard_action）
日期：2026-03-01

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-02-28_vt03_uart_mode_switching.md`
- 数据来源：代码静态核对（本轮未新增实机采样）
- 关键帧/时间段：无（未执行板端采集）
- 状态：`In Progress -> In Progress`
- 差异：上轮完成 VT03 链路接入与 UART 三模式切换；本轮补齐 VT03/键盘/VT13 物理键统一动作层并接入云台/底盘/发射。
- 下一步：Keil 编译 + VT13 实机键位矩阵验收（含长按/短按、断链回退）。

## 1. 目标

- 按 `docs/plans/2026-03-01-vt03-keyboard-action.md` 落地集中式键位管理。
- 新增 `keyboard_action` 模块统一处理键盘边沿、VT13 物理键长短按和动作命令输出。
- 保持 DBUS 原链路可用，同时在 VT03 在线时提供等价控制语义。

## 2. 改动文件列表

- `application/keyboard_action.h`（新增）
- `application/keyboard_action.c`（新增）
- `application/vt03_link.c`
- `application/gimbal_task.c`
- `application/gimbal_behaviour.c`
- `application/chassis_behaviour.c`
- `application/chassis_task.c`
- `application/shoot.c`
- `MDK-ARM/standard_robot.uvprojx`
- `docs/02_ARCHITECTURE.md`
- `docs/03_TASK_MAP.md`
- `docs/04_MODULE_MAP.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`
- `docs/ai_sessions/2026-03-01_vt03_keyboard_action.md`

## 3. 关键改动摘要

- `application/keyboard_action.h/.c`
  - 新增集中式动作层：统一键盘 `rising/falling/held` 检测。
  - 支持 VT13 `fn_1/fn_2/pause` 长短按（300ms 阈值）和 `trigger/dial` 映射。
  - 输出统一命令结构 `keyboard_cmd_t`，供上层模块读取。

- `application/vt03_link.c`
  - 将 `mode_sw` 映射为 DBUS 兼容开关值：
    `0->DOWN`、`1->MID`、`2->UP`、`3->DOWN(防御兜底)`。

- `application/gimbal_task.c`
  - 初始化阶段调用 `keyboard_action_init()`。
  - 1ms 循环开头调用 `keyboard_action_update()`。
  - 云台失联判定改为 `DBUS` 与 `VT03` 双源都离线才断电流。

- `application/gimbal_behaviour.c`
  - 初始化退出判定改为 `DBUS` 或 `VT03` 任一在线即可继续。
  - 失联保护改为双源离线判定。
  - 接入 `Z` 键无力 toggle（`get_kb_zero_force()`）。
  - 清理 `#if !GIMBAL_YAW_CONTINUOUS_TURN` 下的 180° 掉头死代码块。

- `application/chassis_behaviour.c`
  - 接入 `get_kb_chassis_mode()`：`CTRL` toggle 可将底盘覆盖到 `CHASSIS_HUST_SELF_PROTECT`。
  - 移除原 `SWING_KEY` 摇摆逻辑，`FOLLOW_GIMBAL_YAW` 下 `angle_set` 固定为 `0.0f`。

- `application/chassis_task.c`
  - 启动等待与离线零电流判定改为 `DBUS && VT03` 双离线触发。

- `application/shoot.c`
  - 接入 `keyboard_action` 输出：
    - `shoot_toggle`（Q / fn_1 短按）
    - `high_freq_toggle`（C / fn_1 长按）
    - `burst_toggle`（R / fn_2 短按）
    - `fric_speed_adj`（F/SHIFT+F + VT03 拨轮）
    - `reverse_trigger`（G / fn_2 长按）
    - `vt03_trigger`（等效鼠标左键）
  - VT03 在线时，发射键盘辅助功能允许通过 `s[0] != DOWN` 激活（不依赖 `s[1]`）。

- `MDK-ARM/standard_robot.uvprojx`
  - 新增 `..\application\keyboard_action.c` 到工程文件列表。

## 4. 关键决策与理由

- 决策 1：新增 `keyboard_action` 集中层，而不是在 `shoot/chassis/gimbal` 分散改键。
  - 理由：减少重复边沿检测与状态机分叉，便于后续继续扩展 VT13 键位。

- 决策 2：将失联保护改为 `DBUS` 与 `VT03` 双源容错。
  - 理由：VT03 作为有效控制源时，不应因 DBUS 离线误触发全局零电流。

- 决策 3：保留 DBUS 原行为路径并做最小侵入改动。
  - 理由：降低回归风险，确保断开 VT03 后仍可直接回退到传统遥控流程。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增 `osDelay/vTaskDelay` 或阻塞式等待。
- CPU 影响：`gimbal_task` 1ms 周期新增常数级按键边沿计算与少量分支判断。
- 栈影响：新增 `keyboard_action.c` 文件级静态状态，任务栈配置未改。
- 帧率/周期：任务创建顺序、优先级和周期未修改。

## 6. 风险点

- `keyboard_action` 的状态变量（`kb_chassis_mode/kb_zero_force/kb_cap_on`）在 DBUS/VT03 源切换时不会自动复位，可能出现状态延续。
- `fn_2` 长按映射为单周期 `reverse_trigger` 脉冲，实际堵转反转手感需实机确认。
- VT03 `mode_sw` 映射已按协议文档固化，仍需与实物拨杆方向做最终一致性核对。

## ⚠ 未验证假设

- 假设：VT13 `fn_1/fn_2/pause` 的时序与 1ms 任务采样配合可稳定区分 300ms 长短按。
- 未验证原因：本轮仅完成代码落地，未接入 VT13 实机。
- 潜在影响：长短按误判会引发发射状态误切换或堵转反转误触发。
- 后续验证计划：在 VT13 实机下执行短按/长按各 20 次统计，记录误判率与触发延迟。

- 假设：VT03 在线时 `s[0]!=DOWN` 作为发射辅助功能激活条件符合现场操作习惯。
- 未验证原因：尚未进行完整模式挡位联调。
- 潜在影响：可能出现“挡位与发射辅助功能不一致”的操作体验问题。
- 后续验证计划：按 `C/N/S` 三挡逐项验证 `Q/C/R/F/G/trigger` 动作触发矩阵。

## 7. 验证方式与结果

- 串口：未执行板端串口日志采集。
- USB：未执行 VOFA+/FireWater 实机采集。
- 示波器：未执行。
- 上位机：完成代码静态核对（关键条件分支、头文件依赖、Keil 工程条目）。
- 长跑：未执行。
- 编译：本会话未直接调用 Keil/armcc 编译（待用户本地 F7 验证）。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - 删除 `application/keyboard_action.*`；
  - 回退 `gimbal/chassis/shoot/vt03_link` 相关调用与条件分支；
  - 从 `uvprojx` 移除 `keyboard_action.c` 条目。
- 参数回滚点：
  - 关闭 VT03 相关模式并回到 DBUS 纯链路（`CURRENT_UART_MODE` 维持 DBUS/WiFi 方案）。
- 运行时保护：
  - 回滚后先验证 `DBUS_TOE/VT03_TOE` 在线状态，再使能发射链路。

## 9. 附件/证据

- 实施计划：`docs/plans/2026-03-01-vt03-keyboard-action.md`
- 相关风险条目：`docs/risk_log.md`（2026-03-01 新增 VT03 键位动作风险条目）

## v2: referee_usart_task 编译告警清理（#177-D）

### 改动摘要

- 文件：`application/referee_usart_task.c`
- 将 `referee_unpack_fifo_data()` 的前置声明与函数定义都包进 `#if USART6_REFEREE`。
- 结果：在 `UART_MODE_DEBUG_VT03` 下，裁判解包函数不再参与编译，避免“declared but never referenced”告警。

### 决策理由

- 当前 VT03 调试模式下 `referee_unpack_fifo_data()` 没有调用路径，保持其为 `static` 会触发 armcc #177-D。
- 采用条件编译而不是删除函数：保留裁判模式原逻辑，且不影响 `UART_MODE_DEBUG_WIFI` / `UART_MODE_COMPETITION` 的 referee 解包路径。

### ⚠ 未验证假设（v2）

- 假设：`USART6_REFEREE=1` 模式下函数声明/定义路径完整，裁判链路行为与改动前一致。
- 未验证原因：本轮只针对告警做静态修复，未做板端裁判协议回归。
- 潜在影响：若后续宏配置异常，可能出现 referee 解包函数不可见的编译问题。
- 后续验证计划：在 `UART_MODE_DEBUG_WIFI` 与 `UART_MODE_COMPETITION` 下各执行一次 Keil 全量编译并做裁判链路上电检查。

## v3: VT03在线检测/告警容错/USART6 ORE修复/遥测列扩展（你指定的5项）

### 目标
- 完成你指定的 5 条改动：`detect_task.c`、`test_task.c`、`referee_usart_task.c`、`usb_task.c`、`frame_parser.py`。

### 改动文件列表
- `application/detect_task.c`
- `application/test_task.c`
- `application/referee_usart_task.c`
- `application/usb_task.c`
- `D:\tools\stm32-telemetry-mcp\frame_parser.py`

### 关键改动摘要
- `application/detect_task.c`
  - 注册 `VT03_TOE` 的 `data_is_error_fun = vt03_data_is_error`（仅在 `VT03_ENABLE` 下）。
- `application/test_task.c`
  - 蜂鸣器 DBUS 告警改为：`DBUS_TOE && VT03_TOE` 同时离线才报；任一链路在线不报。
- `application/referee_usart_task.c`
  - VT03 模式下，启用 `UART_IT_RXNE` 前先清 `ORE`。
  - `USART6_IRQHandler` 的 VT03 分支增加 `ORE` 处理，避免过载后中断卡死。
- `application/usb_task.c`
  - FireWater 新增第 63 列 `vt03_toe`，总列数从 63 扩为 64。
- `D:\tools\stm32-telemetry-mcp\frame_parser.py`
  - `COLUMNS` 新增 `63: ("vt03_toe", "health", False)`。
  - `TOTAL_COLUMNS` 从 `63` 改为 `64`。

### 关键决策与理由
- 将 `VT03` 在线状态直接放入健康列，避免“链路坏了但不可观测”的调试盲区。
- `DBUS` 告警改为双源离线判定，保证 VT03 控制有效时不触发误蜂鸣。
- `ORE` 在 IRQ 内显式清理，避免 USART6 在高负载下进入不可恢复接收状态。

### 实时性影响
- `detect_task`/`test_task` 增量仅为常量级条件分支。
- `USART6_IRQHandler` 新增一次 `ORE` 分支判断，时延变化可忽略。
- `usb_task` 仅新增 1 列格式化输出，帧长度轻微增加。

### 风险点
- `usb_task.c` 的 `snprintf` 列数与上位机解析器必须始终一致（已同步 parser 到 64 列）。
- 若现场串口噪声极高，`ORE` 频繁清理会导致少量字节丢失，但优先保证中断链路不锁死。

### ⚠ 未验证假设（v3）
- 假设内容：新增 `vt03_toe` 列后，VOFA+/MCP 端已全部切换到 64 列解析配置。
- 未验证原因：本轮未做板端长跑采集，仅完成代码和解析脚本同步。
- 潜在影响：若仍按 63 列解析，后续列会错位，导致调参与诊断误判。
- 后续验证计划：上板后连续采集 5 分钟，检查第 63 列 `vt03_toe` 与断链动作一致，且无列错位。

### 验证方式与结果
- 代码级验证：已完成改动点逐项 diff 核对。
- 编译验证：本轮未调用 Keil/armcc（待你本地 F7）。

### 回滚方式
- 回滚 `application/detect_task.c/test_task.c/referee_usart_task.c/usb_task.c` 到本次改动前版本。
- 外部工具回滚 `D:\tools\stm32-telemetry-mcp\frame_parser.py` 的 `TOTAL_COLUMNS=63` 并删除 `vt03_toe` 列。

## v4: VT03 CRC-16 改为 CCITT-FALSE（仅 vt03_link.c）

### 目标
- 修复 VT03 帧 CRC 校验算法与协议不一致导致的“收帧即丢弃”问题。

### 改动文件列表
- `application/vt03_link.c`

### 关键改动摘要
- 将 `crc16_tab[256]` 替换为 CRC-16/CCITT-FALSE（poly=0x1021，非反转）查表。
- 将 `vt03_crc16()` 替换为非反转计算路径：
  - `crc = (crc << 8) ^ table[((crc >> 8) ^ byte) & 0xFF]`

### 关键决策与理由
- VT03 协议要求 CCITT-FALSE，原实现属于反转族（MCRF4XX 方向），会导致同一帧计算值与发送端不一致。
- 仅改 CRC 相关两处，避免引入帧解码与控制映射的额外回归风险。

### 实时性影响
- 算法仍为 O(n) 查表法，单字节处理开销与原实现同量级。
- 不新增阻塞、任务、延时或中断路径变更。

### 风险点
- 若发送端实际并非 CCITT-FALSE，而是反转 CRC 变种，则仍会校验失败。
- CRC 正确后，历史被 CRC 屏蔽的协议字段问题（若存在）会暴露到上层逻辑。

### ⚠ 未验证假设（v4）
- 假设内容：VT03 发送端 CRC 参数与文档一致（CCITT-FALSE）。
- 未验证原因：本轮仅完成代码侧算法修复，未接入实机逐帧抓包对照。
- 潜在影响：若设备固件版本使用不同 CRC 变种，链路依旧无法建立。
- 后续验证计划：
  1) 实机抓取原始帧，离线复算 CRC 与尾部字段逐帧比对；
  2) 观察 `VT03_TOE` 在线恢复与 `rc_ctrl` 字段更新；
  3) 连续运行 5 分钟确认无“全帧 CRC fail”。

### 验证方式与结果
- 本地算法校验：
  - 空数据 CRC = `0xFFFF`
  - `"123456789"` CRC = `0x29B1`
- 编译验证：未执行 Keil/armcc（待本地 F7）。

### 回滚方式
- 回滚 `application/vt03_link.c` 中 `crc16_tab` 与 `vt03_crc16` 到修复前版本。

## v5: fn_1 异常联动修复 + VT13 键位可观测性增强（待实机）

### 改动摘要

- `application/shoot.c`
  - `SHOOT_READY_BULLET` 分支取消自动 `READY_TRIGGER_SPEED + trigger_motor_turn_back()`，改为纯待发静止（不主动转拨轮）。
  - 新增 `shoot_aux_enable` 判定：`s1=MID` 或（`VT03` 在线且 `s0!=DOWN`）才允许键盘动作（`shoot_toggle/burst/high_freq/fric_adj`）生效。
  - 新增 `fire_cmd_edge` 并允许从 `SHOOT_READY_BULLET` 直接进入 `SHOOT_BULLET`，避免仅依赖按键开关 `SWITCH_TRIGGER_ON` 才能触发发射。
- `application/usb_task.c`
  - `event_bits` 扩展 `bit8..bit20`，用于 VT13/keyboard_action 观测：
    - `bit8`: vt03_online
    - `bit9..12`: `fn1/fn2/trigger/pause` 当前状态
    - `bit13..17`: `shoot_toggle/high_freq/burst/reverse/vt03_trigger` 命令脉冲
    - `bit18..19`: 拨轮 `dial` 方向（`ch4>200` / `<-200`）
    - `bit20`: `s0!=DOWN`（VT03 发射辅助门控开）
  - FireWater 第 63 列显式输出 `vt03_toe`（与注释一致）。

### 决策理由

- 你反馈“短按 `fn_1` 不仅摩擦轮转、拨轮也转”，与代码中 `SHOOT_READY_BULLET` 自动拨轮逻辑一致；先去掉该自动动作，避免未下发开火命令时误动作。
- 你反馈 `trigger` 无响应，现有状态机仅在 `SHOOT_READY` 响应开火边沿；加入 `READY_BULLET -> BULLET` 的直接边沿通道，降低对机械开关状态的耦合。
- 你计划用 STM32 遥测排查，因此在不改列数协议的前提下，把关键原始键位和命令脉冲压到 `event_bits` 高位，便于一次性定位是“输入没到”还是“动作没执行”。

### 实时性影响

- `shoot_set_mode` 与 `usb_task` 仅新增常量级分支与位运算，无阻塞调用。
- 任务周期、优先级、栈大小未改。

### 风险点

- `READY_BULLET` 去自动拨轮后，部分原本依赖预拨动作的机械工况可能需要重新评估首发手感。
- 允许 `READY_BULLET` 直接响应开火边沿后，若机械限位开关异常，需重点关注拨轮堵转恢复路径是否稳定。

### ⚠ 未验证假设（v5）

- 假设内容：`VT13 trigger` 到 `event_bits bit11/17` 的映射与发射触发路径一致，可在实机稳定复现。
- 未验证原因：本轮仅完成代码侧修复与观测通道扩展，尚未进行你现场遥测回放。
- 潜在影响：若映射仍不一致，会出现“event_bits 变了但 shoot_mode 不变”或反向情况，导致误判根因。
- 后续验证计划：
  1) 记录 `shoot_mode(47)`、`trigger_spd_set(58)`、`trigger_cur(54)`、`event_bits(44)`、`rc_s0(39)`、`vt03_toe(63)`；
  2) 分别执行 `fn_1短/长按`、`fn_2短/长按`、`trigger`、`dial`；
  3) 对照 bit9~20 与 `shoot_mode` 变化是否一致。

### 验证方式与结果

- 代码路径验证：已按你描述现象逐条对照并修改对应执行路径。
- 编译验证：未执行 Keil/armcc（待你本地 F7）。

### 回滚方式

- 回滚文件：`application/shoot.c`、`application/usb_task.c` 到 v5 前版本。
- 若仅需回退行为不回退观测：保留 `usb_task` 观测位，单独回退 `shoot.c` 的 `READY_BULLET` 与 `fire_cmd_edge` 相关改动。

## v6: MCP 端 event_bits 虚拟通道补齐（tools/stm32-telemetry-mcp）

### 改动摘要

- `D:\tools\stm32-telemetry-mcp\frame_parser.py`
  - 新增 `EVENT_BIT_FIELDS`（`bit0..20`）到语义名称映射。
  - 新增 `decode_event_bits(event_bits)`，输出 `evt_*` 虚拟通道（0/1）。
  - 将 `evt_*` 虚拟通道加入 `NAME_TO_INDEX`，使 MCP 的 `channels` 参数校验可直接通过。
- `D:\tools\stm32-telemetry-mcp\serial_manager.py`
  - `parse_line()` 在解析到 `event_bits` 后自动展开 `evt_*` 虚拟通道并写入帧字典。
  - 保持原 64 列 CSV 兼容，不改列数、不改原字段名。

### 决策理由

- 你反馈“mcp 端还没改”是准确的：此前只能看到 `event_bits` 原始整数，不便快速定位 `fn_1/fn_2/trigger/dial` 是否到达控制链。
- 通过在 parser 层展开虚拟通道，可直接用 `capture_data(channels=\"evt_fn1_cur,evt_cmd_shoot_toggle,...\")` 做位级验证，减少人工位运算。

### ⚠ 未验证假设（v6）

- 假设内容：现场运行中的 MCP server 会在重启后加载到新的 parser 文件，且不会被旧 `.pyc` 缓存污染。
- 未验证原因：本轮仅做本地脚本级校验，未连接你现场 STM32 做在线 MCP 回归。
- 潜在影响：若服务未重启或路径指向旧版本，`channels` 仍会报 unknown 或看不到 `evt_*` 字段。
- 后续验证计划：
  1) 重启 `stm32-telemetry-mcp` 服务进程；
  2) 调用 `capture_data(..., channels=\"event_bits,evt_fn1_cur,evt_trigger_cur,evt_cmd_vt03_trigger,evt_dial_pos\")`；
  3) 实按键核对虚拟通道是否按预期翻转。

## v7: VT13 发射链路遥测回填（项目1~8）与合并修改方案（待实施）

### 上轮验收结论（规则 7 回填）

- 上轮会话：`docs/ai_sessions/2026-03-01_vt03_keyboard_action.md`（v5/v6）
- 数据来源：`MCP capture（CSV 证据）`
- 关键时间段：`2026-03-01 23:00~23:19`
- 状态：`In Progress -> Open（发射调速边界未满足）`
- 差异：
  - v5/v6 已把“按键可观测性”和“MCP 位级观测”打通。
  - 本轮实机遥测确认：输入层大体可见，但发射执行链仍存在“调速跑飞导致状态机卡住”的问题。
- 下一步：先按“合并修改方案”修复 `dial` 调速链路与 `trigger` 触发保持，再做项目1~8回归。

### 已做过的具体方案（已落地代码）

1. `application/shoot.c`
   - `READY_BULLET` 改为静止待发（移除自动拨轮预转）。
   - 允许 `READY_BULLET` 直接响应开火边沿进入发射。
   - 键盘发射辅助门控统一为 `s1=MID` 或 `VT03在线且s0!=DOWN`。
2. `application/usb_task.c`
   - `event_bits` 扩展到 `bit8..bit20`，增加 VT13 原始键态与命令脉冲可观测性。
   - FireWater 固定帧显式输出 `vt03_toe`（第 63 列，64 列总长）。
3. `D:\tools\stm32-telemetry-mcp\frame_parser.py` + `serial_manager.py`
   - 新增 `event_bits -> evt_*` 虚拟通道展开，支持直接按键位诊断。

### 测试项目与数据证据

| 项目 | 测试目的 | 结论 | 数据地址 |
| --- | --- | --- | --- |
| 项目1 | 短按 `fn_1` 启动发射系统 | 通过：摩擦轮启动，未见自动拨轮动作 | `data/tune_2026-03-01_230035_proj1_prepare_fire.csv` |
| 项目2 | `trigger` 输入是否到达 | 部分通过：输入可见，执行未闭环 | `data/tune_2026-03-01_230138_proj2_trigger_receive.csv` |
| 项目3 | `trigger` 执行链路 | 失败：触发后拨轮电流/速度设定基本不动作 | `data/tune_2026-03-01_230249_proj3_trigger_execute.csv` |
| 项目4 | 保护链路是否误清零 | 当前结论无效（上游执行链未成立） | `data/tune_2026-03-01_230411_proj4_protect_clearcheck.csv` |
| 项目5 | 命令与反馈一致性 | 当前结论无效（上游执行链未成立） | `data/tune_2026-03-01_230536_proj5_cmd_feedback_consistency.csv` |
| 项目6 | 安全挡/作战挡门控对照 | 通过：安全挡屏蔽，作战挡放行 | `data/tune_2026-03-01_230654_proj6_gate_compare.csv` |
| 项目7 | 拨轮调速效果（短时） | 部分通过：先看到加速，减速采样不足 | `data/tune_2026-03-01_230750_proj7_dial_effect.csv` |
| 项目7 | 拨轮调速效果（长时） | 通过：加减速均可触发，链路可用 | `data/tune_2026-03-01_230912_proj7_dial_effect_long.csv` |
| 项目8 | 发射后速度是否始终在 15~25m/s | 失败：出现低于15和高于25 | `data/tune_2026-03-01_231926_proj8_fric_speed_guard.csv` |

### 关键数据摘录（用于定位根因）

1. 链路在线性正常：
   - 多个项目中 `vt03_toe` 持续为 0，`evt_vt03_online` 持续为 1。
2. `trigger` 输入到达但执行链未闭环（项目2~5）：
   - 可见 `evt_trigger_cur` 上升沿，但 `trigger_cur/trigger_spd_set` 长时间接近 0。
3. `dial` 调速链路存在“目标值跑飞”：
   - 项目7长时：`fric_set` 峰峰值约 `351800`。
   - 项目8长时：`fric_set` 最低约 `-1037100`，并出现估算弹速 `8.72~46.46 m/s`（超出 15~25）。
4. 由上可推断：
   - `dial` 当前按电平持续触发，导致调速步进在短时间内累积过大；
   - 目标转速偏离后，状态机难以满足“到速/就绪”条件，进而拖垮 `trigger` 执行链观测。

### 基于测试数据的合并修改方案（下一版代码）

1. `application/keyboard_action.c/.h`
   - 将 `dial` 从“电平持续触发”改为“脉冲触发 + 固定重复周期（如 80~120ms）”。
   - 增加 `vt03_trigger` 保持窗口（如 20~30ms），避免 1ms 脉冲被下游任务采样漏掉。
2. `application/shoot.c/.h`
   - 对 `fric_speed_base` 加硬限幅（按当前标定先限制在 15~25m/s 对应 RPM 区间）。
   - 仅在“发射系统已启动 + 门控允许”时接受 `dial` 调速命令，其他状态忽略。
   - 在进入 `SHOOT_STOP` 或异常回落时，将调速目标回收至安全中档，避免残留跑飞值。
3. 验收策略
   - 继续沿用项目1~8单键逐项测试，不并键；
   - 项目8作为硬门槛：长时采集中实测速度必须全程落在 15~25m/s 内。

### ⚠ 未验证假设（v7）

- 假设内容：15~25m/s 与当前 `fric rpm` 的换算关系在全电压与全弹道工况下可近似线性。
- 未验证原因：本轮仅完成电控侧遥测与现有标定换算，未做全工况弹速仪实测闭环。
- 潜在影响：若换算偏差较大，RPM 限幅可能“电控看似合规但真实弹速仍越界”。
- 后续验证计划：
  1) 先按本方案完成限幅与脉冲化改造；
  2) 用项目8长时复测确认遥测层已稳定在区间；
  3) 条件允许时追加弹速仪校准，回写最终 RPM 边界参数。
