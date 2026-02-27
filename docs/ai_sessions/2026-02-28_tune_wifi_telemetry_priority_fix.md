# 调参记录: wifi_telemetry_priority_fix

日期: 2026-02-28

---

## 00:45:00 | 降低 usb_task 优先级修复 WiFi 模式下 DBUS 假离线，并验证三模式互斥开关 + 双通道 drop_cnt

### 参数变更

**调前:**
```
usb_task priority: osPriorityNormal
drop_cnt: 单一全局计数器 usb_debug_drop_cnt
TELEM_OUTPUT_MODE: 无（USB_DEBUG_OUTPUT_ENABLE 开关）
```

**调后:**
```
usb_task priority: osPriorityBelowNormal (freertos.c:179)
drop_cnt: usb_debug_drop_cnt + wifi_debug_drop_cnt 双通道独立计数
TELEM_OUTPUT_MODE: 三模式互斥 (NONE=0, USB=1, WIFI=2)，当前设为 WIFI
```

### 效果对比

WiFi 采集 135帧/3s, drop=0, dbus_toe=0；USB 采集 149帧/3s, drop=7816(历史累积), 命令通道正常；优先级修复前 WiFi 模式蜂鸣器间歇报警(DBUS 30ms超时)，修复后完全消除

### 结论

1. 三模式互斥开关工作正常，USB/WiFi/NONE 三种模式编译切换无冲突
2. 双通道 drop_cnt 各自独立计数，WiFi 模式 drop=0 验证通过
3. usb_task 降优先级彻底解决 HAL_UART_Transmit 阻塞导致的 DBUS 假离线
4. WiFi 遥测稳定 ~45Hz，满足调参需求
5. 后续建议：长跑测试 WiFi 稳定性（>30min），观察是否有 UDP 丢包或 ESP32 断连

