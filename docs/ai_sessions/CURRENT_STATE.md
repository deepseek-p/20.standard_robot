# CURRENT_STATE锛堟敹绾抽〉锛?
鍚屾鏃ユ湡锛歚2026-02-24`
鏈€杩戣ˉ褰曪細`docs/ai_sessions/2026-02-24_pitch_speed_loop_limit_cycle_tune.md`

## 褰撳墠鐢熸晥鎺у埗璇箟锛堜互浠ｇ爜涓哄噯锛?
- `s0=DOWN`锛歚CHASSIS_NO_MOVE` + `GIMBAL_ZERO_FORCE`
- `s0=MID`锛歚CHASSIS_INFANTRY_FOLLOW_GIMBAL_YAW` + `GIMBAL_ABSOLUTE_ANGLE`锛圚UST_Act锛?- `s0=UP`锛歚CHASSIS_HUST_SELF_PROTECT` + `GIMBAL_ABSOLUTE_ANGLE`锛圚UST_SelfProtect锛?- yaw 杩炵画鏃嬭浆閾捐矾淇濇寔鍚敤锛坄GIMBAL_YAW_CONTINUOUS_TURN` 璺緞浠嶅湪锛?
## 鍓嶅嚑娆℃敼鍔ㄩ獙鏀剁姸鎬侊紙琛ュ綍锛?
| 浼氳瘽 | 楠屾敹鐘舵€?| 褰撳墠缁撹 |
| --- | --- | --- |
| `2026-02-24_pitch_linkage_gyro_mode_pid_tune.md` | In Progress | 宸茬‘璁?GYRO 鍒囨ā鎴愬姛涓旂ǔ鎬佽璇樊鏄捐憲涓嬮檷锛涗絾闈欐娈甸€熷害鐜粛鏈夋瀬闄愮幆鎸崱銆?|
| `2026-02-24_pitch_speed_loop_limit_cycle_tune.md` | In Progress | 宸插湪 `data/pitch_data.md` 绗簩鐗堝洖濉埌鏀瑰悗鏁版嵁锛氶潤姝㈡鏄庢樉鏀瑰杽锛屼絾鎱㈡帹/蹇帹宄板€间粛鍋忛珮锛岄渶缁х画璋冨弬銆?|
| `2026-02-23_chassis_mid_follow_restore_step1.md` + `2026-02-23_mid_follow_gimbal_absolute_alignment_fix.md` | 宸查獙鏀?| MID 涓嬪凡鍒囧埌 FOLLOW + ABS 缁勫悎锛岃В闄?FOLLOW/ENCONDE 缁撴瀯鎬у啿绐併€?|
| `2026-02-23_chassis_follow_pid_step1_soft_gain.md` + `2026-02-23_chassis_follow_wraparound_brake_fix.md` + 鍚庣画鍙傛暟琛ヨ皟 | 宸查獙鏀?| MID 鎸崱涓婚棶棰樺凡鍘嬩綇锛岀敤鎴峰弽棣堚€淢ID 妯″紡宸茬粡鍩烘湰璋冨ソ鈥濄€?|
| `2026-02-23_gimbal_yaw_continuous_turn_v4_callsite_split.md` | 閮ㄥ垎楠屾敹 | 鐢ㄦ埛鍙嶉 MID/UP 鍒囨尅涓?yaw 杩炵画杞湀鍙敤锛涢暱璺戜笌娓╁崌楠岃瘉浠嶅緟琛ラ綈銆?|
| `2026-02-23_chassis_hust_mode_mapping_swap.md` + `2026-02-24_chassis_selfprotect_frame_rotation_compensation.md` + `2026-02-24_chassis_selfprotect_sin_sign_fix.md` | 宸查獙鏀讹紙鍙ｈ堪锛?| 鐢ㄦ埛鍙嶉 UP鈥滅幇璞℃甯革紝鍙竟杞竟璧扳€濓紱褰撳墠缂?VOFA+ 閲忓寲涓庨暱璺戣瘉鎹€?|

## 褰撳墠鍏抽敭鍙傛暟蹇収

- `application/chassis_task.h`
- `CHASSIS_FOLLOW_GIMBAL_PID_KP=8.0f`
- `CHASSIS_FOLLOW_GIMBAL_PID_KI=0.5f`
- `CHASSIS_FOLLOW_GIMBAL_PID_KD=0.0f`
- `CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT=3.0f`
- `CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT=1.0f`
- `application/chassis_task.c`
- `CHASSIS_FOLLOW_ANGLE_BRAKE_START=(PI*0.6f)`
- `CHASSIS_FOLLOW_ANGLE_BRAKE_END=(PI*0.95f)`
- `application/gimbal_task.h`
- `PITCH_SPEED_PID_KP=800.0f`
- `PITCH_SPEED_PID_KI=10.0f`
- `PITCH_SPEED_PID_MAX_OUT=15000.0f`
- `PITCH_SPEED_PID_MAX_IOUT=3000.0f`
- `PITCH_GYRO_ABSOLUTE_PID_KP=5.0f`
- `PITCH_GYRO_ABSOLUTE_PID_KI=0.3f`
- `PITCH_GYRO_ABSOLUTE_PID_KD=0.1f`
- `PITCH_GYRO_ABSOLUTE_PID_MAX_OUT=5.0f`
- `PITCH_GYRO_ABSOLUTE_PID_MAX_IOUT=1.0f`
- `PITCH_RC_SEN=-0.000004f`

## 浠嶅緟瀹屾垚鐨勯獙鏀堕」

- UP锛圚UST_SelfProtect锛夊洓鍦烘櫙鑱旀祴锛氶潤姝㈣嚜鏃嬨€佹棆杞?骞崇Щ銆佹柟鍚戝垏鎹€丮ID/UP 鍒囨尅鐬€併€?- yaw 杩炵画鏃嬭浆闀胯窇涓庢俯鍗囬獙璇侊紙鍚鍙嶅悜闀挎湡宸ュ喌锛夈€?- Pitch 閫熷害鐜簩璋冮獙鏀讹細琛ュ綍闈欐/鎱㈡帹/蹇帹 `col24/33/34/37/39`锛岄噸鐐圭‘璁?`col39` 闈欐娉㈠姩鏄惁鏀舵暃鍒?`卤500` 鍐呫€?
## 椋庨櫓鏃ュ織瀵瑰簲鍏崇郴

- MID 璺熼殢鑷棆/鎸崱锛氳 `docs/risk_log.md` 涓?2026-02-23 `chassis_task / gimbal_behaviour` 鏉＄洰锛堝凡杞负鈥淢itigated锛屽緟闀胯窇鈥濓級銆?- UP 灏忛檧铻哄钩绉荤敾鍦嗭細瑙?`docs/risk_log.md` 涓?2026-02-24 `chassis_behaviour / chassis_task` 鏉＄洰锛坄Mitigated锛堝彛杩帮紝寰匳OFA+/闀胯窇锛塦锛夈€?- Pitch 杩炴潌璋冨弬涓庨€熷害鐜尟鑽★細瑙?`docs/risk_log.md` 涓?2026-02-24 `gimbal_task` 鏉＄洰锛坄In Progress`锛夈€?
## 2026-02-24 USB Online PID Tuning
- Status: In Progress
- Added runtime USB CDC commands in application/usb_task.c: SET <target> <param> <value>, GET <target> <param>, DUMP.
- Added RX ring buffer in Src/usbd_cdc_if.c and exported usb_rx_available / usb_rx_read_byte.
- Added mutable control accessors: get_gimbal_control_point, get_chassis_control_point.
- Pending: Keil build + board-side command interaction validation.

