# 拨轮电机控制优化计划

## 执行状态（2026-02-28）

- ✅ Step 1 已完成：`pid_enhanced_t` 与 `PID_enhanced_init/calc/clear` 已落地。
- ✅ Step 2 已完成：拨轮已切为位置+速度双环串级，单发按编码器目标位置闭环。
- ✅ Step 3 已完成：已加入 `local_heat` 预测、裁判值校准、`R` 键爆发模式与双阈值门控。
- ⏳ Step 4 待执行：Keil 编译 + 板端实机验收（单发精度/连发计热/爆发模式门控）。

## 赛事规则（1v1 对抗赛）

- 17mm 发射机构 x1，初始 200 发不可补充
- 弹速上限：25 m/s
- 血量：200，单发伤害 20 → **10 发击杀**
- **热量上限：80**（每发 +10，满热量仅 8 发连射）
- **冷却速率：12/s**（持续射速约 1.2 发/s）
- **超热量(>80) → 锁枪，必须冷却到 0 才解锁（最少 90/12≈7.5s）**
- **严重超限(≥180) → 本局永久锁枪**
- 冷却结算频率 10Hz（每 0.1s 减 1.2）
- 底盘功率：120W
- 无等级系统，参数全程固定

## 对比分析：当前项目 vs HUST_Infantry_2023

### 架构差异

| 维度 | 当前项目 | HUST | 1v1 适配重点 |
|------|---------|------|-------------|
| PID 环路 | 单环（速度） | 双环（位置+速度串级） | 双环提升单发精度 |
| PID 算法 | 基础 PID | 增强 PID（死区/变速积分/不完全微分） | 变速积分防超调 |
| 热量管理 | 裁判系统直读 | 本地预测+裁判校准 | **极关键**，80 上限容错极低 |
| 连发控制 | 固定 15 rad/s | 按等级动态调速 | 改为按热量余量调速 |
| 堵转处理 | 时间判定反转 | 位置环回退 | 堵转=浪费热量窗口 |

### 当前项目关键缺陷

1. **单环速度 PID** — 单发精度差，可能多拨或少拨
2. **PID 无变速积分** — 容易积分饱和导致超调
3. **热量管理被动** — 80 上限下裁判系统延迟可能导致超热量扣血
4. **连发速度固定** — 15 rad/s 不考虑热量余量，容易打空热量窗口
5. **堵转处理慢** — 700ms 检测 + 500ms 反转 = 1.2s 浪费

---

## 优化计划

### Step 1: 增强 PID 控制器

在现有 `pid.c/pid.h` 新增 `pid_enhanced_t` 类型，支持：
- 死区 (DeadZone)
- 变速积分 (I_L / I_U 区间，误差大时停止积分)
- 不完全微分滤波 (RC_DF)
- 梯形积分

不修改现有 `pid_type_def`，避免影响云台/底盘。

文件：`components/controller/pid.h`, `components/controller/pid.c`

### Step 2: 拨轮双环串级控制

- 新增位置环 PID (`trigger_pos_pid`，增强型)
- 速度环改用增强型 PID 作为内环
- 单发：位置环增量 → 速度环跟踪
- 连发：跳过位置环，直接速度环
- 堵转：位置环回退

文件：`application/shoot.h`, `application/shoot.c`

### Step 3: 热量管理优化（1v1 专用）

超热量惩罚极重：锁枪至少 7.5s，严重超限永久锁枪。

核心逻辑（统一规则）：
- 本地维护 Q1 预测值：
  - 每次拨弹成功：Q1 += 10
  - 每个控制周期(1ms)：Q1 -= 12.0/1000.0 = 0.012，下限 0
  - 裁判系统数据到达时：Q1 = 裁判系统值（校准）
- 两种模式（键盘切换）：
  - **安全模式（默认）**：Q1 + 10 ≤ 80 → 允许射击
  - **爆发模式（开关）**：Q1 + 10 < 180 → 允许射击（超 80 会锁枪但不永久）
