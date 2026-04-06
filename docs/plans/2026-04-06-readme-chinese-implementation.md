# GitHub README 中文首页改版 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将根目录 `README.md` 改为完整中文 GitHub 首页版，并补齐对应设计/计划文档入口。

**Architecture:** 先写设计文档与实施计划，再用一次性 Markdown 重写替换旧 README。新 README 聚焦项目概述、目录结构、构建方式、Wi-Fi 调参检查单、文档入口和分支说明，不复制 `docs/` 细节。最后通过 `git diff --check` 和内容回读做最小校验。

**Tech Stack:** Markdown、Git、PowerShell

---

### Task 1: 设计文档落盘

**Files:**
- Create: `docs/plans/2026-04-06-readme-chinese-design.md`

**Step 1: 写设计文档**

- 说明目标、结构、设计原则和非目标。

**Step 2: 回读设计文档**

Run: `Get-Content -Encoding UTF8 docs/plans/2026-04-06-readme-chinese-design.md`
Expected: 能看到中文设计说明，覆盖 README 各章节目标。

### Task 2: 实施计划落盘

**Files:**
- Create: `docs/plans/2026-04-06-readme-chinese-implementation.md`

**Step 1: 写实施计划**

- 使用固定表头，列出 README 改写、索引更新、校验和提交步骤。

**Step 2: 回读实施计划**

Run: `Get-Content -Encoding UTF8 docs/plans/2026-04-06-readme-chinese-implementation.md`
Expected: 能看到完整计划头和任务拆分。

### Task 3: 改写根 README

**Files:**
- Modify: `README.md`

**Step 1: 删除旧的英文临时说明**

- 移除当前英文 `Wi-Fi UART Param Checklist` 结构。

**Step 2: 写入完整中文首页版**

- 新增以下章节：
  - 项目概述
  - 开发环境
  - 仓库结构
  - 编译与烧录
  - Wi-Fi 串口调参检查清单
  - 文档入口
  - 分支说明

**Step 3: 回读 README**

Run: `Get-Content -Encoding UTF8 README.md`
Expected: README 主体为中文，命令与路径保留原样可复制。

### Task 4: 更新 docs 索引

**Files:**
- Modify: `docs/INDEX.md`

**Step 1: 增加计划文档入口**

- 将本轮新增的 design / implementation 文档加入 `docs/INDEX.md` 的 plans 区域附近。

**Step 2: 回读索引命中项**

Run: `Select-String -Path docs/INDEX.md -Pattern "2026-04-06-readme-chinese"`
Expected: 返回两条新计划文档索引。

### Task 5: 最小校验与提交

**Files:**
- Verify: `README.md`
- Verify: `docs/INDEX.md`
- Verify: `docs/plans/2026-04-06-readme-chinese-design.md`
- Verify: `docs/plans/2026-04-06-readme-chinese-implementation.md`

**Step 1: 运行空白与补丁校验**

Run: `git diff --check`
Expected: 无空白错误；若仅有换行符告警，不应出现 patch failure。

**Step 2: 查看改动摘要**

Run: `git diff --stat`
Expected: README 和新增计划文档在统计中可见。

**Step 3: 提交**

```bash
git add README.md docs/INDEX.md docs/plans/2026-04-06-readme-chinese-design.md docs/plans/2026-04-06-readme-chinese-implementation.md
git commit -m "docs: rewrite readme in chinese"
```

**Step 4: 推送**

```bash
git push
```