## 2026-02-24 Pitch RC Sensitivity
- Status: In Progress
- Changed only one macro in application/gimbal_task.h: PITCH_RC_SEN -0.000004f -> -0.000001f.
- PID parameters and all .c files unchanged.
- Pending: VOFA+ recheck for rc_ch3 to pitch_abs_set slope and limit-hit behavior.

## 2026-02-25 Pitch ENCONDE Fallback
- Status: In Progress
- In GIMBAL_ABSOLUTE_ANGLE behavior: yaw stays GIMBAL_MOTOR_GYRO, pitch switched to GIMBAL_MOTOR_ENCONDE.
- Updated PITCH_ENCODE_RELATIVE_PID to: Kp=8.0, Ki=0.2, Kd=0.1, max_out=5.0, max_iout=1.0.
- PITCH_SPEED_PID and PITCH_ENCODE_SEN unchanged.
- Pending: MCP post-flash verification on assembled gimbal.

## 2026-02-25 Pitch ENCONDE Speed Feedback
- Status: In Progress
- In `gimbal_feedback_update`, pitch uses encoder speed (`speed_rpm * 0.104720f`) as `motor_gyro` when `pitch_mode=GIMBAL_MOTOR_ENCONDE`; non-ENCONDE modes still use IMU gyro.
- Updated `PITCH_ENCODE_RELATIVE_PID` to: `Kp=6.0`, `Ki=0.1`, `Kd=0.0`, `max_out=10.0`, `max_iout=2.0`.
- `PITCH_SPEED_PID_*` remains `800/10/0/15000/3000`.
- Pending: MCP/VOFA+ post-flash verification for `pitch_gyro` consistency and current ripple.
## 2026-02-25 Pitch ENCONDE ECD LPF
- Status: In Progress
- In `gimbal_feedback_update` (pitch ENCONDE branch), added first-order LPF for ecd-diff speed feedback: `alpha=0.05`.
- Purpose: smooth 1ms quantization step (0/0.767rad/s) and improve speed-loop damping continuity.
- Expected gain: lower residual oscillation and current ripple under static/light disturbance.
- Pending: MCP/VOFA+ post-flash verification on `pitch_gyro`, `pitch_given_current`, `pitch_relative`.
## 2026-02-25 Pitch ENCONDE PID Finalize
- Status: Mitigated（待长跑）
- Solidified `PITCH_ENCODE_RELATIVE_PID` to verified values: `Kp=24.0`, `Ki=0.0`, `Kd=0.0`, `max_out=10.0`, `max_iout=0.5`.
- Rationale: avoids static-friction induced integrator windup in linkage mechanism and removes per-boot online retune.
- Pending: long-run thermal validation under continuous operation.

