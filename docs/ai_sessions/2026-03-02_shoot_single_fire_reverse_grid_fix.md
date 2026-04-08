# AI 会话记录：Shoot 单发修复 + 反转持续 + 拨轮格数修正
日期：2026-03-02

## 0. 上轮验收结论

- 上轮会话：`docs/ai_sessions/2026-03-01_vt03_keyboard_action.md`（v7）
- 数据来源：`MCP capture + 用户口述验收`
- 关键帧/时间段：`2026-03-02 00:30~00:47`（VT13 测试 0~8）
- 状态：`Open -> In Progress（代码已修复，待回归）`
- 差异：
  - 暴露 3 个发射链路问题：`trigger` 单次触发连发、`fn_2` 长按反转无效、拨轮每发角度按 `1/8` 计算导致过拨。
  - 本轮按指定 4 处代码修改完成修复，未改动其它参数与模块。
- 下一步：按 VT13 测试 0~8 全量回归，重点复测测试 4/5/8。

## 1. 目标

- 修复 `trigger` 单次触发变连发（实机出现约 13 发）的状态机缺陷。
- 修复 `fn_2` 长按反转仅 1ms 脉冲导致电机基本不动的问题。
- 将拨轮每发步进从 `45°(1/8)` 修正为实机 `40°(1/9)`。

## 2. 改动文件列表

- `application/shoot.h`
- `application/shoot.c`
- `application/keyboard_action.c`
- `docs/ai_sessions/2026-03-02_shoot_single_fire_reverse_grid_fix.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`

## 3. 关键改动摘要

- `application/shoot.h`
  - `TRIGGER_ONEGRID`：`36864.0f -> 32768.0f`。
  - 注释同步改为 `1/9 turn per bullet`。

- `application/shoot.c`
  - `shoot_bullet_control()` 位置到达分支新增 `shoot_control.shoot_mode = SHOOT_DONE;`，保证单发到位后退出。
  - `shoot_set_mode()` 的 `SHOOT_DONE` 分支改为统一超时回 `SHOOT_READY_BULLET`，移除按键状态驱动回 `SHOOT_BULLET` 的路径。

- `application/keyboard_action.c`
  - `fn_2` 长按条件去掉 `!long_fired` 限制，按住期间持续输出 `kb_cmd.reverse_trigger=1`，松开归零。

## 4. 关键决策与理由

- 决策 1：在 `shoot_bullet_control()` 到位分支强制写 `SHOOT_DONE`。
  - 理由：单发路径是位置环驱动，若仅清 `move_flag`，下周期会再次累加目标角度并继续发弹。
  - 备选与放弃：仅增大位置阈值或降低速度环增益不能从状态机层阻断重复触发。

- 决策 2：`SHOOT_DONE` 统一走 `SHOOT_DONE_KEY_OFF_TIME` 超时退回。
  - 理由：去除“微动开关仍 ON 立即回 BULLET”的自循环路径，确保必须等待新开火边沿。
  - 备选与放弃：维持原 `else -> SHOOT_BULLET` 会与决策 1 抵消，仍存在连发风险。

- 决策 3：`fn_2` 长按改为电平持续而非单点脉冲。
  - 理由：反转是速度命令类动作，需要保持一定持续时间才有实际机械效果。
  - 备选与放弃：仅放宽脉冲阈值（如 10ms）仍对任务采样和电机惯性敏感，稳定性差。

- 决策 4：按实机结构修正 `TRIGGER_ONEGRID=32768`。
  - 理由：机械盘 9 格，每发 40°；旧值按 45° 会导致每发过拨 12.5% 并累积错位。

## 5. 实时性影响（必须写）

- 阻塞变化：未新增 `osDelay/vTaskDelay`，无新阻塞调用。
- CPU 影响：仅新增/调整常数级条件分支与状态赋值。
- 栈影响：无新增大对象；任务栈、线程创建和优先级未改。
- 帧率/周期：控制周期仍为 1ms 路径，无调度结构变化。

## 6. 风险点

- 若 `fn_2` 物理键卡死，反转命令会持续输出，需依赖松键/上层保护恢复。
- `TRIGGER_ONEGRID` 修正后，旧的“每发角度经验值”不再适用，需重新确认单发到位统计。
- 单发状态机改为更严格退出后，需复核高频/连发路径是否仅由 `SHOOT_CONTINUE_BULLET` 触发，不出现误抑制。

## ⚠ 未验证假设

