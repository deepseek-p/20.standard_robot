# 风险日志（模板）

用途：记录跨会话持续存在的技术风险，避免“知道有问题但无人跟进”。

## 风险条目

| 日期 | 模块 | 风险描述 | 触发条件 | 影响范围 | 严重度（H/M/L） | 缓解措施 | 当前状态 | 验证计划 | 负责人 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| YYYY-MM-DD | 例如 `chassis_task` | 例如“功率限制阈值在低电压下可能误触发” | 例如“电池 < 22V 且持续急加速” | 例如“底盘动力突降” | H | 例如“增加滤波 + 分段阈值” | Open | 例如“长跑 15min + 上位机功率记录” | @name(对话名称为负责人名字) |
| 2026-02-25 | `gimbal_task` | Pitch ENCONDE 模式若速度环仍用 IMU pitch gyro 反馈，反馈源与执行器不一致，连杆工况下易引起振荡或不稳定 | `pitch_mode=ENCONDE` 且 pitch 闭环控制启用 | pitch 稳定性、电流波动、机构冲击 | H | `gimbal_feedback_update()` 中 ENCONDE 模式改为编码器 `speed_rpm -> rad/s` 反馈；并下调 ENCONDE 角度环 `Kp/Ki`、关闭 `Kd`，提高 `max_out/max_iout` 以改善可控区间 | In Progress | 烧录后采集 `pitch_mode/pitch_gyro/pitch_gyro_set/pitch_given_current/pitch_relative_set`，确认 ENCONDE 下速度反馈同相且峰值下降 | @Codex |
| 2026-02-25 | `gimbal_behaviour` / `gimbal_task` / `AHRS.lib` | pitch 轴 `GYRO` 反馈链路失真（90°旋转仅约2.6°），导致绝对角行为下 pitch 控制不可用；已切换为 ENCONDE fallback | `s0=MID/UP` 且绝对角行为启用，pitch 仍依赖 AHRS pitch 反馈时 | pitch 稳定性、限位安全、瞄准一致性 | H | 绝对角行为下改为 `yaw=GYRO`、`pitch=ENCONDE`；并重设 `PITCH_ENCODE_RELATIVE_PID_*` 到连杆适配参数 | In Progress | 烧录后用 MCP 连续采集验证：模式值、`pitch_rel/rel_set` 收敛、`pitch_cur` 波动、轻推与松手回位 | @Codex |
| 2026-02-24 | `gimbal_task` | Pitch 连杆机构下遥控输入灵敏度过高，轻推摇杆即可能快速触发上下物理限位 | `s0=MID/UP`，`rc_ch3` 持续输入，目标角以过高斜率累加 | Pitch 操控手感、机械冲击风险、瞄准精度 | H | 本轮仅降低输入增量宏：`PITCH_RC_SEN -0.000004f -> -0.000001f`；保持 PID 不变 | In Progress | 复测慢推/快推/微调，观测 `col43/col34/col33/col39`，确认不再快速触限且响应可接受 | @Codex |
| 2026-02-24 | `usb_task` / `usbd_cdc_if` | 新增 USB 在线 PID 调参后，若误下发极端参数（尤其 `Kp/Ki`）或命令源不可控，可能引发控制不稳定；同时回复与遥测共享 CDC，拥塞时会丢回复 | USB CDC 接入并连续执行 `SET` / `DUMP`，或现场多主机误连接 | 云台/底盘稳定性、调参效率、联调安全 | H | 已增加参数范围约束与非阻塞回复（忙时丢弃）；下一步增加上位机侧白名单与“参数生效清单”操作规程 | In Progress | 执行 4 组实测：单条 SET/GET、连续 SET、DUMP、边遥测边调参；记录回复丢失率、`drop` 增长、控制链路是否异常 | @Codex |
| 2026-02-24 | `gimbal_task` | Pitch 连杆驱动下，第一轮 GYRO 参数已显著改善角度稳态，但速度环仍出现静止段极限环（`col39` 在约 `+5000~-6600` 周期翻转） | `s0=MID/UP` 且无输入静止或慢推/快推后回位 | Pitch 机构冲击、瞄准稳定性、温升 | H | 已执行第二轮速度环降增益：`Kp 1500->800`、`Ki 30->10`、`max_out 20000->15000`、`max_iout 5000->3000`；角度环参数保持不变 | In Progress | 已在 `data/pitch_data.md` 第二版（1136-1492 行）回填：静止波动显著下降；当前慢推/快推峰值约 `4413/4967`，下一轮继续压到 `<4000` | @Codex |
| 2026-02-24 | `chassis_behaviour` / `chassis_task` | UP（HUST_SelfProtect）下若平移命令未做坐标补偿，会因底盘自旋导致轨迹画圆、操作方向漂移 | `s0=UP` 且 `wz` 持续自旋同时存在 `vx/vy` 输入 | 小陀螺机动可控性、操作手瞄向一致性 | H | 第一轮已加入 `relative_angle` 坐标旋转补偿；上轮验收结论为“圈变小但未消除”，第二轮已修复符号（`sin(-rel)` -> `sin(rel)`）；用户口述“现象正常，可边转边走” | Mitigated（口述，待VOFA+/长跑） | 后续补充 VOFA+ 四场景与长跑数据：静止自旋、自旋+推前、自旋+推侧、MID/UP 切挡，确认量化指标稳定 | @Codex |
| 2026-02-23 | `chassis_behaviour` / `chassis_task` | 将 `GPS(s0=UP)` 改为 HUST SelfProtect（常值旋转）后，若 `wz` 偏大或地面附着高，可能出现起转冲击、电流峰值升高 | 上电后直接进 GPS 挡位，或平移叠加持续旋转 | 底盘电机温升、功率余量、机体稳定性 | H | 先以 `CHASSIS_HUST_SELF_PROTECT_WZ=4.5f` 作为保守初值；实机按“空载抬轮 -> 落地低速 -> 正常机动”分阶段验证，必要时下调 `WZ` 与平移缩放宏 | Open | 记录 `wz_set`、底盘电流、功率缓冲；覆盖静止 30s、自旋平移 30s、ATTI<->GPS 切挡三类场景 | @Codex |
| 2026-02-23 | `chassis_behaviour` | 将 ATTI/GPS 的驾驶语义切换为“ATTI=HUST_Act、GPS=HUST_SelfProtect”后，若操作手沿用旧记忆，可能误入小陀螺 | 联调/比赛中按旧习惯拨挡 | 操作一致性、战术动作执行 | M | 在遥控器贴纸/操作文档明确 `s0` 新语义；赛前口令确认一次 | Open | 联调时执行 10 次模式切挡确认并记录误操作次数 | @Codex |
| 2026-02-23 | `gimbal_task` / `gimbal_behaviour` | yaw 连续角 v4 改造后，若切模态同步或校准后状态对齐不完整，仍可能出现瞬时误差放大 | MID/UP 高频切换、校准结束后立即操控、连续旋转工况 | yaw 电机冲击、电流峰值、调试误判 | H | 已加入 mode transit 三分支连续 set 同步、nowrap 误差限幅与 snapshot 连续角输出；用户已反馈“MID/UP 切换与转圈可用”，后续补长跑与温升证据 | In Progress | 记录 `yaw_mode/yaw_absolute_set/yaw_absolute/yaw_gyro_set/yaw_given_current`，覆盖 20 次切模态 + 正反各 10 圈 + 10min 温升测试 | @Codex |
| 2026-02-23 | `chassis_behaviour` / `chassis_task` | ATTI 跟随切入时若将 `rel_set` 硬置 0，在 `rel` 初值偏离 0 时会触发较大瞬时跟随输出 | `s0` 切入 MID（FOLLOW_GIMBAL）且 `rel` 初值偏离 0 | 底盘切挡冲击、联调风险 | H | 切入 FOLLOW_GIMBAL 时执行 `rel_set = rel`；用户已反馈 MID/UP 切换无异常冲击 | Mitigated | MID/UP 连续切挡 20 次复测 `wz_set/rel/rel_set`，确认无首拍饱和复现 | @Codex |
| 2026-02-23 | `chassis_task` / `gimbal_behaviour` | ATTI 下若出现 `ch_mode=FOLLOW_GIMBAL_YAW` 同时 `yaw_mode=ENCONDE`，底盘跟随误差可能不可收敛并长期打满 `wz_set`（无摇杆输入也自旋） | `s0=MID`、`rc_ch2=0`、`rel!=rel_set` 且云台处于相对角模式 | 底盘持续高速自旋、电流峰值、联调与实车安全风险 | H | 已执行 step1/step2 解除 FOLLOW+ENCONDE 耦合；后续参数收敛到 `Kp=8, Ki=0.5, Kd=0, max_out=3, max_iout=1`，并将 FOLLOW 刹车阈值调到 `PI*0.6 -> PI*0.95` 区间抑制回绕极限环；用户反馈 MID 基本调好 | Mitigated（待长跑） | 继续回看 `11/14/15/16/19~22/23/42/44`，重点验证长跑工况下 `wz_set` 不再周期翻转且 `rel` 维持可收敛 | @Codex |
| 2026-02-22 | `gimbal_task` / `gimbal_behaviour` | 重新启用 yaw 连续旋转与“跳过 yaw 校准步”后，若误刷到未装滑环机体会出现绕线风险；在滑环机体上持续高速旋转也可能带来温升与寿命风险 | 未确认硬件条件即刷写连续旋转版本；或长时间大幅同向 yaw 输入 | 线束/滑环安全、维护可靠性 | H | 默认通过 `GIMBAL_YAW_CONTINUOUS_TURN` 单开关控制；上线前强制执行“已装滑环”检查；必要时将宏改回 `0` 回退到原限位逻辑 | Open | 连续同向+往返各 10 分钟，记录温升与电流；另在测试分支验证 `GIMBAL_YAW_CONTINUOUS_TURN=0` 可回退到旧行为 | @Codex |
| 2026-02-22 | `gimbal_task` / `chassis_task` | 按用户指令回退到 `2026-02-20_usb_debug_telemetry_framework` 基线，已撤销 2/20~2/22 针对 pitch/chassis 的防御性改动；`GPS` 下 pitch 受扰动后下冲问题重新回到待定位状态 | GPS 模式、无拨杆输入时对 pitch 机构施加轻微外扰 | pitch 机构冲击、联调效率 | H | 先固定在 USB 稳定基线继续采集故障数据，禁止继续叠加防御逻辑；按“模式值+设定值+电流峰值”三元组重新定位真实故障路径 | Open | 采集 `pitch_mode/pitch_abs_set/pitch_abs/pitch_gyro_set/pitch_cur`；要求给出触发前后 3s 连续帧并标注触发时刻 | @Codex |
| 2026-02-22 | `chassis_task` | 纯 GPS（`rc_s0=1`）无输入时底盘 yaw 跟随闭环仍有振荡；已从早期 `±6000` 降到 `±2500` 级别，但切挡回 GPS 后仍会放大 | `ch_mode=0` 且 `rc_ch2=rc_ch3=0`，ATTI->GPS 切挡后 | 底盘稳定性、云台协同、电机冲击 | H | 第1轮修正方向符号；第2轮加入 `rad_format(set-ref)` + `0.03rad` 死区 + `2.5rad/s` 限幅；第3轮继续参数收敛：`Kp 40->8`、死区 `0.03->0.06rad`、限幅 `2.5->1.6rad/s` | In Progress | 回传“第3轮改后纯 GPS 静置 + 切挡”日志，重点核对 `11/14/15/16/19~22/42/43/44`，确认 `wz_set` 不再长期贴 `±1600` 且电流峰值进一步下降 | @Codex |
| 2026-02-19 | `gimbal_task` / `gimbal_behaviour` | yaw 机械限位去除后，持续高速旋转可能超出滑环安全工况或触发维护误操作 | 连续大幅遥控/鼠标输入，或误触发云台校准流程 | 滑环温升、寿命下降、姿态控制稳定性边界 | H | 启用 `GIMBAL_YAW_CONTINUOUS_TURN` 时同步跳过 yaw 校准步；联调阶段限制输入幅度并执行温升监测；必要时追加 yaw 速度软限幅 | Open | 绝对/相对模式连续旋转与反向切换测试 + 10min 长跑 + 上位机电流曲线 + 温升记录 | @Codex |
| 2026-02-19 | `chassis_task` / `chassis_behaviour` | 实机“GPS 模式”出现底盘与云台左右抽搐，疑似底盘跟随闭环在连续旋转场景下不稳定 | 进入底盘跟随云台角模式并持续 yaw 操作 | 底盘机动稳定性、云台跟随体验、实战可靠性 | H | 先完成模式映射与变量观测（`chassis_mode/chassis_relative_angle_set/relative_angle/wz_set`）；确认是否为单圈角回绕与通道耦合引发；再决定是否调参与解耦 | Open | 已观测“GPS 静止自转 + 左摇杆换向抽搐”；下一步录制关键变量曲线，重点观察 `relative_angle` 过 ±PI 时 `wz_set` 响应 | @Codex |
| 2026-02-19 | `gimbal_task` | pitch 轴在较大输入下出现上下大幅来回冲，且上下触发阈值不对称（上推约 2/3、下推约 1/2），疑似环路参数、限位与机械状态共同作用 | GPS 左摇杆工况与 pitch 大幅输入工况 | pitch 稳定性、瞄准精度、机构冲击风险 | H | 分离机械与控制因素：先固定底盘与 yaw，再做 pitch 阶跃测试，记录 `absolute_angle/absolute_angle_set/motor_gyro_set/given_current`；同时核对上下机械限位与摩擦差异 | Open | 已观测“上推约 2/3、下推约 1/2 触发大幅来回冲”；下一步做分方向阶跃与限位附近测试，并同步示波器/上位机记录 | @Codex |
| 2026-02-19 | `remote_control` / `gimbal_task` / `chassis_task` | DBUS 断联保护已完成一次实机验证，但样本与证据不足 | 断链/恢复工况覆盖不充分 | 安全保护完整性 | M | 按安全流程补做多次断链测试并沉淀日志证据，确认云台/底盘归零与恢复行为一致 | In Progress | 已验证“断联瞬停、恢复后随当前摇杆状态动作”；下一步执行断链 3 次与恢复 3 次并记录 `YAW_GIMBAL_MOTOR_TOE` 与 CAN 输出状态 | @Codex |
| 2026-02-20 | `chassis_behaviour` | 连续 yaw 版本中已将上档强制改为 `CHASSIS_NO_FOLLOW_YAW`，可止住跟随闭环抽搐，但会改变原“上档跟随云台”的操作语义 | `GIMBAL_YAW_CONTINUOUS_TURN = 1` 且遥控上档 | 操控手感、战术动作一致性、维护人员模式认知 | M | 文档和会话中明确“连续 yaw 版上档不跟随”；联调阶段按 ATTI/GPS 两档逐项复测并确认操作手册更新 | In Progress | 验证上档不再触发跟随抽搐；记录 `chassis_mode/wz_set`，确认不出现跟随角闭环输出 | @Codex |
| 2026-02-20 | `gimbal_task` | 已增加 pitch 下行软保护（下行增量限幅 + min 软边界），可降低下行冲撞风险，但可能牺牲部分下视角范围且参数尚未实机定标 | 大幅下压 pitch，尤其连杆驱动且下方向无机械限位时 | pitch 可用行程、瞄准覆盖范围、机构安全 | H | 保持参数宏可调（`GIMBAL_PITCH_DOWN_ADD_LIMIT`、`GIMBAL_PITCH_DOWN_SOFT_GUARD_ANGLE`）；先以安全优先验证，再按数据回调参数 | In Progress | 按上电脚本测“静止/底盘转动/摇杆阶跃”三类场景，记录抖动阈值、是否仍有大幅往返冲、以及可用俯仰范围 | @Codex |
| 2026-02-21 | `gimbal_task` | pitch 相对角模式下，最小边界夹紧在无下压输入时可能触发，导致目标值被吸附到软下限并出现持续下压电流 | ATTI/GPS 工况切换、无 pitch 输入但 `relative_angle_set` 历史值异常或越界 | pitch 轴抖动、下砸到底、机构冲击风险 | H | 新增 `GIMBAL_PITCH_MIN_CLAMP_REQUIRE_DOWN_CMD`；在 `gimbal_relative_angle_limit()` 中仅当 `add < 0` 时执行最小边界夹紧，避免“无下压输入自动吸附” | In Progress | 复测 ATTI/GPS 静止与左右摇杆场景，重点观察 `pitch_rel/pitch_rel_set/pitch_gyro_set/pitch_cur` 是否仍出现 `-10000/-30000` 饱和组合 | @Codex |
| 2026-02-21 | `gimbal_task` | pitch 绝对角模式在无输入时可能出现持续加力；并且在 ATTI/GPS（`pitch_mode:2->1`）切换瞬间可能继承切换前 PID 状态导致下冲 | GPS 工况高速旋转、连杆机构耦合、模式切换瞬间 | pitch 电机过流、机构冲击、云台稳定性下降 | H | 已启用 `GIMBAL_PITCH_ABS_NO_CMD_GUARD_ENABLE`（无输入小误差时置零并清理 PID）；并在 `gimbal_mode_change_control_transit()` 中对 pitch 在 `ENCODE<->GYRO` 切换时同步设定值并清空角度环/速度环状态 | In Progress | 复测“ATTI->GPS 切换瞬间 + GPS静止/左右摇杆”，重点观察 `pitch_mode/pitch_abs/pitch_abs_set/pitch_gyro_set/pitch_cur` 是否仍出现瞬时大电流并下冲到底 | @Codex |
| 2026-02-22 | `gimbal_task` | pitch 在 `pitch_mode=1` 且无输入时，外部轻微扰动可使 `abs_err` 超过无输入小误差阈值，触发过大的 `pitch_gyro_set` 与电流尖峰，出现“向下猛打”风险 | `rc_ch3=0 && mouse_y=0 && pitch_mode=1` 且外力扰动导致 `|pitch_abs_set - pitch_abs| > 0.08rad` | 发射机构/连杆冲击、pitch 可控性 | H | 在 `gimbal_motor_absolute_angle_control()` 的无输入分支加入纠偏角速度限幅（`|pitch_gyro_set| <= 0.35rad/s`），并在限幅触发时清速度环积分，抑制大电流突发 | In Progress | 对比修改前后同场景，重点观察 `pitch_gyro_set` 与 `pitch_cur` 峰值；参考故障帧基线 `1279/13732`，期望显著下降并且不再出现下砸到底 | @Codex |
| 2026-02-20 | `usb_task` | USB 调试遥测频率提高后，若主机未就绪或 CDC 通道繁忙，可能出现持续丢包与调试数据不连续 | USB 未枚举、串口工具未打开、输出通道过多或频率过高 | 调试结论可信度、排障效率 | M | 使用通道掩码与分通道周期限频；保留 `drop` 计数并在日志中持续上报；必要时关闭 `RC` 通道或降低 GIMBAL/CHASSIS 频率 | Open | 在默认配置下长跑 10 分钟，统计 `drop` 增长速率；再在高负载配置（全通道）复测并确定安全频率上限 | @Codex |
| 2026-02-20 | `usb_task` / `gimbal_task` / `chassis_task` | 调试快照为跨任务无锁读取，极端情况下可能读到非原子组合值（同一帧内字段时间不完全一致） | 云台/底盘任务高速更新同时 USB 任务取样 | 单帧精确性、瞬态分析精度 | M | 明确“快照用于趋势与事件定位，不作为硬实时闭环依据”；后续若需高精度可升级为双缓冲时间戳快照 | Open | 比对 USB 遥测与上位机/示波器关键边沿，确认趋势一致性；若出现高频抖动误判再引入双缓冲机制 | @Codex |