## 2026-02-26 Shoot Fric CAN Closed-Loop Migration
- Status: In Progress
- Replaced fric control path from PWM ramp to CAN speed closed-loop (`hcan2`, `0x200` group, `0x201/0x202` feedback).
- `CAN_receive` now splits motor parsing by CAN bus to avoid `hcan1/hcan2` same-ID aliasing on `0x201/0x202`.
- `shoot` state machine now uses real fric RPM readiness check, plus keyboard edge controls: `Q` toggle, `C` high frequency, `F/SHIFT+F` speed trim, `G` trigger reverse.
- `gimbal_task` now sends both `CAN_cmd_gimbal` and `CAN_cmd_fric`; fault paths send zero to both.
- Pending: board-side direction check, ready-state transition check, and continuous fire verification.

## 2026-02-26 WiFi Bridge (USART1 + ESP32)
- Status: Blocked - 需数据
- STM32:
  - 新增 `application/wifi_bridge.h` 统一 `WIFI_BRIDGE_ENABLE`。
  - `referee_usart_task` 在 WiFi 模式保持原裁判初始化与解包链路（`usart6_init` + FIFO 解包）。
  - `USART6_IRQHandler` 回归裁判 IDLE+DMA；WiFi RX 改为 `USART1_IRQHandler` + 环形缓冲。
  - `usb_task` 新增 `wifi_uart1_init`、`wifi_cmd_process`、命令回复路由与 USART1 遥测发送。
  - `remote_control.c` 在 WiFi 模式下屏蔽 `sbus_to_usart1`，避免 USART1 复用冲突。
- ESP32:
  - 保持 `D:/tools/esp32_wifi_bridge`，AP=`TX`，UDP:8401，TCP:8402，按行分类转发。
- Pending:
  - 缺实机联调数据，需回填 capture/set/get/dump 连续压测结果、丢帧统计与遥控链路回归。

## 2026-02-27 USB/WiFi Telemetry Mode Mutex + Split Drop Counter
- Status: In Progress
- `usb_task.h` 新增互斥输出模式：
  `TELEM_MODE_NONE / TELEM_MODE_USB / TELEM_MODE_WIFI`，
  默认 `TELEM_OUTPUT_MODE=TELEM_MODE_USB`。
- `wifi_bridge.h` 的 `WIFI_BRIDGE_ENABLE` 改为从 `TELEM_OUTPUT_MODE` 自动派生。
- `usb_task.c`：
  - 主循环仅在非 NONE 模式调度 FireWater 输出。
  - 输出路径改为互斥（USB-only / WiFi-only / no-output）。
  - drop 计数拆分为 `usb_debug_drop_cnt` 和 `wifi_debug_drop_cnt`，
    FireWater 的 drop 字段在 WiFi 模式显示 `wifi_debug_drop_cnt`。
