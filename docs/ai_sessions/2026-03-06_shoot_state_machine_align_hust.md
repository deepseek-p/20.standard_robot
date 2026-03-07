# AI 会话记录：Shoot 状态机 HUST 结构对齐

日期：`2026-03-06`

## 0. 会话摘要

- 本日先完成了板端空载遥测排查。
- 第一轮因为串口链路不可用，结论是 `Blocked`。
- 随后恢复 `COM22` 后完成了一次空载单发后的实时采集，结论是 `Failed`：
  - `shoot_mode` 长时间停在 `SHOOT_READY_BULLET`
  - `trigger_ecd_set` 固定不变
  - `trigger_ecd_fdb` 围绕旧目标大幅往返
  - `trigger_cur` 持续打满 `±10000`
- 基于源码对比 `HUST_Infantry_2023` 后，确认主要差异不是 PID 常数，而是结构与语义：
  - HUST 是“上层命令生成 + 下层拨盘执行”
  - HUST 在动作结束后会把持仓目标重新贴到当前反馈
  - HUST 末端还有一层 `±8000` 电流限流
- 本轮已按批准方案把本项目内部重构为更接近 HUST 的命令层/执行层结构，但还没有重新完成 Keil 编译、烧录和板端复验。

## 1. 本轮目标

- 将 `shoot` 从单文件混合状态机重构为更接近 HUST 的内部结构：
  - 上层命令生成
  - 下层 trigger executor
- 取消微动开关驱动主状态迁移，仅保留为观测/保护信号。
- 修复上一轮暴露出的两类高风险行为：
  - `READY_BULLET` 持仓自激与持续大电流
  - 手动反转/长按反转语义不清，可能重复退多格
- 在不能直接上板前，先通过主机侧 TDD 固化关键行为。

## 2. 关键板端证据回顾

### 2.1 串口阻塞轮次

- 工具目录：`D:\tools\stm32-telemetry-mcp`
- 早期链路探测阶段，未枚举到目标 `COM22`，因此空载 20 发未能开始。
- 该轮结论：`Blocked`

### 2.2 空载失败轮次

- 证据文件：
  - `D:\RM资料\电控\Development-Board-C-Examples-master\Development-Board-C-Examples-master\20.standard_robot\data\tune_2026-03-06_shoot_hust_acceptance_live_001.csv`
- 关键现象：
  - 8 秒采集共 `399` 帧
  - `shoot_mode` 全程为 `2`，即 `SHOOT_READY_BULLET`
  - `bullet_cnt` 维持 `6 -> 6`
  - `trigger_ecd_set` 固定在 `310947`
  - `trigger_ecd_fdb` 围绕固定目标往返，位置误差约 `-85218 ~ +70748`
  - 8 秒内位置误差符号翻转 `67` 次
  - `trigger_cur` 持续打满 `±10000`
- 判定：
  - “READY 段目标被周期回写”不是主因
  - 真正问题是“固定旧目标下的持仓环路持续自激”
- 该轮结论：`Failed`

## 3. HUST 对照后的设计决策

- 保留当前工程对外接口：
  - `shoot_init()`
  - `shoot_control_loop()`
  - `shoot_get_fric_current()`
- 内部改为两层：
  - `shoot_command_layer`
  - `trigger_executor_layer`
- 微动开关处理改为 HUST 风格：
  - 保留观测值
  - 不再驱动 `READY_BULLET <-> READY` 主状态切换
- 执行层独占：
  - `trigger_ecd_set`
  - 单发一步一格
  - 连发退出回贴
  - 手动反转与反转完成
  - 单发完成计数
- 增加末端输出限流，与 HUST 对齐到 `±8000`

## 4. 本轮实现落地

### 4.1 新增文件

- `application/shoot_logic.c`
- `application/shoot_logic.h`
- `tests/shoot_logic_test.c`

### 4.2 核心代码改动

- `application/shoot.c`
  - 接入 `shoot_logic`
  - 由 executor 驱动 `shoot_mode` 兼容映射
  - 删除手动反转的旧“直接速度旁路”
  - `READY_BULLET/READY/DONE` 使用独立 hold clamp
  - 返回前新增 trigger 电流最终限流