| 2026-02-25 | `gimbal_task` | pitch ENCONDE 速度反馈虽已改为 ecd差分，但 1ms 周期下反馈量化阶梯（0/0.767rad/s）导致速度环阻尼不连续，残余振荡仍明显 | `pitch_mode=ENCONDE` 且静止/小扰动工况 | pitch 稳定性、回位品质、电流波动 | M | 在 `gimbal_feedback_update()` 的 ENCONDE 分支加入一阶低通（alpha=0.05）平滑速度反馈 | In Progress | 对比 `pitch_gyro/pitch_given_current/pitch_relative`：确认阶梯噪声下降、振荡幅值缩小、回位更平滑 | @Codex |

| 2026-02-25 | `gimbal_task` | pitch ENCONDE 默认参数若未固化在线验证最优值，上电后仍需重复在线调参，存在调参一致性与现场效率风险 | 使用旧默认 `Kp=6, Ki=0.1` 上电运行 | pitch 跟踪精度、调试效率、现场稳定性 | M | 固化 `PITCH_ENCODE_RELATIVE_PID` 为 `Kp=24, Ki=0, Kd=0, max_out=10, max_iout=0.5`，抑制连杆静摩擦下积分 windup | Mitigated（待长跑） | 复测上电即用场景：静态误差、动态跟踪误差、连续动作 10min 温升后稳定性 | @Codex |

## 使用规则

- 每个高风险改动都应检查是否新增/更新风险条目。
- 风险关闭前，至少保留一次“验证通过”证据（日志、波形、上位机记录等）。
- 若风险影响实时性，需在 `docs/ai_sessions/*.md` 交叉引用该条目。

## 2026-02-20 Supplement Risk Entries

- Module: `usbd_cdc_if`
- Risk: calling `CDC_Transmit_FS` before USB CDC class is configured can dereference null `pClassData` and hard fault.
- Trigger: power-on with USB physically connected but host side not yet configured.
- Mitigation: return `USBD_FAIL` when `hUsbDeviceFS.pClassData == NULL`.
- Status: Mitigated in code, pending repeated power-cycle verification.

- Module: `usb_task`
- Risk: FireWater frame length may exceed buffer and generate truncated lines that break VOFA+ parsing.
- Trigger: field count growth or large integer values with small tx buffer.
- Mitigation: increase frame buffer to `1024` and drop oversized frames instead of sending truncation.
- Status: Mitigated in code, pending VOFA+ long-run verification.


