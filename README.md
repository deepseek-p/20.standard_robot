# 20.standard_robot

基于 `STM32F407 + FreeRTOS (CMSIS-RTOS v1)` 的 RoboMaster 标准步兵固件工程，包含底盘、云台、发射、裁判系统、遥测调参和板级驱动等模块。

## 项目概述

- 目标平台：`STM32F407`
- RTOS：`FreeRTOS (CMSIS-RTOS v1)`
- 工程入口：`MDK-ARM/standard_robot.uvprojx`
- CubeMX 配置：`standard_robot.ioc`

## 开发环境

- `Keil uVision`：编译、下载、调试
- `STM32CubeMX`：外设初始化代码生成
- `PowerShell / Git`：日常脚本、版本管理与分支协作

## 仓库结构

- `application/`：机器人业务逻辑、FreeRTOS 任务、协议处理与控制模块
- `bsp/boards/`：板级驱动封装，文件命名使用 `bsp_*.c/h`
- `components/`：公共算法、控制器、设备驱动与支撑工具
- `Src/` 与 `Inc/`：STM32CubeMX 生成的 HAL 初始化、ISR 和系统配置
- `Drivers/` 与 `Middlewares/`：STM32 HAL、CMSIS、FreeRTOS、USB 中间件
- `MDK-ARM/`：Keil 工程文件、启动文件与编译配置
- `docs/`：AI 协作交接文档、风险日志、会话记录、设计与实施计划

## 编译与烧录

1. 在 Keil 中打开 `MDK-ARM/standard_robot.uvprojx`
2. 按 `F7` 编译
3. 使用 Download 按钮烧录到开发板
4. 如需清理 Keil 产物，在仓库根目录运行 `.\keilkilll.bat`
5. 如需重新生成 HAL 初始化代码，使用 `STM32CubeMX` 打开 `standard_robot.ioc`，生成后回到 Keil 重新编译

## Wi-Fi 串口调参检查清单

1. ESP32 端口检查
   - 运行：`Test-NetConnection 192.168.4.1 -Port 3333`
   - 预期：返回 `True`
2. PC 连接
   - 运行：`ncat 192.168.4.1 3333`
3. 遥测检查（`STM32 -> ESP32 -> TCP`）
   - 连接后 2 秒内应看到类似：
     `tlm t=12345 err=... out=... ref=...`
   - 如果没有输出，请检查：
     - UART1 接线是否正确：`PA9 TX`、`PB7 RX`
     - 波特率是否为 `115200`
     - 是否共地
4. 命令回显与响应检查
   - `show`
     - 预期先看到 `OK recv:show`，随后输出键值列表
   - `get gimbal_yaw.kp`
     - 预期先看到 `OK recv:...`，随后输出 `gimbal_yaw.kp=...`
   - `set gimbal_yaw.kp 4000`
     - 预期先看到 `OK recv:...`，随后输出 `OK gimbal_yaw.kp=...`
   - `panic`
     - 预期先看到 `OK recv:panic`，随后输出 `OK panic`
5. 收发线对调判定
   - 如果能看到遥测，但命令没有回显，优先对调 TX/RX 线

补充说明：

- UART1 由 `Src/usart.c` 中的 `MX_USART1_UART_Init` 配置为 `115200 8N1`
- 若返回 `ERR value_out_of_range`，表示设置值被限幅；可再执行一次 `get` 查看最终生效值

## 文档入口

- 总索引：[`docs/INDEX.md`](docs/INDEX.md)
- 当前状态：[`docs/ai_sessions/CURRENT_STATE.md`](docs/ai_sessions/CURRENT_STATE.md)
- 仓库协作规范：[`AGENTS.md`](AGENTS.md)

## 分支说明

- GitHub 仓库首页默认通常展示 `main` 分支
- 如果首页显示的更新时间较旧，不代表最新开发没有推送，可能只是最新提交还在其他开发分支上
- 查看最新改动时，请切换到对应开发分支，或等待该分支合并回 `main`
