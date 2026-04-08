# 调参记录: vt03_uart_integration

日期: 2026-03-01

---

## 19:31:46 | VT03/VT13 遥控器 UART 接入调通：修复 CRC 算法、接线交叉、通道映射

### 参数变更

**调前:**
```
CRC: 自定义 CRC-16/CCITT-FALSE 表 (table[1]=0x1021, 左移算法)
接线: VT03 TX→C板 TXD, VT03 RX→C板 RXD (颜色匹配, 未交叉)
通道: ch[2]=bit22 (左水平), ch[3]=bit33 (左垂直)
诊断 detect_hook: 在 USART6_IRQHandler RXNE 分支中
```

**调后:**
```
CRC: 复用裁判系统 verify_CRC16_check_sum() (CRC-16 反射型, table[1]=0x1189, 右移算法)
接线: VT03 TX→C板 RXD, VT03 RX→C板 TXD (信号线交叉)
通道: ch[2]=bit33 (左水平/yaw), ch[3]=bit22 (左垂直/pitch) — 交换对齐 DT7
诊断 detect_hook: 已删除, 仅保留 vt03_frame_decode() 内的正式 hook
```

### 效果对比

修复前 (CRC跳过): vt03_toe pp=1.0 偶尔掉线, rc_s0 std=0.23 不稳定
修复后 (裁判系统CRC): vt03_toe pp=0 零掉线, rc_s0 std=0 完全稳定
通道交换后: rc_ch2 pp=1320 (yaw), rc_ch3 pp=1320 (pitch) 全范围响应正确
参考 CSV: tune_2026-03-01_005.csv (CRC修复后首次验证), tune_2026-03-01_006.csv (通道交换后最终验证)

### 证据文件

- [data/tune_2026-03-01_005.csv](../../data/tune_2026-03-01_005.csv)
- [data/tune_2026-03-01_006.csv](../../data/tune_2026-03-01_006.csv)

### 结论

VT03 UART 接入完成，三个问题已修复:
1. 接线: VT03 与 C板 UART1 的 TX/RX 需交叉连接 (VT03-TX→PG9/RXD, VT03-GND→GND)
2. CRC: VT03 使用与裁判系统相同的 CRC-16 反射型 (CRC-16/MCRF4XX), 非手册描述的 CCITT-FALSE; 已删除自定义 CRC 表, 直接复用 CRC8_CRC16.c 的 verify_CRC16_check_sum()
3. 通道映射: VT03 的 bit22-32 对应左垂直(pitch/ch[3]), bit33-43 对应左水平(yaw/ch[2]), 与 DT7 相反, 需交换

修改文件: vt03_link.c (CRC+通道), referee_usart_task.c (删诊断hook)
遗留: 右摇杆和键鼠功能待后续测试验证