- `application/shoot.h`
  - 新增：
    - `TRIGGER_POS_MAX_OUT_HOLD`
    - `TRIGGER_CAN_CURRENT_LIMIT`
  - 新增字段：
    - `microswitch_on`
    - `arm_enable`
    - `single_fire_req`
    - `continue_req`
    - `reverse_req`
    - `arm_state_dbg`
    - `trigger_exec_state_dbg`
- `application/usb_task.c`
  - FireWater 新增：
    - `trigger_sw`
    - `reverse_flag`
    - `referee_heat`
- `MDK-ARM/standard_robot.uvprojx`
  - 将 `shoot_logic.c` 加入工程文件

### 4.3 行为语义变化

- idle hold 改为 HUST 风格：
  - 进入 idle 时回贴当前位置
  - 不再长期抱住历史单发目标
- single fire：
  - 只在进入时推进一格
  - 到位后只计数一次
  - 之后重新贴当前反馈
- manual reverse：
  - 进入 executor 内部 `REVERSE_RECOVER`
  - 完整回退一格后退出
  - 不增加 `bullet_count`
  - 对“长按保持 `reverse_trigger=1`”增加边沿门控，避免连续多格回退
- current clamp：
  - 最终 `given_current` 限制到 `±8000`

## 5. 本轮 TDD 与本地验证

### 5.1 RED -> GREEN

- 新增测试 1：idle hold 首次进入时回贴当前位置
- 新增测试 2：single fire 只推进一格一次
- 新增测试 3：微动开关不再驱动 READY 分裂
- 新增测试 4：idle hold 不每周期追着反馈跑
- 新增测试 5：manual reverse 必须完整退一格且不计数
- 新增测试 6：长按 `reverse_trigger=1` 不得连续退多格
- 新增测试 7：`STOP/READY_FRIC` 阶段必须持续把目标贴到最新反馈，不能把启动早期旧值带入 READY

### 5.2 主机侧命令

```powershell
D:\64\x86_64-14.2.0-release-win32-seh-msvcrt-rt_v12-rev1\mingw64\bin\gcc.exe tests/shoot_logic_test.c application/shoot_logic.c -I application -o tests/shoot_logic_test.exe
tests\shoot_logic_test.exe
```

### 5.3 结果

- 上述测试全部通过，退出码 `0`
- `git diff --check` 已执行，只有 LF/CRLF warning，没有 patch error

## 6. 未完成验证

- 当前会话未找到可用的：
  - `UV4.exe`
  - `armcc.exe`
  - `armclang.exe`
- 因此本轮尚未完成：
  - Keil 工程级编译
  - 下载烧录
  - 基于新固件的 `COM22` 空载 20 发复验

## 6.1 遥测工具链恢复

- 已确认 `COM22` 板端持续输出 FireWater 帧，直接读原始串口可见每帧 `67` 列。
- `D:\tools\stm32-telemetry-mcp\frame_parser.py` 仍停留在 `TOTAL_COLUMNS = 65`，导致 `capture()` 将全部新固件帧判为非法并返回 `0` 帧。
- 本轮已将 `frame_parser.py` 补齐到 `67` 列，并同步映射：
  - `64:trigger_sw`
  - `65:reverse_flag`
  - `66:referee_heat`
- 修复后使用 `D:\tools\stm32-telemetry-mcp` 重新采集 `COM22`：
  - 命令：`capture('COM22', 2.5, [...])`
  - 结果：`123` 帧 / `2.5s`
  - 已确认可稳定读出：
    - `shoot_mode`
    - `trigger_spd`
    - `trigger_spd_set`
    - `trigger_ecd_fdb`
    - `trigger_ecd_set`
    - `bullet_cnt`
    - `trigger_cur`
    - `trigger_sw`
    - `reverse_flag`
    - `local_heat`
    - `referee_heat`
- 当前阻塞已从“工具不能采”转为“等待按验收步骤做板端动作并抓取空载数据”。

## 6.2 新固件 READY 静态复验

