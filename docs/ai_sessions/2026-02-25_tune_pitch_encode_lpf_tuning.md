# 调参记录: pitch_encode_lpf_tuning

日期: 2026-02-25

---

## 23:32:09 | ecd差分+一阶低通滤波(alpha=0.05)解决速度反馈问题，Kp=24 Ki=0时稳定，稳态误差约1°，遥控器可正常操控pitch

### 参数变更

**调前:**
```
pitch_encode Kp=6 Ki=0.1 Kd=0 max_out=10 max_iout=2
pitch_speed Kp=800 Ki=10 max_out=15000 max_iout=3000
速度反馈: ecd差分无滤波，低速量化阶梯(0↔0.767rad/s)
结果: 所有Kp+Ki组合均振荡
```

**调后:**
```
pitch_encode Kp=24 Ki=0 Kd=0 max_out=10 max_iout=0.5
pitch_speed Kp=800 Ki=10 max_out=15000 max_iout=3000
速度反馈: ecd差分+LPF(alpha=0.05, ~8Hz截止)
```

### 效果对比

1. Ki=0.1时任何Kp(0.5~6)均振荡→Ki是振荡关键因素(积分windup+静摩擦)
2. Ki=0后Kp从4→8→12→16→24均稳定，稳态误差随Kp增大而减小:
   Kp=4: err=0.146rad, Kp=8: err=0.049rad, Kp=12: err=0.034rad, Kp=16: err=0.026rad, Kp=24: err=0.016rad
3. 遥控器推pitch测试: 动态跟踪误差约0.035rad(2°)，响应正常
4. CSV证据: tune_2026-02-25_003.csv(Kp=4遥控测试), tune_2026-02-25_004.csv(Kp=24遥控测试)

### 证据文件

- [data/tune_2026-02-25_003.csv](../../data/tune_2026-02-25_003.csv)
- [data/tune_2026-02-25_004.csv](../../data/tune_2026-02-25_004.csv)

### 结论

ecd差分+LPF方案成功。Kp=24 Ki=0可用，稳态误差1°可接受。连杆机构静摩擦大，Ki会导致积分windup振荡，暂不使用Ki。后续可考虑: 1)增大alpha提高滤波响应速度 2)尝试极小Ki(0.005)配合更小max_iout 3)将Kp=24写入gimbal_task.h固化

