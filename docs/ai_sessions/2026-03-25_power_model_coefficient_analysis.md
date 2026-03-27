# 2026-03-25 功率模型系数分析与改进建议

## 1. 分析目的

对比 `main` 分支原参数与 `codex/power-limit-ui` 分支新参数的功率预测差异，
评估离线拟合方法的可靠性，给出下一步改进建议。

## 2. 数据口径说明（最重要）

### 2.1 CSV 中 i1-i4 是限流后的值

代码流程（`chassis_task.c:654-667`）：

```
PID_calc() → motor_speed_pid[i].out (原始 PID 输出)
  → chassis_power_control() 改写 .out (限流/应急缩放后)
    → give_current = (int16_t)(.out)
      → CAN 发送 + 遥测 snapshot
```

所以 CSV 中 `i1-i4` = `give_current` = **功率控制之后**的值。

### 2.2 CSV 中 model_power 是限流前的值

在 `chassis_power_control.c:76-88`，`power_est` 是在限流改写 `.out` 之前记录的：

```c
for (i = 0; i < 4; i++) {
    current = motor_speed_pid[i].out;  // ← 原始 PID 输出
    motor_power[i] = motor_power_estimate(current, speed_rpm);
    if (motor_power[i] > 0.0f)
        total_power += motor_power[i];  // ← 只累加正功率
}
ctrl->power_est = total_power;  // ← 限流前记录
// 然后才改写 .out
```

### 2.3 口径对应关系

| CSV 列 | 含义 | 时序 | 适合做什么 |
|---|---|---|---|
| `i1-i4` | 限流后电流命令 | 限流后 | seg1-7 (限流关闭) 时等于原始 PID 输出，可拟合；final_check (限流开启) 时不等于原始值，不可拟合 |
| `model_power` | 板端功率预测(正功率累加) | 限流前 | 反映板端模型行为，但不能与 i1-i4 反算值直接对比 |
| `scap_power_x100` | 超电模块功率读数 | 独立测量 | 低中负载近似可信(±10% 经验)，高负载系统性低报 |
| `buf_pid_fdb` | 裁判 buffer 能量 | 独立测量 | buffer 变化时可反推平均实际功率 |
| `model_max` | effective_limit (÷100) | 限流前 | `-1` 表示限流禁用 |

### 2.4 MCP 解码

`frame_parser.py` 中 `CENTI_COLS = {71, 72, 73, 74}` 对应
`model_power / model_max / model_input / buf_pid_fdb`，MCP 服务器自动 ÷100。
所以 CSV 中这 4 列的值就是真实物理量（W 或 J）。

`scap_power_x100` 不在 CENTI_COLS，需手动 ÷100。

## 3. 系数对比

| 参数 | main (原) | raw_fit (scap) | corr×1.1 | 分支 (Codex) | 说明 |
|---|---|---|---|---|---|
| kt (扭矩) | 1.997e-6 | 1.992e-6 | 2.191e-6 | **2.16e-6** | 最稳定，各方法差 <10% |
| k2 (转速损耗) | 1.453e-7 | 2.236e-7 | 2.460e-7 | **2.09e-7** | 新值比原值高 44% |
| a (铜损) | 1.23e-7 | 1.490e-7 | 1.639e-7 | **1.83e-7** | 新值比原值高 49% |
| c (常数) | **4.081** | 1.513 | 1.665 | **2.21** | 新值比原值低 46% |

分支系数介于 raw_fit 和 corr×1.1 之间，不完全等于任何一版拟合输出。

## 4. 拟合数据总览

### 4.1 7 段拟合数据（seg1-7, 功率限流关闭）

这 7 段录制时 `model_max = -1`（限流禁用标志），因此 `i1-i4 = 原始 PID 输出`，
适合用于拟合。

| 段名 | 样本 | scap 均值 | buf 变化 | 工况 |
|---|---|---|---|---|
| seg1 static spin45 | 450 | 3.8W | 60→60J | 静止，轮子不转 |
| seg2 spin45 only | 426 | 26.1W | 60→60J | 纯自旋 |
| seg3 spin45 low fwd | 421 | 27.7W | 60→60J | 自旋+低速前进 |
| seg4 spin45 mid fwd | 419 | 20.9W | 60→60J | 自旋+中速前进 |
| seg5 spin45 high mix | 409 | 44.8W | 60→60J | 自旋+高速混合 |
| seg6 spin65 only | 422 | 40.8W | 60→60J | 纯高速自旋 |
| seg7 spin65 high mix | 411 | 60.8W | 60→60J | 高速自旋+高负载混合 |