- 证据文件：`data/tune_2026-03-06_shoot_hust_acceptance_live_003_static_ready.csv`
- 操作：进入可发射状态，但不按 `trigger`，静态保持 `5s`
- 采集结果：
  - `274` 帧 / `5.5s`
  - `shoot_mode` 全程 `2`（`READY_BULLET`）
  - `bullet_cnt` 全程 `0 -> 0`
  - `trigger_ecd_set` 全程固定 `0`，`SET_CHANGES = 0`
  - `trigger_ecd_fdb` 范围 `-90618 ~ 96767`
  - 位置误差范围 `-90618 ~ 96767`
  - 误差符号翻转 `41` 次
  - `trigger_cur` 全程饱和在 `±8000`，`|current| >= 8000` 占比 `274/274`
  - `trigger_spd` 范围 `-49.282 ~ 47.479`
  - `trigger_spd_set` 全程打满 `±8000`
  - `trigger_sw` 全程 `0`
  - `reverse_flag` 全程 `0`
- 结论：
  - `READY_BULLET` 仍存在持续极限环 / 自激，静态门直接失败
  - 新增 `±8000` 限流只降低了幅值，没有消除“固定目标下的往返抽动”
  - 这次证据进一步排除了两条支路：
    - 不是微动开关抖动触发的主状态切换
    - 不是反转恢复路径在反复介入
  - 当前更像是拨弹执行层方向/误差符号/控制律本身在 `idle hold` 下不稳定
- 门禁结论：
  - 空载静态未通过
  - 禁止进入空载单发 20 次
  - 继续禁止进入实弹

## 6.3 根因定位与最小修复

- 新证据：
  - 工具恢复后的 STOP 基线中，`trigger_ecd_fdb = -258020`，但 `trigger_ecd_set = 0`
  - 这说明问题不只发生在 READY 阶段，目标值在更早的未就绪阶段就已经被锁成了陈旧值
- 根因判断：
  - `shoot_logic.c` 的 executor 在 `!arm_enable || !fric_ready` 分支中，只在“首次进入 idle hold”时把 `trigger_ecd_set` 贴到 `trigger_ecd_fdb`
  - 若 `shoot_init()` 或早期控制周期抓到的是尚未有效更新的拨弹反馈，之后即使真实编码器反馈上线，`trigger_ecd_set` 也不会在 STOP/WAIT_FRIC 里继续刷新
  - 一旦摩擦轮 ready，系统进入 `READY_BULLET`，就会带着一个陈旧 setpoint 与真实反馈之间的巨大位置误差，位置环立即把 `trigger_spd_set` 打满，随后速度环把 `trigger_cur` 打满，形成持续极限环
- 与 HUST 的对照：
  - `HUST_Infantry_2023` 的 `Shoot_Powerdown_Cal()` 每周期都执行 `PidBodanMotorPos.SetPoint = Bodan_Pos`
  - 也就是在未进入发射执行前，拨盘目标始终贴当前真实位置，不会把旧值带到 ready hold
- 本轮最小修复：
  - 修改 `application/shoot_logic.c`
  - 在 `!arm_enable || !fric_ready` 分支中，改为每周期执行 `exec->trigger_ecd_set = exec->trigger_ecd_fdb`
  - 保持 `READY_BULLET` 下的 idle hold 不每周期追反馈，避免重新引入“READY 段目标周期回写”
- TDD：
  - 新增测试：`test_disarmed_or_wait_fric_keeps_target_attached_to_latest_feedback()`
  - RED：修复前断言失败，`exec.trigger_ecd_set` 仍停在早期旧值
  - GREEN：修复后测试通过，且原有 idle hold / single fire / reverse 相关测试继续通过

## 6.4 修复后 STOP 基线复验

- 证据文件：`data/tune_2026-03-06_shoot_hust_acceptance_live_004_postfix_stop_baseline.csv`
- 操作：烧录本轮 stale-setpoint 修复后，上电保持 `STOP` 静态 `2.5s`
- 采集结果：
  - `124` 帧 / `2.5s`
  - `shoot_mode` 全程 `0`（`STOP`）
  - `trigger_ecd_fdb` 全程 `-4042`
  - `trigger_ecd_set` 全程 `-4042`
  - `fdb - set` 全程 `0`
  - `trigger_cur = 0`
  - `trigger_spd = 0`
  - `trigger_sw = 0`
  - `reverse_flag = 0`
- 结论：
  - 本轮修复已经消除了“STOP 基线里 `trigger_ecd_set` 锁在旧值 `0`”的问题
  - 说明未就绪阶段 stale setpoint 这一层根因在板端已经被修正
  - 下一步可以进入 `READY` 静态 `5s` 复验，验证是否仍有持仓自激

