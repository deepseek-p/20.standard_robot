# GitHub README 中文首页改版设计

## 1. 目标

- 将仓库根目录 `README.md` 从临时调参备忘改写为适合 GitHub 首页展示的中文说明页。
- 保留现有 Wi-Fi UART 调参检查清单，但改为中文表达并放入更完整的项目介绍结构中。
- 在 README 中直接解释“默认分支较旧、开发分支较新”的查看方式，降低使用者的理解成本。

## 2. 设计原则

- 全文使用中文说明，英文仅保留必要的文件路径、分支名、命令和工程名。
- 优先提供项目入口信息，而不是堆叠零散调试细节。
- 保持内容简洁，避免把 `docs/` 中的大量交接细节重复搬进 README。
- 让第一次进入仓库的人可以快速回答三个问题：
  - 这是一个什么项目？
  - 我从哪里编译、烧录、查看文档？
  - 为什么 GitHub 首页显示的更新时间可能不是最新？

## 3. README 结构

### 3.1 标题与项目概述

- 标题保留 `20.standard_robot`。
- 增加一句话说明：这是基于 `STM32F407 + FreeRTOS (CMSIS-RTOS v1)` 的 RoboMaster 标准步兵固件工程。

### 3.2 开发环境

- 简述主要工具链：
  - `Keil uVision`
  - `STM32CubeMX`
  - `PowerShell / Git`

### 3.3 仓库结构

- 以列表形式说明核心目录职责：
  - `application/`
  - `bsp/boards/`
  - `components/`
  - `Src/` / `Inc/`
  - `Drivers/` / `Middlewares/`
  - `MDK-ARM/`
  - `standard_robot.ioc`

### 3.4 编译与烧录

- 保留当前实际开发流程：
  - 打开 `MDK-ARM/standard_robot.uvprojx`
  - `F7` 编译
  - Download 烧录
  - `.\keilkilll.bat` 清理
  - 需要时用 `standard_robot.ioc` 重新生成代码

### 3.5 Wi-Fi 串口调参检查清单

- 将当前英文检查单完整改写为中文：
  - ESP32 端口检查
  - PC 连接
  - 遥测检查
  - 命令回显检查
  - 收发线对调判定
- 保留命令原样，方便复制执行。

### 3.6 文档入口

- 指向：
  - `docs/INDEX.md`
  - `docs/ai_sessions/CURRENT_STATE.md`
  - `AGENTS.md`

### 3.7 分支说明

- 明确说明 GitHub 默认展示的通常是 `main`。
- 若仓库首页显示较旧更新时间，需要切换到正在开发的分支，或等待分支合并回 `main`。

## 4. 非目标

- 不在 README 中重复 `docs/` 体系的完整 AI 交接规则。
- 不改代码、不改构建逻辑、不改 Keil/CubeMX 配置。
- 不在本轮处理分支合并，仅更新 README 展示信息。
