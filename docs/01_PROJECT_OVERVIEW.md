# 项目概览与入口

## 1. 项目定位

本仓库是 RoboMaster 标准步兵固件工程，目标平台为 `STM32F407 + FreeRTOS (CMSIS-RTOS v1)`。  
任务创建集中在 `Src/freertos.c`，系统节拍来自 `Inc/FreeRTOSConfig.h`（`configTICK_RATE_HZ = 1000`，即 `1 tick = 1ms`）。

## 2. 关键入口文件

- 工程文件：`MDK-ARM/standard_robot.uvprojx`
- CubeMX 配置：`standard_robot.ioc`
- 系统启动：`Src/main.c`
- 任务创建：`Src/freertos.c`
- RTOS 配置：`Inc/FreeRTOSConfig.h`
- 目录指南：[`../AGENTS.md`](../AGENTS.md)
- Git 流程：[`../git-guide.md`](../git-guide.md)

## 3. docs 导航

- 规则先读：[`00_AI_HANDOFF_RULES.md`](00_AI_HANDOFF_RULES.md)
- 架构：[`02_ARCHITECTURE.md`](02_ARCHITECTURE.md)
- 任务映射：[`03_TASK_MAP.md`](03_TASK_MAP.md)
- 模块地图：[`04_MODULE_MAP.md`](04_MODULE_MAP.md)
- 索引总览：[`INDEX.md`](INDEX.md)

## 4. 交接状态入口

- 当前状态收纳：[`ai_sessions/CURRENT_STATE.md`](ai_sessions/CURRENT_STATE.md)
- 根目录兼容入口：[`../AI_HANDOVER_CURRENT_STATE.md`](../AI_HANDOVER_CURRENT_STATE.md)

## 5. 对 AGENTS.md 的使用方式

`AGENTS.md` 已包含仓库结构、构建方式、编码约束。  
这里不重复粘贴全文，仅约定：

- 文档与流程说明优先看 `docs/`。
- 构建与目录细节以 `AGENTS.md` 为准。
- 二者冲突时，先核对仓库实际代码与工程文件，再更新文档。