- 假设：`TRIGGER_ONEGRID=32768` 与当前减速比、编码器展开计数在所有供弹工况下都对应“恰好一发”。
- 未验证原因：本轮仅完成代码修复，未执行修复后 20 次以上单发统计。
- 潜在影响：若机构磨损或间隙变化，仍可能出现偶发少拨/过拨。
- 后续验证计划：按测试 5 连续执行 20 次单发，统计 `bullet_cnt` 增量与 `trigger_ecd` 到位误差；若异常再做阈值微调。

- 假设：`fn_2` 长按持续反转不会与堵转自动反转策略相互打架。
- 未验证原因：未完成堵转 + 长按混合工况联测。
- 潜在影响：在极端卡弹场景下，反转退出时机可能影响恢复时间。
- 后续验证计划：执行“人工堵转 -> 长按 fn_2 -> 松开恢复”序列，采集 `trigger_spd/trigger_cur/evt_cmd_reverse_trigger`。

## 7. 验证方式与结果

- 串口：本轮未新增串口日志。
- USB/MCP：
  - 上轮问题证据来源于 MCP 采集：
    - `data/tune_2026-03-02_003909_vt13_test4_fn2_long_reverse.csv`
    - `data/tune_2026-03-02_004028_vt13_test5_trigger_fire.csv`
    - `data/tune_2026-03-02_004213_vt13_test6_dial_speed.csv`
    - `data/tune_2026-03-02_004609_vt13_test8_mode_sw.csv`
  - 本轮完成代码级核对：4 处指定改动均已落地到目标分支。
- 示波器：未执行。
- 上位机：未新增 GUI 侧验证。
- 长跑：未执行。
- 编译：本会话未执行 Keil/armcc 编译（待本地 F7）。

## 8. 回滚方式（如何恢复）

- 代码回滚点：
  - `application/shoot.h`：`TRIGGER_ONEGRID` 回退到 `36864.0f`。
  - `application/shoot.c`：撤销 `shoot_bullet_control()` 到位分支 `SHOOT_DONE` 赋值；恢复 `SHOOT_DONE` 原条件分支。
  - `application/keyboard_action.c`：恢复 `fn_2` 长按条件中的 `!long_fired`。
- 参数回滚点：不涉及其它 PID/限幅宏改动，无需额外参数回滚。
- 运行时保护：回滚前后均应先置 `SHOOT_STOP`，再做空载联调。

## 9. 附件/证据

- 采集数据：
  - `data/tune_2026-03-02_003909_vt13_test4_fn2_long_reverse.csv`
  - `data/tune_2026-03-02_004028_vt13_test5_trigger_fire.csv`
  - `data/tune_2026-03-02_004609_vt13_test8_mode_sw.csv`
- 相关风险条目：`docs/risk_log.md`（2026-03-02 新增条目，Shoot 单发/反转/格数修复）

## v2: VT03 s[1] 假边沿 + mouse.press_l 噪声自动开火修复（仅 vt03_link.c）

### 改动摘要

- 文件：`application/vt03_link.c`
- 修改 1：`rc_ctrl.rc.s[1] = RC_SW_DOWN` -> `RC_SW_MID`（位于 `vt03_frame_decode()` 映射段）。
- 修改 2：`mouse.press_l/press_r` 由 `d[14]` 解析改为强制清零：
  - `rc_ctrl.mouse.press_l = 0;`
  - `rc_ctrl.mouse.press_r = 0;`

### 决策理由

- `s[1]` 默认置 `DOWN` 会与发射状态机的 `last_s` 初值产生假边沿，导致未按 trigger 也可能进入开火路径。
- VT13 无物理鼠标，`d[14]` 不是可靠鼠标按键来源；保留该解析会把随机字节噪声直接注入 `press_l`。
- VT03 开火触发已走独立链路：`vt03_ext.trigger -> keyboard_action -> kb_cmd.vt03_trigger -> shoot_feedback_update`，不依赖 `mouse.press_l`。

### ⚠ 未验证假设（v2）

- 假设：将 `s[1]` 固定为 `RC_SW_MID` 不会影响 VT03 其它按键动作（`fn_1/fn_2/pause/dial`）的门控行为。
- 未验证原因：本轮只做代码修复，尚未执行完整 0~8 回归。
- 潜在影响：若有隐式逻辑依赖 `s[1]` 的 `UP/DOWN`，可能引入副作用。
- 后续验证计划：复测测试 1/5，重点看“仅 `fn_1` 启动后 5s 内 `bullet_cnt` 不自增、`evt_trigger_cur` 无脉冲”。