## 6.5 修复后 READY 静态复验

- 证据文件：`data/tune_2026-03-06_shoot_hust_acceptance_live_005_ready_static_after_fix.csv`
- 操作：切入可发射状态，不按 `trigger`，静态保持 `5s`
- 采集结果：
  - `274` 帧 / `5.5s`
  - `shoot_mode` 过渡为 `[0, 1, 2]`，末态稳定在 `2(READY_BULLET)`
  - `bullet_cnt` 全程 `0 -> 0`
  - `trigger_ecd_fdb` 全程 `-4041`
  - `trigger_ecd_set` 全程 `-4041`
  - `fdb - set` 全程 `0`
  - `trigger_cur = 0`
  - `trigger_spd = 0`
  - `trigger_spd_set = 0`
  - `trigger_sw = 0`
  - `reverse_flag = 0`
- 结论：
  - 修复后 `READY_BULLET` 静态段不再出现自激、往返摇摆或持续大电流
  - 先前的 READY 持仓失稳已至少在“静态不击发”场景下消失
  - 空载门禁可以从“禁止进入单发”推进到“允许进入空载单发 20 次”

## 6.6 修复后空载单发 5 发预验收

- 证据文件：`data/tune_2026-03-06_shoot_hust_acceptance_live_006_empty_single_5shots.csv`
- 操作：空载单发 `5` 发，用户实际节奏快于预定 `3s` 间隔，但全程未触发反转
- 采集结果：
  - `1098` 帧 / `22s`
  - `shoot_mode` 仅出现 `[2, 4, 6]`，未出现 `5(CONTINUE_BULLET)`
  - `bullet_cnt` 全程 `0 -> 5`
  - `5` 次计数增量均为 `+1`
  - `trigger_ecd_set` 只在单发进入和完成回贴时变化，`READY_BULLET` 内未观察到周期性回写
  - `SHOOT_DONE(6)` 只观测到 `1` 帧，未出现长时间停留；其余 4 发在当前采样粒度下未显式采到 DONE 帧
- 逐发关键指标：
  - Shot1：后续窗口 `peak|fdb-set| = 68022`，`peak|cur| = 8000`
  - Shot2：后续窗口 `peak|fdb-set| = 59264`，`peak|cur| = 8000`
  - Shot3：后续窗口 `peak|fdb-set| = 75218`，`peak|cur| = 8000`
  - Shot4：后续窗口 `peak|fdb-set| = 99616`，`peak|cur| = 8000`
  - Shot5：后续窗口 `peak|fdb-set| = 82768`，`peak|cur| = 8000`
- 与基线 `data/tune_2026-03-06_003.csv` 对比：
  - 当前 `5` 发 `peak|fdb-set|` 最大值 `99616`
  - 基线前 `5` 发同口径最大值 `53924`
  - 当前相对基线恶化约 `+45692`
  - 当前前 `5` 发平均 `peak|fdb-set| = 76977.6`
  - 基线前 `5` 发平均 `peak|fdb-set| = 48424.6`
  - 当前相对基线恶化约 `+28553`
- 更关键的动态失效：
  - 以每次 `bullet_cnt` 增量到下一次增量/采集结束为区间统计，`trigger_cur` 在所有区间都持续饱和 `±8000`
  - 各区间 `ZEROISH(|cur|<=500)` 帧数均为 `0`
  - 说明虽然静态 READY 已稳定，但单发完成后系统仍长时间处于大误差和持续高电流恢复过程，没有正常收敛回稳
- 结论：
  - 空载单发动态验收 `Failed`
  - 通过项：`bullet_cnt` 每次只 `+1`；未误入 `CONTINUE_BULLET`；`READY_BULLET` 未观察到周期性 setpoint 回写
  - 失败项：overshoot 明显劣于基线；单发后持续高电流/长时间不回稳；`DONE` 虽未长停，但当前采样下也未稳定观测到每发都出现
- 门禁结论：
  - 禁止继续后续 `15` 发空载单发
  - 继续禁止进入实弹

## 6.7 按 HUST 收尾语义再次修正

