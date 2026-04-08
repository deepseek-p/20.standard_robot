# 调参记录: keyboard_mouse_vt13

日期: 2026-03-03

---

## 18:48:52 | VT13透传键鼠全键位测试 + 鼠标左右键协议解析修复

### 参数变更

**调前:**
```
vt03_link.c 鼠标按键解析:
  rc_ctrl.mouse.press_l = 0 (硬编码)
  rc_ctrl.mouse.press_r = 0 (硬编码)
  d[14] 鼠标按键字节未解析

detect_task.c 拨轮电机检测:
  error_list[TRIGGER_MOTOR_TOE].enable = 0
  error_list[TRIGGER_MOTOR_TOE].error_exist = 1 (初始化默认值未清除)
  error_list[TRIGGER_MOTOR_TOE].is_lost = 1 (初始化默认值未清除)
```

**调后:**
```
vt03_link.c 鼠标按键解析:
  mb = d[14];
  rc_ctrl.mouse.press_l = (uint8_t)(mb & 0x01u);
  rc_ctrl.mouse.press_r = (uint8_t)((mb >> 1) & 0x01u);
  vt03_ext.mouse_mid = (uint8_t)((mb >> 4) & 0x03u);

detect_task.c 拨轮电机检测:
  error_list[TRIGGER_MOTOR_TOE].enable = 0
  error_list[TRIGGER_MOTOR_TOE].error_exist = 0
  error_list[TRIGGER_MOTOR_TOE].is_lost = 0
```

### 效果对比

tune_2026-03-03_019.csv: 鼠标左键修复后测试，shoot_mode 2→4→5→2 循环15+次，trigger_cur=7000~10000，press_l 正确触发发射
tune_2026-03-03_020.csv: 鼠标右键测试，shoot_mode=0(无力模式下无可观测效果，正常)
tune_2026-03-03_021.csv: CTRL底盘模式切换，ch_mode在0(跟随)和2(小陀螺)间切换，wz_set从1.5→4.5，evt_chassis_mode_changed=1
tune_2026-03-03_022.csv: SHIFT超级电容，key_v=16检测正确
tune_2026-03-03_023.csv: B键UI刷新，key_v=32768(0x8000)检测正确

前次会话已验证: WASD移动、鼠标XY瞄准、Q射击开关、C高频、R连发、G反转、F/Shift+F摩擦轮调速、Z零力模式
VT13按键已验证: fn_1短按(shoot_toggle)、fn_1长按(high_freq)、fn_2短按(burst)、fn_2长按(reverse)、dial拨轮调速、pause超级电容、mode_sw模式切换

### 证据文件

- [data/tune_2026-03-03_019.csv](../../data/tune_2026-03-03_019.csv)
- [data/tune_2026-03-03_020.csv](../../data/tune_2026-03-03_020.csv)
- [data/tune_2026-03-03_021.csv](../../data/tune_2026-03-03_021.csv)
- [data/tune_2026-03-03_022.csv](../../data/tune_2026-03-03_022.csv)
- [data/tune_2026-03-03_023.csv](../../data/tune_2026-03-03_023.csv)

### 结论

全部键鼠功能测试通过:
1. 鼠标左键修复: d[14]协议字节包含鼠标按键数据(bit0=左键,bit1=右键,bit4-5=中键)，之前硬编码为0导致左键无效，改为从协议解析后正常工作
2. 蜂鸣器报警修复: detect_init中enable=0不会清除error_exist(初始化为1)，test_task蜂鸣器检查error_exist而非enable，需额外清除error_exist和is_lost
3. 唯一待验证: VT13 trigger按键和鼠标左键实弹发射(拨轮电机轴松动待修复)
4. 待修复后恢复: detect_task.c中TRIGGER_MOTOR_TOE的enable=0临时禁用需在电机修好后移除