## v3: 移除 BULLET 微动开关提前退出 + TRIGGER_ONEGRID 回调（仅 shoot.c/h）

### 改动摘要

- 文件：`application/shoot.c`
  - 在 `shoot_bullet_control()` 中删除如下提前退出逻辑：
    - `if (shoot_control.key == SWITCH_TRIGGER_OFF) { shoot_control.shoot_mode = SHOOT_DONE; }`
  - 其余位置环控制流程保持不变，仍在到位分支设置 `shoot_mode = SHOOT_DONE`。
- 文件：`application/shoot.h`
  - `TRIGGER_ONEGRID` 从 `32768.0f` 回调为 `36864.0f`。
  - 注释同步为：`// 8192 * 36 / 8 = 36864 (1/8 turn per bullet)`。

### 决策理由

- VT03 的 fire_cmd 可从 `SHOOT_READY_BULLET` 直接进入 `SHOOT_BULLET`，而该工况下微动开关通常为 OFF。
- 若 `shoot_bullet_control()` 内仍保留“key=OFF 立即 DONE”，会在第一个控制周期提前退出，导致拨轮仅小幅转动且不发弹。
- 改为纯位置环判定发弹完成（到位后 DONE）可避免该路径被微动开关误杀，与既有连续发射路径兼容。
- 将 `TRIGGER_ONEGRID` 回调到 `36864`，与当前 1/8 分度机构匹配。

### ⚠ 未验证假设（v3）

- 假设：当前机构分度确实是 `1/8`，`TRIGGER_ONEGRID=36864` 在不同摩擦/供弹状态下都能稳定完成单发。
- 未验证原因：本轮未完成修复后 20 次以上单发实机统计。
- 潜在影响：若实物分度与参数不匹配，仍可能出现少拨/过拨或卡弹恢复异常。
- 后续验证计划：复测测试 5（至少 20 次），观察 `bullet_cnt`、`trigger_ecd_fdb` 到位误差与发弹成功率。

## v4: READY_BULLET/DONE 活跃制动（位置环常开）抑制单发 overshoot

### 改动摘要

- 文件：`application/shoot.c`
- 改动 1：零电流门控从 `<= SHOOT_READY_BULLET` 改为 `< SHOOT_READY_BULLET`，使 `READY_BULLET` 不再被强制清零。
- 改动 2：`SHOOT_READY_BULLET` 分支改为位置环计算 `speed_set`，并保持 `trigger_ecd_set` 不跟随当前反馈。
- 改动 3：`SHOOT_DONE` 分支改为位置环计算 `speed_set`，保持上次目标用于过冲制动。
- 保持不变：
  - `shoot_bullet_control()` 未改动；
  - `SHOOT_READY` 分支仍保留 `trigger_ecd_set = trigger_ecd_fdb`；
  - 其它状态分支未改。

### 决策理由

- 现象根因是单发完成后进入 `READY_BULLET` 时目标点被跟随重置、且电流被门控清零，导致电机自由滑行。
- 采用 HUST 方案语义：目标点固定、位置环持续输出速度设定，过冲时误差为负自然形成反向制动。
- 将门控边界改为 `< READY_BULLET` 后，`READY_BULLET/DONE` 两态可输出制动电流，同时仍保留 `STOP/READY_FRIC` 的零电流保护。

### ⚠ 未验证假设（v4）

- 假设：当前 `trigger_pos_pid + trigger_spd_pid` 参数在 `READY_BULLET/DONE` 活跃制动下不会引入低幅振荡或高频来回抖动。
- 未验证原因：本轮只完成代码改动，尚未做长时静置和连续单发实机回归。
- 潜在影响：若阻尼不足，可能出现“制动后小幅往返”或温升增加。
- 后续验证计划：
  1) 测试5连续单发 20 次，观测每次 `trigger_ecd_fdb` 过冲量是否收敛；
  2) 空闲 10 秒观察 `trigger_cur` 是否快速归零且无持续抖动；
  3) 对比改动前后单发物理格数（目标回到约 1 格）。

## v5: 修复 READY_BULLET 位置环堵转满电流，改为 DONE 阶段延长制动

### 改动摘要

- 文件：`application/shoot.c`
- 改动 1：`SHOOT_READY_BULLET` 分支恢复为“零速度 + 位置跟随”：
  - `trigger_speed_set = 0.0f`
  - `speed_set = 0.0f`
  - `trigger_ecd_set = trigger_ecd_fdb`
