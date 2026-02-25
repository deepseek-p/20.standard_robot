# 调参记录: pitch_encode_speed_feedback

日期: 2026-02-25

---

## 22:53:07 | ecd差分速度反馈替代speed_rpm，解决了振荡但低速量化阶梯导致残余振荡，需加低通滤波

### 参数变更

**调前:**
```
pitch速度环反馈: GM6020 speed_rpm (整数RPM，低速=0)
pitch_encode Kp=6 Ki=0.1 Kd=0 max_out=10 max_iout=2
pitch_speed Kp=800 Ki=10 max_out=15000 max_iout=3000
```

**调后:**
```
pitch速度环反馈: ecd差分 (diff*0.76699 rad/s，无滤波)
pitch_encode Kp=3 Ki=0.1 Kd=0 max_out=10 max_iout=2
pitch_speed Kp=400~800 Ki=10 max_out=15000 max_iout=3000
```

### 效果对比

1. speed_rpm方案: Kp=3稳定但gyro全程=0(低速RPM报0)，pitch卡在限位推不回来(电流-10719仍不动)
2. ecd差分方案: gyro有值了(±1570)，pitch能跟踪目标(rel≈set)，但低速时gyro仍有大量=0帧
3. 量化问题: 1ms周期×8192counts/rev，低速时diff=0或1，速度跳变0↔0.767rad/s
4. 残余振荡: rel在-268~+54间摆动(pp=0.29rad)，周期约0.8s，与量化阶梯导致的速度环阻尼缺失一致

### 证据文件

- [data/tune_2026-02-25_001.csv](../../data/tune_2026-02-25_001.csv)

### 结论

ecd差分方向正确但需一阶低通滤波(alpha=0.05, ~8Hz截止)平滑量化阶梯。下一步: Codex#5c加滤波后重新调参。