- 对照结论：
  - `HUST_Infantry_2023` 中：
    - `PluckThreholdPos = 5000` 只用于“是否允许推进下一格/是否接近当前位置目标”
    - 鼠标松手后只有在 `ABS(Bodan_Pos - PidBodanMotorPos.SetPoint) < 1000` 时，才执行 `PidBodanMotorPos.SetPoint = Bodan_Pos`
  - 当前项目上一版的问题是：
    - 一旦 `|pos_err| < 5000` 就立即把 `trigger_ecd_set` 回贴到当前位置
    - 这比 HUST 更早结束单发，导致动态 overshoot 和长时间高电流恢复
- 本轮代码调整：
  - `application/shoot_logic.h`
    - 新增 `fire_input_active`
    - 新增 `shot_counted`
    - 新增 `SHOOT_LOGIC_RELEASE_THRESHOLD = 1000`
  - `application/shoot_logic.c`
    - `STEP_ONE_GRID` 中，`5000` 仅作为“本发已完成一格动作”的计数阈值
    - 不再在 `|pos_err| < 5000` 时立即 `DONE + 回贴当前位`
    - 只有在“输入已释放且 `|pos_err| < 1000`”时，才按 HUST 语义执行 `DONE` 和 `trigger_ecd_set = trigger_ecd_fdb`
  - `application/shoot.c`
    - 上层命令层新增 `fire_input_active` 传递给 executor，用于对齐 HUST 的“松手后回贴”条件
- TDD：
  - 新增测试：
    - `test_single_fire_waits_for_release_and_tighter_settle_before_done()`
    - `test_single_fire_does_not_done_while_trigger_still_held()`
  - 先因接口/行为不满足而 `RED`
  - 本轮修改后重新编译运行，全部主机侧测试通过

## 6.8 按 HUST 收尾语义烧录后的空载单发 5 发复验

- 证据文件：`data/tune_2026-03-06_shoot_hust_acceptance_live_007_empty_single_5shots_hust_finish.csv`
- 操作：烧录本轮 HUST 收尾语义修复后，再次执行空载单发 `5` 发
- 采集结果：
  - `1099` 帧 / `22s`
  - `shoot_mode` 出现 `[0, 1, 2, 4]`，未观测到 `5(CONTINUE_BULLET)`，也未稳定采到 `6(DONE)`
  - `bullet_cnt` 全程 `0 -> 5`
  - `5` 次计数增量均为 `+1`
  - `READY_BULLET` 内未观察到周期性 `trigger_ecd_set` 回写
  - 逐发后续恢复段 `peak|trigger_ecd_fdb-trigger_ecd_set|`：
    - Shot1：`70222`
    - Shot2：`66111`
    - Shot3：`70864`
    - Shot4：`76859`
    - Shot5：`83377`
  - 当前前 `5` 发 `peak|fdb-set|` 最大值 `83377`，平均 `73486.6`
- 与上一轮 `live_006` 对比：
  - 上一轮最大值 `99616`，平均 `76977.6`
  - 本轮最大值下降 `16239`
  - 本轮平均值下降 `3491.0`
  - 说明按 HUST 收尾语义延后回贴后，overshoot 有改善，但改善幅度不足以通过验收
- 与基线 `data/tune_2026-03-06_003.csv` 对比：
  - 基线前 `5` 发最大值 `53924`
  - 基线前 `5` 发平均值 `48424.6`
  - 当前最坏值仍劣化 `29453`
  - 当前平均值仍劣化 `25062.0`
- 更关键的动态失效：
  - 每次 `bullet_cnt` 增量后的恢复区间内，`trigger_cur` 仍几乎全程饱和在 `±8000`
  - 各发之间未恢复到低电流持仓态
  - `SHOOT_DONE` 未能在当前遥测中稳定显式出现
- 结论：
  - 本轮空载单发 `5` 发复验仍为 `Failed`
  - 通过项：`bullet_cnt` 每次只 `+1`；未误入 `CONTINUE_BULLET`；`READY_BULLET` 内无周期性 setpoint 回写
  - 失败项：overshoot 仍明显劣于基线；单发后恢复区间电流持续饱和；`DONE` 不满足“稳定短暂可观测”
- 门禁结论：
  - 继续禁止后续 `15` 发与整轮 `20` 发
  - 继续禁止进入实弹