- 改动 2：零电流门控恢复为 `<= SHOOT_READY_BULLET`，确保 READY_BULLET 电流归零。
- 改动 3：`shoot_set_mode()` 中 `SHOOT_DONE` 状态转移增加速度条件与超时兜底：
  - `key_time > SHOOT_DONE_KEY_OFF_TIME && fabs(speed) < 1.0f` 才允许转入 READY_BULLET
  - `key_time > 500` 强制转入 READY_BULLET，防止异常卡死
- 保持不变：
  - `SHOOT_DONE` 控制分支仍为位置 PID 制动（上一版改动保留）；
  - `shoot_bullet_control()` 未改；
  - `TRIGGER_ONEGRID = 36864` 未改。

### 决策理由

- v4 方案在本机构上触发了 READY_BULLET 持续满电流堵转风险（机械阻力大、误差难消）。
- 将主动制动集中在 DONE 阶段更安全：此时转子仍在运动，位置环输出用于减速制动而非静态硬顶。
- 进入 READY_BULLET 后恢复零电流与位置跟随，可避免长时间堵转发热。

### ⚠ 未验证假设（v5）

- 假设：`fabs(speed)` 低速门限 + `key_time > SHOOT_DONE_KEY_OFF_TIME` 的停稳判据对当前电机反馈噪声足够鲁棒，不会过早/过晚切态。
- 未验证原因：本轮未做长时实机验证与噪声边界测试。
- 潜在影响：若阈值不合适，可能出现 DONE 停留过长或仍有残余过冲。
- 后续验证计划：
  1) 复测测试5连续 20 次，统计单发成功率与过冲格数；
  2) 观测 READY_BULLET 阶段 `trigger_cur` 应快速回零，避免持续高电流；
  3) 验证极端情况下 `key_time > 500` 兜底能稳定回到 READY_BULLET。

## v6: HUST 风格拨弹控制落地（BULLET 全力矩 + DONE/READY_BULLET 柔和持仓 + 反转次数上限）

### 改动摘要

- 文件：`application/shoot.h`
  - 新增宏：
    - `TRIGGER_POS_MAX_OUT_HOLD = 10.0f`
    - `TRIGGER_POS_MAX_OUT_FIRE = 20000.0f`
    - `MAX_REVERSE_COUNT = 3`
  - `shoot_control_t` 新增字段：`uint8_t reverse_count`。
- 文件：`application/shoot.c`
  - `SHOOT_READY_BULLET` 分支改为位置 PID 柔和持仓（`max_out=HOLD`，`ecd_set` 不跟随）。
  - `SHOOT_DONE` 分支改为位置 PID 柔和持仓（`max_out=HOLD`）。
  - 零电流门控由 `<= SHOOT_READY_BULLET` 收窄到 `<= SHOOT_READY_FRIC`。
  - `shoot_bullet_control()` 开头恢复 `max_out=FIRE`，并在新射击起始时 `reverse_count=0`。
  - `trigger_motor_turn_back()` 增加反转次数上限：`reverse_count >= 3` 时放弃本次并回 `READY_BULLET`。
  - `shoot_init()` 新增 `reverse_count=0` 初始化。

### 决策理由

- 保留 BULLET 阶段全力矩（满弹阻力场景不降扭矩），同时将 DONE/READY_BULLET 的位置环限幅降到可控范围，避免高增益级联下电流饱和振荡。
- `SetPoint` 只在发射/反转等关键事件变化，非发射状态不再频繁重置目标点，减少过冲后失控滑行。
- 反转次数上限用于防止持续卡弹时“无限反转-重试”导致热与机械风险上升。

### ⚠ 未验证假设（v6）

- 假设：`TRIGGER_POS_MAX_OUT_HOLD=10` 在当前速度环增益下足以抑制过冲且不会引发显著持仓振荡。
- 未验证原因：尚未完成连续单发与高阻力（满弹）双场景回归。
- 潜在影响：若 HOLD 限幅过小，可能回位慢；若过大，可能在个别工况下仍出现大电流脉冲。
- 后续验证计划：
  1) 单发 20 次统计每次编码器增量是否稳定在 1 格附近；
  2) 满弹工况验证 BULLET 阶段是否仍具备足够推弹力；
  3) 人工卡弹验证 `reverse_count` 超限后能安全回到 `READY_BULLET`。