- 无论单发/连发，每发前都检查
- 摩擦轮速度固定一档（对应 24~24.5 m/s）

文件：`application/shoot.h`, `application/shoot.c`

### Step 4: 验证

- 编译验证
- MCP 遥测在线调参
- 记录结论

---

## 风险与注意事项

- 双环串级需要重新调参，位置环 Kp 不宜过大
- 热量预测的射弹检测依赖摩擦轮转速反馈质量
- 变速积分的 I_L/I_U 区间需要根据实际误差范围调整
- 改动涉及控制链路，需遵守 `docs/00_AI_HANDOFF_RULES.md`
- 摩擦轮速度需要实测标定到 24~24.5 m/s（留余量不超 25）

---

## 实现提示词（分步交给 Codex）

### Prompt 1: 增强 PID 控制器

```
任务：在现有 PID 模块中新增增强型 PID，仅供拨轮电机使用，不修改现有 pid_type_def。

修改文件：
- components/controller/pid.h
- components/controller/pid.c

在 pid.h 的 `extern void PID_clear(...)` 之后、`#endif` 之前，新增：

1. 结构体 pid_enhanced_t，字段：
   - Kp, Ki, Kd (fp32)
   - max_out, max_iout (fp32)
   - dead_zone (fp32) — 误差绝对值小于此值视为 0
   - I_L (fp32) — |error| < I_L 时全量积分
   - I_U (fp32) — I_L ≤ |error| < I_U 时按比例衰减积分；≥ I_U 时不积分
   - RC_DF (fp32) — 不完全微分滤波系数，0~1，越小滤波越强
   - set, fdb, out, out_last (fp32)
   - Pout, Iout, Dout, Dout_last (fp32)
   - error[2] (fp32) — [0]=当前 [1]=上次

2. 三个函数声明：
   - PID_enhanced_init(pid_enhanced_t *pid, fp32 kp, ki, kd, max_out, max_iout, dead_zone, I_L, I_U, RC_DF)
   - PID_enhanced_calc(pid_enhanced_t *pid, fp32 ref, fp32 set) → fp32
   - PID_enhanced_clear(pid_enhanced_t *pid)

在 pid.c 末尾实现这三个函数。PID_enhanced_calc 的计算逻辑：
1. 更新误差：error[1]=error[0], error[0]=set-ref
2. 死区：|error[0]| < dead_zone 则 err=0，否则 err=error[0]
3. P项：Pout = Kp * err
4. 变速积分（梯形）：
   - |err| < I_L：Iout += Ki * (err + error[1]) * 0.5
   - I_L ≤ |err| < I_U：Iout += Ki * (err + error[1]) * 0.5 * (1 - (|err|-I_L)/(I_U-I_L))
   - |err| ≥ I_U：不累加
   - 限幅 Iout 到 ±max_iout
5. 不完全微分：raw_d = Kd * (out - out_last)，Dout = raw_d * RC_DF + Dout_last * (1-RC_DF)
6. 输出：out_last=out, out = Pout+Iout+Dout，限幅 ±max_out

参考 HUST_Infantry_2023/F405_Gimbal/Algorithm/pid.c 的 PID_Calc 函数实现。
使用已有的 LimitMax 宏做限幅。

禁止修改：pid_type_def、PID_init、PID_calc、PID_clear 的任何代码。
```

### Prompt 2: 拨轮双环串级控制

```
任务：将拨轮电机从单环速度 PID 改为位置+速度双环串级控制。

前置依赖：Prompt 1 已完成（pid_enhanced_t 可用）。

修改文件：
- application/shoot.h
- application/shoot.c

=== shoot.h 修改 ===

1. 新增 #include "pid.h"（如果还没有的话）