- Pending:
  - 在 Keil 中分别验证 `TELEM_OUTPUT_MODE=0/1/2` 三种模式编译结果（warning/error）。
  - 板端验证三模式行为与 drop 字段语义是否符合预期。

## 2026-02-27 Pitch Adaptive Gravity Feedforward
- Status: In Progress
- Replaced fixed-K gravity feedforward with adaptive LMS model in `gimbal_motor_relative_angle_control`:
  `I_ff = K_hat * cos(theta) + b_hat`, total current composed after speed-loop output `I_pid`.
- Added quasi-static gated online update of `K_hat/b_hat` using residual `I_pid`, with:
  speed/angle/saturation gating, per-cycle max-step limit, and parameter clamp.
- Removed pitch feedforward dependence on `shoot_control.bullet_fired_count` (counter remains in shoot module but no longer used for compensation).
- Kept `PITCH_ENCODE_RELATIVE_PID` at verified values: `Kp=24.0`, `Ki=0.0`, `Kd=0.0`, `max_out=10.0`, `max_iout=0.5`.
- Pending:
  - Verify empty-mag startup convergence time and post-convergence holding current reduction.
  - Verify adaptive tracking under variable ammo load and repeated motion disturbance.

## 2026-02-27 Pitch Adaptive Gravity Feedforward Deadlock Fix (7b)
- Status: In Progress
- Removed angle-error gate from adaptive learning condition to break pinned-state deadlock:
  learning now requires only low-speed + non-saturation.
- Updated adaptive gains for faster recovery after load jump:
  `PITCH_FF_GAMMA_K/B: 0.0005 -> 0.005`,
  `PITCH_FF_ALPHA_DOT_MAX: 5.0 -> 50.0`,
  and removed `PITCH_FF_ERR_TH`.
- Purpose: avoid “FF不足 -> 推不动 -> 误差大 -> 不学习”的锁死链路 in loaded pitch-down condition.
- Pending:
  - Verify loaded fallback recovery time and final steady-state error.
  - Verify full-stick up/down reachability after adaptive convergence.

## 2026-02-27 Pitch FF Decouple + Telemetry (7c)
- Status: In Progress
- In `gimbal_motor_relative_angle_control` (pitch path), FF and PID are now composed into
  `I_total` and hard-clamped to `±16000` before current output conversion.
- Adaptive states are now observable at module boundary:
  `get_pitch_ff_K_hat()` and `get_pitch_ff_b_hat()`.
- FireWater frame is extended with two FF channels:
  `55:ff_k_hat` (milli-scale), `56:ff_b_hat` (raw).
- Pending:
  - Validate parser alignment on MCP/VOFA+ side for the new 57-column frame.
  - Verify loaded full-stick up/down reachability and check if `pitch_cur` frequently hits clamp.

## 2026-02-27 Pitch FF Adaptive Gamma (7d)
- Status: In Progress
- Replaced fixed LMS learning gains (`PITCH_FF_GAMMA_K/B`) with adaptive gamma schedule:
  `gamma = PITCH_FF_GAMMA_BASE + PITCH_FF_GAMMA_ERR_GAIN * |angle_err|`,
  clamped by `PITCH_FF_GAMMA_MAX`.
- LMS update now uses:
  `dk = gamma * error * cos_theta`,
  `db = gamma * error`,
  while keeping `PITCH_FF_ALPHA_DOT_MAX` step limit unchanged.
- Purpose: accelerate FF convergence in large-error loaded scenarios and reduce
  asymmetry between anti-gravity and pro-gravity motion recovery.
- Pending:
  - Compare loaded anti-gravity recovery time before/after 7d.
  - Check post-convergence current ripple and steady-state error near target.

## 2026-02-27 USBTask Priority Lowering for WiFi DBUS Stability
- Status: In Progress
- In `Src/freertos.c`, `USBTask` priority changed:
  `osPriorityNormal -> osPriorityBelowNormal`.
- Goal: avoid DBUS false-offline alarms when `TELEM_MODE_WIFI` uses blocking
  `HAL_UART_Transmit(&huart1, ..., 100u)` in telemetry path.
- Unchanged:
  - `detect_task` offline threshold and timeout logic.
  - `HAL_UART_Transmit` blocking send behavior.
  - `USBTask` stack size (`512`).
- Pending:
  - Build + flash in Keil.
  - `TELEM_MODE_WIFI` run for 5 minutes and confirm no intermittent DBUS buzzer alarm.