**Buffer 全程 60J 说明**：没有持续超限到让 buffer 掉下来，但不能保证每一帧瞬时功率都在限制以下。
这 7 段适合拟合「未掉 buffer 区」的模型形状，不适合给高负载绝对精度定锚。

### 4.2 final_check（功率限流开启）

- tick: 2365976 → 2373805ms, 总时长 **7.829s**
- buffer: **53J → 1J**
- scap_power 均值: 87.7W
- model_power (板端, 正功率累加, 限流前) 均值: 136.3W

Buffer 反推平均实际功率：`120 + 52/7.829 ≈ 126.6W`

**注意**：final_check 的 `i1-i4` 是限流后的值，不能用来反算模型预测再与
板端 `model_power` 放同一行对比——这是两个不同口径的数据。

### 4.3 scap_power 低报问题

| 负载 | scap_power | 参考真值 | 偏差 | 证据强度 |
|---|---|---|---|---|
| 低中 (seg1-7) | 3.8~60.8W | — | ~10% 低 | **弱**（经验假设，无 buffer 锚点） |
| 高 (final_check) | 87.7W | ~126.6W (buffer推) | **31% 低** | **强**（有 buffer 变化支撑） |

「低中负载 scap 低报 10%」在这批数据中并未被严格证明，因为 seg1-7 buffer 没掉，
无法用 buffer 做真值锚点。这更像是经验估计。

高负载 31% 低报是这批数据中唯一有 buffer 定量支撑的结论。

## 5. 拟合方法评价

### 5.1 方法回顾

`tools/power_model_fit.py` 做的是线性最小二乘：

```
P_total = kt × Σ(I_i × rpm_i) + k2 × Σ(rpm_i²) + a × Σ(I_i²) + 4c
```

对 scap_power 做 `numpy.linalg.lstsq()` 拟合。

### 5.2 拟合质量

| 方案 | N | RMSE | R² | 标签 |
|---|---|---|---|---|
| 原参 (Motor-modeling) | 2958 | 8.89W | 0.871 | 对 scap |
| raw_fit (scap 原值) | 2958 | 4.68W | 0.964 | scap |
| corr×1.1 (scap×1.1) | 2958 | 5.15W | 0.964 | scap×1.1 |

raw_fit 的 R²=0.964 说明线性模型在 seg1-7 范围内形状是好的。

### 5.3 方法局限

1. **标签不可靠**：scap_power 在高负载低报 ~31%，任何基于 scap 的拟合都会在高负载区被带偏。
2. **无高负载训练样本**：seg1-7 最高到 ~60W（scap），真正需要限流的 >100W 区间完全没有覆盖。
3. **final_check 不能直接用于拟合**：其 i1-i4 已被限流改写，输入特征失真。
4. **seg1 R² 为负**：静态段只有常数项起作用，任何模型都很难拟合 3.8W 实测（常数项 + a×Σ(I²) 天然高估）。

### 5.4 常数项差异根因

原参 c=4.081W/电机 → 四轮空载预测 16.3W，而 scap 实测只有 3.8W。

可能原因：
- Motor-modeling-and-power-control 的标定条件不同（不同车、不同 ESC 固件版本、不同测量设备）
- 原标定可能包含了整车电子设备底功耗，而 scap 只量到电机侧
- 电机使用寿命差异（轴承磨损改变摩擦特性）

但 kt 基本一致 (~2.0e-6) 说明电机机械特性没有本质改变，差异主要在损耗项。

## 6. 板端 positive-only 累加策略

`chassis_power_control.c:82-86` 只累加 `motor_power[i] > 0` 的电机。

这意味着：
- 制动/回馈电机的功率被忽略
- 设计假设：负功率支路的回馈不计入供电侧总耗能

这个假设是否成立，取决于 C620 ESC 的真实回馈效率：
- 若回馈效率高：母线净功率低于正功率之和，positive-only 会高估 → 偏保守
- 若回馈效率低（能量在 ESC 内部耗散）：positive-only 接近母线实际功耗 → 合理

