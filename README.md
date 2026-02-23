# 20.standard_robot

## Wi-Fi UART Param Checklist

1) ESP32 port check:
   - `Test-NetConnection 192.168.4.1 -Port 3333` -> True
2) PC connect:
   - `ncat 192.168.4.1 3333`
3) Telemetry check (STM32 -> ESP32 -> TCP):
   - within 2 seconds you should see lines like:
     `tlm t=12345 err=... out=... ref=...`
   - if missing: check UART1 wiring (PA9 TX, PB7 RX), baud 115200, and common GND.
4) Command echo + response:
   - `show` -> `OK recv:show` then key/value lines
   - `get gimbal_yaw.kp` -> `OK recv:...` then `gimbal_yaw.kp=...`
   - `set gimbal_yaw.kp 4000` -> `OK recv:...` then `OK gimbal_yaw.kp=...`
   - `panic` -> `OK recv:panic` then `OK panic`
5) TX/RX swap rule:
   - if telemetry appears but commands do not echo, swap TX/RX wires.

Notes:
- UART1 is configured in `Src/usart.c` by CubeMX (MX_USART1_UART_Init) at 115200 8N1.
- `ERR value_out_of_range` means the value was clamped; run `get` to confirm the applied value.

## AI 交接文档入口

- 总索引：[`docs/INDEX.md`](docs/INDEX.md)
- 当前状态：[`docs/ai_sessions/CURRENT_STATE.md`](docs/ai_sessions/CURRENT_STATE.md)