2. 替换拨轮 PID 宏定义为双环参数：
   删除：TRIGGER_ANGLE_PID_KP/KI/KD, TRIGGER_BULLET_PID_MAX_OUT/IOUT, TRIGGER_READY_PID_MAX_OUT/IOUT
   新增：
   // 拨轮位置环（增强型）
   #define TRIGGER_POS_KP      0.4f
   #define TRIGGER_POS_KI      0.02f
   #define TRIGGER_POS_KD      0.0f
   #define TRIGGER_POS_MAX_OUT 20000.0f
   #define TRIGGER_POS_MAX_IOUT 1500.0f
   #define TRIGGER_POS_DEADZONE 0.0f
   #define TRIGGER_POS_I_L     8000.0f
   #define TRIGGER_POS_I_U     12000.0f
   #define TRIGGER_POS_RC_DF   0.5f

   // 拨轮速度环（增强型）
   #define TRIGGER_SPD_KP      6.0f
   #define TRIGGER_SPD_KI      3.0f
   #define TRIGGER_SPD_KD      0.0f
   #define TRIGGER_SPD_MAX_OUT 10000.0f
   #define TRIGGER_SPD_MAX_IOUT 1000.0f
   #define TRIGGER_SPD_DEADZONE 50.0f
   #define TRIGGER_SPD_I_L     100.0f
   #define TRIGGER_SPD_I_U     200.0f
   #define TRIGGER_SPD_RC_DF   0.5f

   参数来源：HUST_Infantry_2023 的 Pid_BodanMotor_Init()，已验证可用。

3. 新增拨轮位置常量：
   #define TRIGGER_ONEGRID     36864.0f   // 一格弹丸的编码器增量
   #define TRIGGER_POS_THRESHOLD 5000.0f  // 位置到达判定阈值

4. 在 shoot_control_t 结构体中：
   - 将 `pid_type_def trigger_motor_pid` 替换为：
     pid_enhanced_t trigger_pos_pid;   // 位置环
     pid_enhanced_t trigger_spd_pid;   // 速度环
   - 新增字段：
     fp32 trigger_ecd_set;    // 拨轮目标编码器位置
     fp32 trigger_ecd_fdb;    // 拨轮当前编码器位置（连续值）
     bool_t reverse_flag;     // 堵转回退标志

=== shoot.c 修改 ===

1. shoot_init()：
   - 用 PID_enhanced_init 初始化 trigger_pos_pid 和 trigger_spd_pid
   - trigger_ecd_set = trigger_ecd_fdb = 当前编码器连续位置

2. shoot_feedback_update()：
   - 保留现有的 ecd_count 圈数累加逻辑
   - 新增：trigger_ecd_fdb = ecd_count * ECD_RANGE + shoot_motor_measure->ecd
     （连续编码器位置，不转角度，直接用 ecd 值做位置环）

3. shoot_bullet_control()（单发）改为：
   - move_flag==0 时：trigger_ecd_set += TRIGGER_ONEGRID, move_flag=1
   - 位置环计算：speed_set = PID_enhanced_calc(&trigger_pos_pid, trigger_ecd_fdb, trigger_ecd_set)
   - 到达判定：|trigger_ecd_set - trigger_ecd_fdb| < TRIGGER_POS_THRESHOLD 时完成
   - 堵转检测：保留现有逻辑，但堵转时 trigger_ecd_set -= TRIGGER_ONEGRID（回退）

4. SHOOT_CONTINUE_BULLET 模式：
   - 跳过位置环，直接给 speed_set（保持现有连发速度逻辑）
   - 堵转处理保持 trigger_motor_turn_back()

5. shoot_control_loop() 中 PID 计算改为：
   - 非连发模式：speed_set 由位置环输出
   - 所有模式最终：given_current = PID_enhanced_calc(&trigger_spd_pid, speed, speed_set)

禁止修改：摩擦轮相关代码、CAN 通信代码、gimbal_task.c。
```

### Prompt 3: 热量管理 + 爆发模式

```
任务：实现本地热量预测和爆发模式开关，替换现有的简单热量判断。

前置依赖：Prompt 2 已完成。

赛事规则（必须严格遵守）：
- 热量上限 Q0 = 80，每发 +10，冷却 12/s
- Q1 > 80：锁枪，必须冷却到 0 才解锁
- Q1 ≥ 180：本局永久锁枪
- 冷却结算 10Hz（每 0.1s 减 1.2），但我们控制周期 1ms