## 6.9 shoot executor 集成层 6 项修复

- 触发背景：
  - 根据 `Claude` 的集成层审查意见，以及板端证据 `data/tune_2026-03-06_shoot_hust_acceptance_live_007_empty_single_5shots_hust_finish.csv`
  - 本轮目标不是改 PID 参数，而是收敛 executor 集成层的状态管理、计弹归属和持仓入口行为
- 本轮代码修改：
  - `Fix 1`：
    - 在 `application/shoot.c` 增加 `shoot_clear_trigger_pid_state()`
    - 当 `shoot_mode` 进入 `SHOOT_DONE` 或从非 hold 状态回到 `SHOOT_READY_BULLET` 时，统一执行
      - `PID_enhanced_clear(&shoot_control.trigger_pos_pid)`
      - `PID_enhanced_clear(&shoot_control.trigger_spd_pid)`
    - 目的：消除单发大误差阶段遗留的双环积分，避免回到持仓时 `trigger_cur` 因旧积分持续打满
  - `Fix 2`：
    - `trigger_motor_turn_back()` 不再直接改写 `shoot_exec_state.exec_state / trigger_ecd_set / hold_target_valid / public_mode`
    - 改为在 `shoot.c` 侧仅产生 `jam_reverse_req / jam_abandon_req`
    - 通过 `shoot_command_logic_update()` 传给 `shoot_executor_logic_update()` 消费
    - 当前 `shoot.c` 中仅保留每周期同步拷贝与末尾读回，不再越权写 executor / command 内部字段
  - `Fix 3`：
    - 连发计弹移入 `application/shoot_logic.c`
    - `shoot_executor_state_t` 新增 `trigger_ecd_last_fire`
    - `TRIGGER_EXEC_CONTINUOUS_RUN` 中按 `one_grid` 位移统一递增 `bullet_count`
    - `shoot.c` 删除原先 `SHOOT_CONTINUE_BULLET` 路径里的 while 旁路计弹逻辑
  - `Fix 4`：
    - `application/shoot.h` 中 `trigger_ecd_set / trigger_ecd_fdb / trigger_ecd_last_fire` 全部改为 `int32_t`
    - 同步去掉 `shoot.c` 中不必要的 `fp32 <-> int32_t` 往返转换
  - `Fix 5`：
    - 删除 `SHOOT_READY` / `SHOOT_LOGIC_MODE_READY` 死代码符号
    - 但为保持遥测数值兼容，`SHOOT_BULLET/CONTINUE_BULLET/DONE` 继续显式保持原数值 `4/5/6`
    - `shoot_control_loop()` 中 READY 分支仅保留 `SHOOT_READY_BULLET || SHOOT_DONE`
  - `Fix 6`：
    - executor 消费 `single_fire_req` 后立即显式清零
    - 防止极端时序下同一 req 被重复消费
- 测试补充：
  - `tests/shoot_logic_test.c`
    - 新增 `test_jam_abandon_returns_idle_hold_and_attaches_target()`
    - 新增 `test_continuous_run_counts_bullets_by_one_grid_progress()`
    - 新增 `test_single_fire_req_is_cleared_after_consumption()`
  - 现有测试全部继续通过
- 主机侧验证：
  - 编译命令：
    - `D:\64\x86_64-14.2.0-release-win32-seh-msvcrt-rt_v12-rev1\mingw64\bin\gcc.exe tests/shoot_logic_test.c application/shoot_logic.c -I application -o tests/shoot_logic_test.exe`
  - 运行结果：
    - `tests\shoot_logic_test.exe` 退出码 `0`
  - `git diff --check`：
    - 通过，仅剩仓库现有 LF/CRLF warning
- 当前结论：
  - 本轮 6 项代码修复与主机侧回归均已完成
  - 但尚未重新烧录并复做板端空载单发，因此板端总判定仍保持 `Failed`

## 7. 当前结论

- 代码状态：`In Progress`
  - HUST 风格 command layer + trigger executor 已在当前 worktree 落地
  - 手动反转 one-grid 语义、长按边沿门控、末端限流和新增遥测字段均已接入
  - 本轮已补上 stale setpoint 根因修复
  - 本轮又按 HUST 收尾语义修正了单发完成条件与回贴时机
  - 本轮已继续完成 shoot executor 集成层 6 项修复：
    - PID 持仓入口清零
    - jam request flag 化
    - 连发计弹回收至 executor
    - trigger ecd 相关字段改为 `int32_t`
    - 删除 READY 死代码
    - `single_fire_req` 消费后清零
