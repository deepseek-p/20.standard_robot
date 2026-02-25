# 2026-02-25 pitch encode pid kp24 ki0 finalize

## 1. 目标

- 将在线调参已验证通过的 pitch ENCONDE 角度环参数固化到代码，避免每次上电重复调参。

## 2. 上轮验收结论（先回填）

- 上轮会话：`docs/ai_sessions/2026-02-25_pitch_encode_ecd_lpf_damping.md`
- 数据来源：`口述`
- 结论：
- `ecd差分 + 一阶低通` 反馈链路可用且稳定；
- 在线调参得到最优参数：`Kp=24`、`Ki=0`；
- `Ki>0` 在连杆机构静摩擦下会导致积分 windup 振荡。
- 状态：`Passed -> Mitigated（待长跑）`
- 下一步：将最优参数固化并进行上电复核。

## 3. 改动文件列表

- `application/gimbal_task.h`
- `docs/ai_sessions/2026-02-25_pitch_encode_pid_kp24_ki0_finalize.md`
- `docs/risk_log.md`
- `docs/ai_sessions/CURRENT_STATE.md`
- `docs/INDEX.md`

## 4. 关键改动摘要

- `application/gimbal_task.h`
- `PITCH_ENCODE_RELATIVE_PID_KP: 6.0f -> 24.0f`
- `PITCH_ENCODE_RELATIVE_PID_KI: 0.1f -> 0.0f`
- `PITCH_ENCODE_RELATIVE_PID_KD: 0.0f -> 0.0f`（保持）
- `PITCH_ENCODE_RELATIVE_PID_MAX_OUT: 10.0f -> 10.0f`（保持）
- `PITCH_ENCODE_RELATIVE_PID_MAX_IOUT: 2.0f -> 0.5f`

## 5. 改码前三问（6.1）

1. 当前故障的实际执行路径是什么？
- 在 `pitch_mode=ENCONDE` 下，旧默认参数（`Kp=6 Ki=0.1`）导致上电后仍需在线调参，`Ki>0` 触发积分相关振荡。

2. 本次修改命中的是哪条路径？
- 直接命中 `pitch ENCONDE` 角度环宏定义，仅修改 `PITCH_ENCODE_RELATIVE_PID_*`。

3. 如何用数据证明本次修改生效？
- 上电后无需在线调参即可达到已验证指标：
- 稳态误差约 `0.016rad`，动态跟踪误差约 `0.035rad`；
- 无振荡，且 `Ki=0` 下无 windup 诱发摆动。

## 6. 关键决策与理由

- 决策：固化 `Kp=24 Ki=0`。
- 理由：该组合已在在线调参中覆盖 `Kp=4 -> 24` 稳定区间，精度与动态误差均优于旧默认值，且规避连杆静摩擦下积分副作用。

## 7. 实时性影响

- 仅参数常量变更，无新增计算、无新增阻塞、任务周期不变。

## 8. 风险点

- `Ki=0` 在极端工况可能出现小稳态偏差回零变慢。
- `Kp=24` 对机械状态/间隙变化敏感，装配变化后可能需微调。

## ⚠ 未验证假设

- 假设：当前机构状态下，`Kp=24 Ki=0` 在长跑和温升后仍保持同等稳定性。
- 未验证原因：本轮为参数固化，尚未覆盖长时间热态与连续动作工况。
- 潜在影响：热态增益变化可能使误差或电流波动上升。
- 验证计划：执行 10 分钟连续工况采集并复核误差与电流峰值，负责人 `@Codex`。

## 9. 验证方式

- 静态验证：确认宏值已更新为目标参数。
- 待实机验证：上电直接测试静止/轻推/快推，检查是否达到口述验收指标且无需二次在线调参。

## 10. 回滚方式

- 将 `application/gimbal_task.h` 的 `PITCH_ENCODE_RELATIVE_PID_KP/KI/MAX_IOUT` 回退到上轮值（`6.0/0.1/2.0`）。
