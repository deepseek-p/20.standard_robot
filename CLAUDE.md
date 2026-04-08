# CLAUDE.md — 项目级规则

## 交互规则（最高优先级）

1. **默认只建议，不改代码。** 除非用户明确说"改吧"、"动手"、"go ahead"等，否则只输出文字分析和建议。
2. **先读再说。** 给任何意见之前，必须先读完相关源文件。禁止靠猜测评价代码行为。
3. **审查模式下报告发现即可。** 不要从分析跳到编辑，等用户确认要改哪些再动手。
4. **用中文交流。**

## 项目上下文

- RoboMaster 标准步兵固件，STM32F407 + FreeRTOS (CMSIS-RTOS v1)
- 编译工具链：Keil MDK (armcc)，工程文件 `MDK-ARM/standard_robot.uvprojx`
- 控制周期：gimbal_task 1ms，chassis_task 2ms
- CAN 总线：
  - hcan1: 底盘 3508 x4 (0x201~0x204) + 云台 yaw 6020 (0x205) + pitch 6020 (0x206)
  - hcan2: 摩擦轮 3508 x2 (0x201/0x202)
- 遥控：SBUS via USART3 DMA 双缓冲
- IMU：BMI088 (SPI1) + IST8310 (I2C)，EXTI 触发 INS_task
- 调试通道：USB CDC (FireWater 遥测 + 在线 PID 命令)
- WiFi 遥测：ESP32 桥接 (USART1)，UDP 遥测 + TCP 命令

## 文档规范

- 改动涉及控制链路时，必须遵守 `docs/00_AI_HANDOFF_RULES.md` 的全部规则
- 硬件映射：`docs/hardware_map.md`（由 `tools/parse_ioc.py` 从 .ioc 自动生成）
- 会话记录模板：`docs/ai_sessions/TEMPLATE.md`
- 当前状态：`docs/ai_sessions/CURRENT_STATE.md`

## 多 AI 协作

- 用户会在 Claude、Codex 等多个 AI 之间传递任务
- 生成交接文档时，明确区分"已测试"和"未测试"的内容
- 标注当前 commit 和分支
- 列出禁止事项（哪些参数不能动、哪些文件不能改）