- 板端验收状态：`Failed`
  - 结构性修复已落地，板端遥测链路已恢复
  - STOP 基线与 READY 静态板端复验均已通过
  - 按 HUST 收尾语义重新烧录后，空载单发 `5` 发复验仍 `Failed`
  - 相比 `live_006`，最坏 overshoot 已回落，但仍显著劣于基线
  - 各发之间 `trigger_cur` 仍持续饱和，`DONE` 也未稳定暴露
  - 本轮代码修复尚未重新上板验证，因此板端结论暂不变化
- 空载验收门：
  - READY 静态门已通过
  - 但空载单发动态门未通过
  - 禁止继续后续 15 发与整轮 20 发
- 实弹验收门：
  - 继续禁止，直到空载 20 次通过

## 8. 下一步

1. 烧录本轮 6 项集成层修复后的固件
2. 重新执行空载单发 `5` 发预验收
3. 重点复核：
   - `SHOOT_DONE` 是否开始稳定短暂可观测
   - `trigger_cur` 是否能在各发恢复段快速回零，而非持续饱和
   - `peak|trigger_ecd_fdb-trigger_ecd_set|` 是否继续回落并接近 `data/tune_2026-03-06_003.csv`
4. 空载 `5` 发未通过前，继续禁止扩到 `20` 发与实弹

## ⚠ 未验证假设

- 假设：READY 自激的主根因是“未就绪阶段 stale setpoint 被带入 READY_BULLET”，而不是执行层方向或误差符号本身反了。
- 当前状态：STOP 基线与 READY 静态板端复验都支持该假设。
- 潜在影响：静态 READY 已恢复，且按 HUST 收尾语义后的空载单发 `5` 发最坏 overshoot 已较 `live_006` 有所下降；本轮又补上了 PID 清零、jam flag 化和 executor 统一计弹，但这些修复是否足以消除动态恢复段的持续饱和，仍待板端复验。
- 后续验证计划：
  - 负责人：`@Codex`
  - 方法：烧录本轮 6 项集成层修复后，重新从空载单发 `5` 发开始，并对比 `data/tune_2026-03-06_003.csv`
  - 触发条件：完成本轮修复固件烧录后立即执行
  - 门槛：空载单发未通过前，禁止进入实弹

## 6.10 READY_BULLET 持仓限幅收窄

- 触发背景：
  - READY_BULLET 静态持仓仍存在极限环风险，根因进一步收敛为 hold 阶段位置环 max_out/max_iout 仍明显偏大
  - 位置环输出的是 speed_set，而不是直接电流；当 hold 阶段 max_out 远超电机真实速度量级时，速度环会立刻饱和并把 trigger_cur 推到高位
- 本轮修改：
  - `application/shoot.h`
    - `TRIGGER_POS_MAX_OUT_HOLD: 8000.0f -> 15.0f`
    - 新增 `TRIGGER_POS_MAX_IOUT_HOLD = 10.0f`
  - `application/shoot.c`
    - `SHOOT_READY_BULLET || SHOOT_DONE` 分支新增 `trigger_pos_pid.max_iout = TRIGGER_POS_MAX_IOUT_HOLD`
    - `shoot_bullet_control()` 中 fire 阶段恢复 `trigger_pos_pid.max_iout = TRIGGER_POS_MAX_IOUT`
- 保持不变：
  - 未改位置/速度 PID 的 `Kp/Ki/Kd`
  - 未改速度环参数
  - 未改 `shoot_logic.c/h`
  - 未改 `TRIGGER_POS_MAX_OUT_FIRE`
  - 未改遥测列数
- 主机侧验证：
  - `gcc tests/shoot_logic_test.c application/shoot_logic.c -I application -o tests/shoot_logic_test.exe`
  - `tests\shoot_logic_test.exe`
  - 退出码 `0`
- 当前结论：
  - 本轮仅完成代码侧 hold 限幅修正
  - 板端 READY_BULLET 静态持仓和空载单发恢复段是否收敛，仍需重新烧录并复验