修改文件：
- application/shoot.h
- application/shoot.c

=== shoot.h 修改 ===

1. 删除 SHOOT_HEAT_REMAIN_VALUE 宏，新增：
   #define HEAT_PER_BULLET     10.0f
   #define HEAT_LIMIT_SAFE     80.0f    // 安全模式上限
   #define HEAT_LIMIT_BURST    180.0f   // 爆发模式上限（严重超限阈值）
   #define HEAT_COOL_RATE      12.0f    // 冷却速率 /s
   #define SHOOT_BURST_KEYBOARD KEY_PRESSED_OFFSET_R  // R键切换爆发模式

   // 全局模式：调试 vs 比赛（编译期开关）
   // 设为 1 = 调试模式（不限热量不限弹速），0 = 比赛模式
   #define SHOOT_DEBUG_MODE    0

2. 在 shoot_control_t 中新增字段：
   fp32 local_heat;          // 本地预测热量 Q1
   uint16_t referee_heat;    // 裁判系统上次同步的热量
   bool_t burst_mode;        // 爆发模式开关（仅比赛模式有效）
   bool_t last_key_r;        // R键上次状态

=== shoot.c 修改 ===

1. shoot_init()：
   - local_heat = 0, burst_mode = 0, last_key_r = 0

2. R键切换（在 shoot_set_mode() 中）：
   - 检测 R 键上升沿（当前按下 && 上次未按下）
   - 翻转 burst_mode
   - 更新 last_key_r
   - 注意：R 键在 gimbal_task.h 定义为 TEST_KEYBOARD 但未使用，无冲突

3. 本地热量维护（在 shoot_control_loop() 开头）：
   - 每周期冷却：local_heat -= HEAT_COOL_RATE / 1000.0f（控制周期1ms）
   - 下限钳位：if (local_heat < 0) local_heat = 0
   - 裁判系统校准：调用 get_shoot_heat0_limit_and_heat0 获取 referee 热量
     如果 referee 值与上次不同，说明裁判系统更新了，用 referee 值覆盖 local_heat
     更新 referee_heat = 当前 referee 值

4. 射击许可判断（替换现有 shoot_set_mode() 中的热量判断）：
   删除原有逻辑：
     get_shoot_heat0_limit_and_heat0(&heat_limit, &heat);
     if (!toe_is_error(REFEREE_TOE) && (heat + SHOOT_HEAT_REMAIN_VALUE > heat_limit))
   替换为：
   #if SHOOT_DEBUG_MODE == 0  // 比赛模式才限热量
     fp32 limit = burst_mode ? HEAT_LIMIT_BURST : HEAT_LIMIT_SAFE;
     if (local_heat + HEAT_PER_BULLET > limit)
     {
         if (shoot_mode == SHOOT_BULLET || shoot_mode == SHOOT_CONTINUE_BULLET)
             shoot_mode = SHOOT_READY_BULLET;
     }
   #endif

5. 拨弹成功时累加热量：
   在 shoot_bullet_control() 中，当单发完成（bullet_fired_count++ 处）：
     local_heat += HEAT_PER_BULLET;
   在 SHOOT_CONTINUE_BULLET 的 trigger_motor_turn_back() 中，
   需要检测拨弹成功（可复用摩擦轮扰动检测或编码器位移检测）：
     简化方案：每转过 TRIGGER_ONEGRID 的编码器增量视为一发，local_heat += HEAT_PER_BULLET

6. 摩擦轮速度：
   保留 FRIC_SPEED_LOW/MID/HIGH 三档和 F/Shift+F 切换逻辑，调试和比赛模式通用。
   不做任何修改。

禁止修改：PID 代码、CAN 通信代码、gimbal_task.c、referee.c。
保留 get_shoot_heat0_limit_and_heat0 调用用于校准，但不再用它做射击许可判断。
```
