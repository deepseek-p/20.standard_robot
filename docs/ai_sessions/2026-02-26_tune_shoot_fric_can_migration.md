# 调参记录: shoot_fric_can_migration

日期: 2026-02-26

---

## 21:14:04 | 摩擦轮从PWM开环改为M3508+C620 CAN闭环速度控制，全部验证通过

### 参数变更

**调前:**
```
摩擦轮: PWM开环控制 (TIM1 CH1/CH2, bsp_fric.c)
拨弹电机: M2006 CAN ID 0x207 (不变)
```

**调后:**
```
摩擦轮: M3508+C620 CAN闭环速度PID
  fric1: hcan2 ID 0x201, 正转
  fric2: hcan2 ID 0x202, 反转
  PID: Kp=10.0, Ki=0, Kd=0, max_out=9900
  目标转速: FRIC_SPEED_LOW=4900 RPM
拨弹电机: M2006 CAN ID 0x207 (不变)
```

### 效果对比

1. CAN通信验证(tune_001): fric1_rpm≈0, fric2_rpm=0, 电机静止正常
2. PID收敛验证(tune_001): fric1≈+4878 fric2≈-4861, 误差<40RPM, std≈22
3. 振荡检测: 18Hz/92RPM峰峰值, 仅1.9%机械噪声, 非PID振荡
4. 装弹实射(tune_002): 弹道正常不偏, trigger_cur有明显负载
5. 失联保护(tune_003): 遥控器关闭后dbus_toe=1, fric_set归零, 摩擦轮自然滑停

### 证据文件

- [data/tune_2026-02-26_001.csv](../../data/tune_2026-02-26_001.csv)
- [data/tune_2026-02-26_002.csv](../../data/tune_2026-02-26_002.csv)
- [data/tune_2026-02-26_003.csv](../../data/tune_2026-02-26_003.csv)

### 结论

摩擦轮CAN闭环改造成功。PID参数Kp=10纯P控制即可满足需求，转速稳定。后续可进行键鼠模式测试（Q切换/C高射频/F调速）和不同速度档位验证。

