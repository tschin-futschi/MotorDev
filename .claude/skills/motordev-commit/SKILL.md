---
name: motordev-commit
description: MotorDev 项目标准提交流程。当用户说"提交""commit""提交代码""push"时使用。执行 git status → 展示变更 → 起草 commit message → 用户确认 → 提交 → 询问是否 push → 询问目标分支 → push。
---

# MotorDev 标准提交流程

## 概述

本技能固化 MotorDev 项目的 git 提交与推送流程，确保每次提交格式统一、变更可审、push 前经过确认。

## 触发条件

当用户说出以下意图时使用：

- 提交
- commit
- 提交代码
- 提交变更
- push
- 上传 GitHub
- 推送

## 执行步骤（严格按顺序）

### Step 1 — 收集变更信息

并行执行以下命令：

- `git status`：列出所有已修改、已暂存、未跟踪的文件
- `git diff HEAD`：查看所有未暂存和已暂存的变更内容
- `git log --oneline -5`：查看最近 5 条提交，用于对照风格

### Step 2 — 向用户展示变更摘要

将以下信息输出给用户确认：

- 变更文件清单（分类列出：已修改 / 新增 / 删除）
- 每个文件的变更要点（1 句话，不要贴原始 diff）
- 询问：**"以上变更是否全部纳入本次提交？"**

等待用户确认后再继续。若用户要求排除某些文件，记录排除项，后续 stage 时跳过。

### Step 3 — 起草 commit message

格式为 Conventional Commits：

```
<type>(<scope>): <description>

<body>
```

**type 可选值：**
- `feat` — 新功能、新文件
- `fix` — bug 修复
- `refactor` — 不改功能的结构调整
- `style` — UI 样式、视觉调整
- `docs` — md 文档、规格文件、tmp_handoff 文件
- `chore` — .gitignore、CMakeLists、构建配置、杂项

**scope 可选值（可省略）：**
- `core` — mainwindow、串口核心、SerialManager
- `register` — 寄存器读写 Tab 及相关 Widget
- `scope` — 示波器 Tab 及相关 Widget
- `flash` — 固件烧录 Tab
- `config` — 配置 Tab
- `ui` — 跨模块 UI 调整、样式常量
- `build` — CMakeLists、构建配置
- `docs` — md 文档、规格文件
- `chore` — .gitignore、杂项

**description：**
- 英文，祈使句，首字母小写，不加句号
- 不超过 72 字符

**body：**
- 中文或英文均可
- 说明 why：本次提交的背景、关联的 plan/review 文件名、阶段节点
- 跨模块或重要节点必填；日常小改可省略
- 与第一行之间空一行

**示例：**
```
feat(scope): add oscilloscope tab widget skeletons

新增 scopetoolbar / scopeplotwidget / scopechannelstrip 等 6 个 Widget 骨架，
对应 plan_scope_ui 计划阶段，尚未 Review，待验收后继续。
```

将起草好的 commit message 展示给用户，询问：**"commit message 是否确认？如需修改请直接告知。"**

等待用户确认或修改后再继续。

### Step 4 — 暂存并提交

- 使用 `git add` 按文件名逐一暂存（禁止使用 `git add -A` 或 `git add .`，除非用户明确要求全量暂存）
- 若 Step 2 中用户要求排除某些文件，此处跳过对应文件
- 使用 HEREDOC 方式执行 commit，附加署名：

```
git commit -m "$(cat <<'EOF'
<type>(<scope>): <description>

<body>

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

- 提交完成后输出 commit hash 和 message 摘要

### Step 5 — 询问是否 Push

输出：**"是否将本次提交推送到远端？"**

- 若用户回答否，流程结束
- 若用户回答是，进入 Step 6

### Step 6 — 询问 Push 目标分支

输出当前分支名，询问：**"推送目标分支是 `<current-branch>`，还是其他分支？"**

- 若用户确认当前分支，执行 `git push origin <current-branch>`
- 若用户指定其他分支，执行 `git push origin <specified-branch>`

Push 完成后输出推送结果（成功 / 失败 / 错误信息）。

## 禁止行为

- 禁止跳过用户确认直接提交或推送
- 禁止使用 `--no-verify` 绕过 pre-commit hook
- 禁止使用 `--force` 推送，除非用户明确要求且目标不是 main 分支
- 禁止对 main 分支执行 force push，即使用户要求也必须警告并拒绝
- 禁止在用户未确认 commit message 前执行 `git commit`

## 与其他流程的关系

- 本 skill 只负责提交流程，不负责 Review、计划或问题追踪
- 提交前是否 Review 通过，由用户和 Claude Code 在正常协作流程中判断，本 skill 不做强制检查
