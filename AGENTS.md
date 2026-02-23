# 仓库指南

本指南用于说明如何参与本仓库的 STM32F4 标准机器人固件开发。

## 项目结构与模块组织
- `application/`：机器人业务逻辑、FreeRTOS 任务与协议处理（例如 `*_task.c`）。
- `bsp/boards/`：板级驱动，命名使用 `bsp_*.c/h` 模式。
- `components/`：公共算法、控制器、设备与支撑工具。
- `Src/` 与 `Inc/`：STM32CubeMX 生成的 HAL 初始化、ISR 与外设胶水代码。
- `Drivers/` 与 `Middlewares/`：STM32 HAL/CMSIS、FreeRTOS 与 USB 中间件。
- `MDK-ARM/`：Keil uVision 工程（`standard_robot.uvprojx`）与启动文件。
- `standard_robot.ioc`：CubeMX 配置文件，用于再生成 `Src/` 与 `Inc/`。

## 构建、测试与开发命令
- 编译/下载：在 Keil uVision 中打开 `MDK-ARM/standard_robot.uvprojx`，按 `F7` 编译，使用 Download 按钮烧录。
- 清理产物：在仓库根目录运行 `.\keilkilll.bat` 删除 Keil 生成的中间文件（如 `.map`、`.axf`）。
- 重新生成代码：用 STM32CubeMX 打开 `standard_robot.ioc` 生成代码后，再在 Keil 中编译。

## 编码风格与命名规范
- 保持 `/* USER CODE BEGIN */` 与 `/* USER CODE END */` 块不变，业务逻辑放在块内，确保可再生成。
- 与 `Src/`、`application/` 现有风格一致，避免重排自动生成区域。
- 命名遵循现有习惯：外设 HAL 文件如 `adc.c/h`，任务使用 `*_task.c/h`，行为逻辑用 `*_behaviour.c/h`，板级驱动为 `bsp_*.c/h`。

## 测试指南
- 当前未配置单元测试框架，主要通过硬件实机与 FreeRTOS 任务验证。
- 临时验证可扩展 `application/test_task.c`，或新增 `*_task.c` 并在 `MX_FREERTOS_Init()` 中注册。

## 提交与合并请求规范
- 提交历史以简洁说明为主（如 "Initial commit"），暂无强制格式。
- 建议使用简短的祈使语并加子系统前缀（例如 `bsp: fix CAN filter`）。
- PR 应包含变更说明、目标板卡/MCU、编译与烧录步骤，并注明是否修改了 CubeMX 或 Keil 工程文件。

## AI 执行强制规则（docs 体系）
- 从本条开始，`docs/` 文档体系为 AI 协作的强制规范，AI 必须执行，不得跳过。
- AI 在开始任何实现前，必须先读取并遵循：
  - `docs/INDEX.md`
  - `docs/00_AI_HANDOFF_RULES.md`
  - 与任务直接相关的专题文档（如 `docs/02_ARCHITECTURE.md`、`docs/03_TASK_MAP.md`、`docs/04_MODULE_MAP.md`）
- 涉及任务调度、架构、模块职责、风险与交接的改动，必须同步更新对应 docs 文档；未更新视为任务未完成。
- 命中 `docs/00_AI_HANDOFF_RULES.md` 中“必须写 ai_sessions”的场景时，AI 必须新增会话记录到 `docs/ai_sessions/`，并包含模板要求的全部字段。
- 每次会话记录必须包含独立章节 `## ⚠ 未验证假设`；缺失该节视为不合规。
- 若本文件与 `docs/` 同类流程规则存在冲突，以 `docs/00_AI_HANDOFF_RULES.md` 与 `docs/INDEX.md` 的约束为准。
