# 调参记录: trigger_single_shot

日期: 2026-03-03

---

## 11:34:57 | 从位置PID振荡→纯速度制动→HUST风格柔和持仓，迭代解决单发过冲多发问题；最终因电机轴松动暂停测试

### 参数变更

**调前:**
```
DONE handler: speed_set = PID_enhanced_calc(pos_pid, ecd_fdb, ecd_set) [max_out=20000, full gains]
READY_BULLET: speed_set=0, ecd_set=ecd_fdb (track position), zero current
Zero-current gate: shoot_mode <= SHOOT_READY_BULLET
SHOOT_DONE_KEY_OFF_TIME = 15
Stall handling: trigger_motor_turn_back() with no reverse count limit
Microswitch check: already removed (previous session)
```

**调后:**
```
DONE handler: pos PID with max_out=TRIGGER_POS_MAX_OUT_HOLD (10.0f) [soft hold]
READY_BULLET: pos PID with max_out=10.0f, ecd_set NOT tracked (HUST style)
BULLET: max_out=TRIGGER_POS_MAX_OUT_FIRE (20000.0f) [full torque for firing]
Zero-current gate: shoot_mode <= SHOOT_READY_FRIC (READY_BULLET now has PID active)
SHOOT_DONE_KEY_OFF_TIME = 100
Stall handling: MAX_REVERSE_COUNT=3, reverse_count++ per reversal, exceed → READY_BULLET
New fields: reverse_count (uint8_t) in shoot_control_t
New macros: TRIGGER_POS_MAX_OUT_HOLD=10.0f, TRIGGER_POS_MAX_OUT_FIRE=20000.0f, MAX_REVERSE_COUNT=3
```

### 效果对比

## 方案迭代与数据对比

### 方案1: 位置PID制动 (max_out=20000) — tune_2026-03-02_033.csv
- DONE阶段电流饱和±10000导致bang-bang振荡
- 每发过冲0.4~0.87格，DONE退出时速度-5~-7 rad/s（振荡过零触发退出）
- 12发中bullet_cnt均+1（软件正确），但物理上"有时1颗有时2颗"
- 关键数据: Shot2 overshoot=+0.778格, Shot5=+0.865格

### 方案2: 去掉ecd_set追踪(不track) — tune_2026-03-02_034.csv
- READY_BULLET不追踪ecd_set导致位置误差累积
- BULLET入口pos_err高达58000~64000 (1.5~1.7格)
- 每发实际行程2.7~3.0格，严重过冲
- 比方案1更差

### 方案3: 纯速度制动 (speed_set=0) — tune_2026-03-02_035.csv
- 电机平稳减速到0（无振荡），但无位置回拉
- 从25 rad/s惯性滑行2.1~3.0格
- 12发中每发overshoot 1.1~2.0格
- 比方案1更差

### 方案4 (当前): HUST风格柔和持仓 (max_out=10)
- DONE/READY_BULLET: 位置PID max_out=10 → 最大速度指令10 rad/s → 最大电流~2000
- BULLET: max_out=20000 全力矩推弹
- 理论优势: 不饱和不振荡，有位置回拉，发射力矩不受限
- 测试中发现卡弹时反复正反转导致电机轴松动，暂停测试

### 级联增益分析
- 我们: pos Kp=0.4 × spd Kp=200 = 80 (高增益→易饱和振荡)
- HUST: pos Kp=0.4 × spd Kp=6.2 = 2.48
- HUST方案在低增益下天然稳定，我们需要通过动态max_out限幅适配

### 证据文件

- [data/tune_2026-03-02_033.csv](../../data/tune_2026-03-02_033.csv)
- [data/tune_2026-03-02_034.csv](../../data/tune_2026-03-02_034.csv)
- [data/tune_2026-03-02_035.csv](../../data/tune_2026-03-02_035.csv)

### 结论

## 当前状态
代码已改为HUST风格（位置PID始终运行+动态max_out），但电机轴因反复正反转测试松动，无法继续实弹验证。

## 待办
1. 【硬件】修复/更换拨弹电机轴（松动）
2. 【测试】修复后验证HUST方案的单发精度（预期过冲<0.3格）
3. 【测试】满弹状态下验证READY_BULLET持仓电流是否可接受（max_out=10 → ~2000电流）
4. 【调参】如果持仓仍有小幅振荡，可尝试降低TRIGGER_POS_MAX_OUT_HOLD（10→5）
5. 【调参】如果BULLET发射力矩不足，确认max_out=20000恢复正常
6. 【机械】考虑拨弹机构的机械阻力问题——HUST方案要求空载/轻载时位置PID能收敛

## 禁止修改
- TRIGGER_ONEGRID = 36864.0f
- 速度环PID参数 (Kp=200, Ki=3.0)
- 位置环PID参数 (Kp=0.4, Ki=0.02)
- 连发模式 (CONTINUE_BULLET) 逻辑

