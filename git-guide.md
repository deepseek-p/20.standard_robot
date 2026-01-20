# Git 入门指南（新手版）

本教程面向第一次接触 Git 的新手，涵盖常见概念、常用命令与 GitHub 协作流程。示例以 Windows + PowerShell 为主，但命令在其他系统同样适用。

## 1. Git 是什么

- Git 是版本控制工具，用来记录代码历史、多人协作、快速回退。
- 仓库（repository）是存放代码与历史记录的地方。
- 提交（commit）是一次“快照”，记录一批改动。
- 分支（branch）是平行开发线路，避免互相干扰。
- 远程仓库（remote）常见于 GitHub/GitLab，用于同步和协作。

## 2. 安装与基础配置

1) 安装 Git
- Windows: https://git-scm.com/download/win
- 安装后在 PowerShell / CMD 里执行 `git --version` 验证。

2) 设置用户名和邮箱（会写进提交记录）
```bash
git config --global user.name "你的名字"
git config --global user.email "你的邮箱"
```

3) 建议设置换行符（Windows）
```bash
git config --global core.autocrlf true
```

4) 查看配置
```bash
git config --global --list
```

## 3. 创建与克隆仓库

### 3.1 本地新建仓库
```bash
cd 你的项目目录
git init
```

### 3.2 克隆已有仓库
```bash
git clone https://github.com/用户名/仓库名.git
```

## 4. 日常工作流程（最常用）

### 4.1 查看当前状态
```bash
git status
```

### 4.2 暂存改动（add）
```bash
git add 文件名
# 或者一次性暂存所有改动
git add .
```

### 4.3 提交改动（commit）
```bash
git commit -m "简短清晰的提交说明"
```

### 4.4 查看提交历史
```bash
git log --oneline --decorate --graph --all
```

### 4.5 查看差异
```bash
# 工作区 vs 暂存区
git diff
# 暂存区 vs 最近一次提交
git diff --staged
```

## 5. 远程仓库与 GitHub

### 5.1 关联远程仓库
```bash
git remote add origin https://github.com/用户名/仓库名.git
```

### 5.2 推送到远程
```bash
# 第一次推送（并设置上游分支）
git push -u origin main
# 以后直接
 git push
```

### 5.3 拉取更新
```bash
# 拉取并合并
git pull
# 或更安全：拉取并变基（保持历史更直）
git pull --rebase
```

### 5.4 查看远程配置
```bash
git remote -v
```

## 6. 分支基础

### 6.1 查看分支
```bash
git branch -vv
```

### 6.2 新建并切换分支
```bash
git checkout -b feature/xxx
# 新版命令
# git switch -c feature/xxx
```

### 6.3 合并分支
```bash
# 切回 main
git checkout main
# 合并 feature/xxx
git merge feature/xxx
```

### 6.4 删除分支
```bash
git branch -d feature/xxx
```

## 7. .gitignore（忽略不需要提交的文件）

在仓库根目录创建 `.gitignore`，常见内容示例：
```gitignore
# 编译产物
build/
*.o
*.obj
*.map

# IDE / 编辑器
.vscode/
.idea/
*.swp

# 系统文件
.DS_Store
Thumbs.db
```

## 8. 常见撤销操作（非常重要）

### 8.1 撤销工作区改动（未 add）
```bash
git restore 文件名
```

### 8.2 撤销暂存区（已 add）
```bash
git restore --staged 文件名
```

### 8.3 修改上一次提交信息（仅本地）
```bash
git commit --amend -m "新的提交说明"
```

### 8.4 回退到某次提交（谨慎）
```bash
# 仅改变 HEAD 指向，不动工作区
git reset --soft 提交ID
# 回退并保留工作区改动
git reset --mixed 提交ID
# 强制回退（危险，会丢失改动）
git reset --hard 提交ID
```

## 9. 常见问题排查

### 9.1 push 被拒绝（远程有更新）
```bash
git pull --rebase origin main
git push
```

### 9.2 提交后发现漏文件
```bash
git add 漏掉的文件
git commit --amend --no-edit
```

### 9.3 误删文件恢复
```bash
git restore 文件名
```

## 10. 推荐的日常小流程

1) 拉取更新：`git pull --rebase`
2) 编码修改
3) 查看改动：`git status` / `git diff`
4) 暂存：`git add .`
5) 提交：`git commit -m "描述"`
6) 推送：`git push`

## 11. 常用命令速查

```bash
git status
git add .
git commit -m "msg"
git push
git pull --rebase
git log --oneline --graph --all
git diff
git branch -vv
git checkout -b feature/xxx
```

如果你希望我根据你的实际项目（比如 STM32/Keil 工程）补充忽略规则或推荐工作流，可以直接告诉我。
