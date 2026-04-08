# Shoot HUST 控制核心 — 板端验证与修复

日期：2026-03-07
分支：shoot-state-machine-align-hust
状态：Passed（空载 + 实弹）

## 背景

2026-03-07 早些时候，按 `docs/plans/2026-03-07-shoot-hust-core-replacement-impl.md` 完成了 HUST 控制核心替换（由 Codex 执行）。本会话对替换后的固件进行板端验证，发现并修复了两个问题，最终通过全部测试项。

## 测试工具

- USB CDC 遥测（COM22）
- `stm32-telemetry-mcp` Python MCP 服务器（`capture(port='COM22', duration_sec=N)`）
- VT13 遥控器（通过 VT03 link + keyboard_action 接入）

## 测试结果

### 1. STOP 基线 — PASS
- `shoot_mode=0` 全程稳定
- `trigger_ecd_fdb = trigger_ecd_set`，零偏差
- 所有电流/速度为零

### 2. Enable → Ready — PASS
- 模式转换 0→1→2 正确
- 摩擦轮 ~120ms 达到 4900 rpm 目标

### 3. 单发（空载）— PASS（修复后）
- 模式转换 2→3→2 正确
- `ecd_set` 步进 36864（TRIGGER_ONEGRID）
- `bullet_cnt` 正确递增
- **修复前**：到位后 ecd_fdb 振荡 ±4000，电流 ±10000 bang-bang（根因：Butterworth 滤波器相位滞后）
- **修复后**：avg_err=217，稳定无振荡

### 4. 连发（空载）— PASS（修复后）
- 模式转换含 mode=4（CONTINUE_BULLET）
- `speed_set=3500.0`（PULLER_SPEED_NORMAL）正确
- 松开后锁位回 READY，avg_err=229
- **修复前**：VT13 长按无法触发连发（根因：keyboard_action 的 25ms 脉冲扩展 < 100ms 长按阈值）
- **修复后**：使用 `vt03_ext.trigger` 原始持续状态，长按正常触发

### 5. 反转（空载）— PASS
- `ecd_set` 步进 -36864（反向一格）
- `reverse_flag` 在 BULLET 期间为 1，到位后清零
- `bullet_cnt` 不变（反转不计弹丸）

### 6. 实弹测试 — PASS
- 单发、连发、反转均正常工作
- 累计发射 137 发（含空载和实弹）
- 无堵转、无异常

## 修复列表

### Fix 1: 移除 Butterworth 速度滤波器
- 文件：`application/shoot.c` `shoot_feedback_update()`
- 改动：删除 2 阶 Butterworth 滤波器（截止 ~20Hz），直接使用 `speed_rpm`
- 原因：滤波器引入 ~15-20ms 相位滞后，导致级联 PID 在到位后振荡
- HUST 参考：不使用速度滤波，直接用 raw rpm

### Fix 2: VT03 trigger 改用原始持续状态
- 文件：`application/shoot.c` `shoot_feedback_update()`
- 改动：`cmd->vt03_trigger`（25ms 脉冲）→ `vt03_ext.trigger`（持续状态）
- 原因：keyboard_action 的边沿检测 + 25ms 扩展窗口短于 100ms 长按阈值，导致连发永远无法触发

### Fix 3: 摩擦轮上限降低
- 文件：`application/shoot.h`
- 改动：`FRIC_SPEED_HIGH` 8000 → 7400
- 原因：参考 HUST 注释（4850rpm≈14.1m/s），8000rpm 估算超 25m/s 弹速限制

## 最终参数

```
速度反馈: raw rpm（无滤波）
位置环: Kp=0.4, Ki=0.02, max_out=20000, max_iout=1500
速度环: Kp=6.2, Ki=3.2, max_out=10000, max_iout=1000, deadzone=50
摩擦轮: LOW=4900, MID=5800, HIGH=7400（限 25m/s）
连发速度: NORMAL=3500rpm, HIGH_FREQ=4500rpm
步进: TRIGGER_ONEGRID=36864, 到位阈值=5000
```

## 遗留

- 热量门控未实弹验证（无裁判系统连接）
- 高频模式（C 键切换 3500↔4500）未专项测试
- 爆发模式（R 键 80↔180 热量上限）未专项测试
- 弹速需用测速器校准确认 4900rpm 实际弹速