当前只能说「这是一个需要验证的模型假设」，不能直接判定为 bug。

## 7. Buffer PID 响应分析

参数：`Kp=2.0, Ki=0.1, Kd=0.0, max_out=40, max_iout=20`
Setpoint：50J, Emergency：20J

当模型系统性低估时，buffer PID 是实际守住安全线的最后一层。
但 final_check 中 buffer 从 53J 一路掉到 1J，说明 PID 修正速度不足以弥补模型误差。

方向性分析（非严格推导）：
- 误差 = 50 - buf。当 buf=30 时，Kp 贡献 = 2.0×20 = 40W（已触及 max_out）
- effective_limit = 120 - 40 = 80W
- 但如果模型预测 80W（positive-only, 限流前），实际功率 >120W，限流后仍超限

核心矛盾：**模型低估 → 限流不够 → buffer 持续下降 → PID 被逼到饱和仍不够**

## 8. 综合结论

### 可以确定的

1. scap_power 作为高负载拟合标签不可靠（final_check 低报 31%，有 buffer 证据）
2. 原参常数项 (4.081W) 对本车空载预测严重偏高（预测 16.3W vs 实测 3.8W）
3. 分支新参数在低中负载预测更贴近 scap（R² 从 0.871 提升到 0.964）
4. 任何纯 scap 拟合的系数在高负载都有低估风险
5. final_check 的 i1-i4 是限流后的值，不能混用

### 不能确定的

1. 低中负载 scap 的精确偏差（10% 只是经验假设，无 buffer 锚点）
2. positive-only 策略是高估还是低估（取决于 C620 回馈效率）
3. 所有方案是否稳定低估 25-32%（只有一段 final_check 高负载数据）

## 9. 改进建议

### 方案 A: 在线校正因子 —— ❌ 不可行

~~用裁判实测 chassis_power 与模型预测的比值做 EMA 在线校正。~~

**已废弃**：当前赛季裁判系统不再实时反馈底盘功率（`chassis_power` 字段不更新），
`get_chassis_power_and_buffer()` 返回的 `chassis_power` 不可用。
`chassis_power_buffer` (buffer 能量) 仍可用，buffer PID 继续有效。

### 方案 B: 提高 buffer PID 激进度（推荐，比赛可用）

```c
// chassis_task.c
const static fp32 buffer_energy_pid[3] = {3.0f, 0.15f, 0.0f};  // Kp 2→3, Ki 0.1→0.15
PID_init(..., 50.0f, 25.0f);  // max_out 40→50, max_iout 20→25
```

同时降低 setpoint: `BUFFER_ENERGY_SETPOINT 50→45`

优点：不改模型结构，纯 PID 调参，与原参数配合最安全。
缺点：不解决模型低估根因，只是让安全网反应更快；日常动力略弱。

### 方案 C: 补采高负载标定数据

1. 在限流关闭状态下，执行短时高负载操控（急加速、急转向）
2. 同时录 i1-i4（此时 = 原始 PID 输出）+ buf_pid_fdb
3. 用 buffer 变化段反推实际功率作为标签
4. 将这些高负载样本加入 lstsq 拟合

优点：直接解决「高负载区无训练数据」问题。
缺点：需要额外采集，且限流关闭状态下高负载可能触发裁判超功率罚电。

### 推荐路径（比赛紧急方案）

**V1 保守版（默认）**：回退到原参数 + main buffer PID，已验证不会 buffer 归零
**V2 加固版（备用）**：原参数 + 方案 B 激进 PID，应对高摩擦场地
**赛后**：方案 C 补采高负载数据，重新标定

详见 `2026-03-25_codex_power_limit_improvement.md`

## 10. 附件

- 拟合脚本: `tools/power_model_fit.py`
- 拟合结果: `data/power_model_fit_2026-03-24.json`
- 7 段拟合数据: `data/tune_2026-03-24_15*_power_fit_seg*.csv`
- 高负载校验数据: `data/tune_2026-03-24_153919_power_fit_final_check_wifi.csv`
- 分支代码: `application/chassis_power_control.c/h` (codex/power-limit-ui)
