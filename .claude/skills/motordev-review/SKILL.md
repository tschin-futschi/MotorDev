---
name: motordev-review
description: MotorDev 项目标准 Review 流程。当用户说"review 代码""帮我 review""做 review"时使用单阶段模式；当用户说"全量 review""review 整个项目"时使用全量模式。单阶段模式自动找最新未 review 的 report，全量模式直接扫源码并对照所有权威文档。
---

# MotorDev 标准 Review 流程

## 概述

本技能固化 MotorDev 项目的代码 Review 流程，支持两种模式：

- **单阶段模式**：工程师提交 `report_*.md` 后触发，对照 plan 验收标准和红线逐项检查
- **全量模式**：不依赖 `tmp_handoff/`，直接扫描全部源码，对照所有权威文档做完整检查

## 触发条件

**单阶段模式**，当用户说出：
- review 代码
- 帮我 review
- 做 review
- 看一下报告
- 工程师提交了

**全量模式**，当用户说出：
- 全量 review
- review 整个项目
- 全面检查一下代码

---

## 单阶段模式

### Step 1 — 定位最新未 review 的 report

在 `tmp_handoff/` 中：

1. 列出所有 `report_*.md` 文件
2. 列出所有 `review_*.md` 文件
3. 找出**没有对应 review 的 report**：即 stage 名称在 report 文件名中出现，但在 review 文件名中不存在的
4. 若存在多个未 review 的 report，取**时间戳最新**的一个
5. 若用户在触发时指定了阶段名，优先使用用户指定的

向用户说明：**"将对 `<report文件名>` 进行 Review。"**

### Step 2 — 读取必要文件

并行读取：

1. 目标 `report_*.md`：了解实现内容、变更文件、验证状态、风险与阻塞
2. 对应 `plan_*.md`：找到 `Acceptance` 验收标准清单、`Files-In-Scope`、`Files-Frozen`、`Constraints`
3. 报告中列出的**变更源码文件**：逐一读取，用于代码检查

若 plan 文件名无法从 report 的 `Based-On` 字段直接推断，在 `tmp_handoff/` 中按 stage 名称匹配最近一份 plan。

### Step 3 — 逐项检查

#### 3A — 验收标准对照（来自 plan 的 Acceptance 清单）

逐条列出 plan 中的验收标准，标注每条是否满足：
- ✓ 满足
- ✗ 不满足（说明原因）
- ？ 无法从 report 或代码判断（说明原因）

#### 3B — CLAUDE.md 红线检查

逐项检查以下红线，每项标注 ✓ / ✗ / ？：

1. **UI/逻辑分层**：业务逻辑是否下沉到 Manager/Service，未混写在 UI 类中
2. **无硬编码色值/尺寸**：颜色、尺寸、间距是否全部引用 `Style::Color` / `Style::Size` 常量
3. **控件作为成员变量**：需要交互的控件是否作为成员变量声明，而非局部变量
4. **setupUi / connectSignals 分离**：UI 构建与信号槽连接是否拆分为独立方法
5. **无多区域揉合**：是否按区域封装为独立 Widget，未将多个 UI 区域写在单个大类中
6. **布局策略**：是否优先使用 QSplitter、QSizePolicy 和比例布局，而非固定像素

#### 3C — 编译验证状态

从 report 的 `Verification` 字段确认：
- 编译是否通过（明确写出 pass / fail / 未提及）

#### 3D — 代码命名规范

读取变更源码文件，检查：
- 成员变量命名是否遵循 `m_` 前缀约定
- 方法命名是否清晰、符合项目已有风格
- 新增类名是否与模块职责匹配

### Step 4 — 汇总 Findings

将 Step 3 中发现的所有问题汇总为 Findings 列表。

每条 Finding 格式：

```
[级别] <问题描述>
位置：<文件名:行号 或 类名/方法名>
说明：<具体原因或证据>
建议：<修改方向>
```

级别定义：
- `Blocking` — 必须修复，否则不能通过（红线违反、验收标准未满足）
- `Non-blocking` — 建议修复，不阻塞通过（命名不规范、小结构问题）
- `Nit` — 极小问题，可改可不改（注释缺失、空行风格等）

### Step 5 — 给出 Review 结论

根据 Findings 决定结论：

| 结论 | 条件 |
|---|---|
| `Approved` | 无任何 Blocking 问题，无或仅有 Nit |
| `Approved with comments` | 无 Blocking 问题，有 Non-blocking 或 Nit |
| `Changes requested` | 存在任意 Blocking 问题 |

### Step 6 — 输出 review_*.md

在 `tmp_handoff/` 中创建 review 文件，命名格式：

`review_<stage>_<yyyymmdd>_<hhmmss>.md`

文件内容结构：

```markdown
# Review

Stage: <stage名称>
Status: <Approved | Approved with comments | Changes requested>
Source: Claude Code
Date: <yyyy-mm-dd hh:mm:ss>
Related-Report: <report文件名>
Related-Plan: <plan文件名>

## Summary

<2-3句话总结本次Review结论和主要发现>

## Acceptance Checklist

<逐条列出验收标准及是否满足>

## Red-Line Check

<逐条列出红线检查结果>

## Findings

<按 Blocking → Non-blocking → Nit 顺序列出，无问题写 None>

## Compilation

<编译验证状态>

## Decision

<Approved / Approved with comments / Changes requested>
<若 Changes requested，明确列出工程师必须修复的项目清单>
```

### Step 7 — 向用户报告结论

输出简短结论：
- Review 文件路径
- 最终结论（Approved / Approved with comments / Changes requested）
- 若 Changes requested：列出 Blocking 问题清单
- 若 Approved with comments：列出 Non-blocking 问题清单

---

## 全量模式

### 概述

不依赖 `tmp_handoff/`，直接读取全部源码和所有权威文档，做九个维度的完整检查。每个维度独立输出结果。遇到需要用户决策的维度，必须停下来等待用户确认后再继续。

### Step 1 — 收集基础信息

并行执行：
- `git status`：列出未提交文件
- 列出 `src/` 下所有源码文件
- 读取 `docs/tech_overview.md`、`docs/file_index.md`、`docs/vision.md`、`protocol.md`、`design_spec.md`、`CLAUDE.md`

### Step 2 — 逐维度检查（按顺序执行，遇决策点暂停）

#### 维度 1 — 代码质量

读取 `src/` 下所有源码文件，检查：

**CLAUDE.md 红线：**
1. UI/逻辑分层：业务逻辑是否下沉到 Manager/Service
2. 无硬编码色值/尺寸：是否全部引用 `Style::Color` / `Style::Size` 常量
3. 控件作为成员变量：交互控件是否声明为成员变量
4. setupUi / connectSignals 分离：UI 构建与信号槽连接是否拆分
5. 无多区域揉合：是否按区域封装为独立 Widget
6. 布局策略：是否优先使用 QSplitter、QSizePolicy 和比例布局

**命名规范：**
- 成员变量是否遵循 `m_` 前缀约定
- 方法命名是否清晰、符合项目已有风格
- 类名是否与模块职责匹配

#### 维度 2 — 架构结构

1. 对照 `docs/tech_overview.md` 检查实际目录结构和模块职责
2. 对照 `docs/file_index.md` 检查实际源码文件清单

**若发现 tech_overview 与实际源码结构不一致：**
- 停下来，明确列出所有差异项
- 询问用户：**"以上差异，是遵循 tech_overview.md 的定义（需要调整代码），还是修改 tech_overview.md 以反映当前现状？"**
- 等待用户逐项确认后再继续

**若发现 file_index.md 与实际源码不一致：**
- 停下来，明确列出所有差异项（新增文件未收录 / 已删除文件仍在索引 / 模块职责描述与实现不符）
- 询问用户：**"以上差异，是遵循 file_index.md 的定义，还是更新 file_index.md 以反映当前现状？"**
- 等待用户逐项确认后再继续

#### 维度 3 — 协议一致性

对照 `protocol.md`，检查源码中：
- 串口命令字节值是否与协议定义一致
- 帧格式、字段顺序、长度是否正确
- 有无硬编码协议魔数未在协议层定义（应集中定义，不散落在业务代码中）

#### 维度 4 — UI 规格一致性

对照 `design_spec.md`，检查源码中：
- UI 布局、区域划分是否与规格一致
- 有无实现了 design_spec 未定义的 UI 区域（越界实现）

**若发现不一致：**
- 明确列出所有不一致项
- 询问用户：**"以上不一致，是遵循 design_spec.md（需要调整代码），还是修改 design_spec.md 以反映当前实现？"**
- 等待用户逐项确认后再继续

#### 维度 5 — 编译状态

执行项目构建命令验证当前代码是否能编译通过：

```
D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -C build_make_qt -j4
```

明确输出：编译通过 / 编译失败（附错误信息）

#### 维度 6 — 未提交变更

输出 `git status` 结果，分类列出：
- 已修改未提交的文件
- 新增未跟踪的文件
- 已删除未提交的文件

提示用户哪些文件还未提交。

#### 维度 7 — vision.md 对齐

对照 `docs/vision.md` 的 v1.0 目标（寄存器读写、固件烧录、实时示波器），检查：
- 当前已实现的功能是否超出 v1.0 定义范围
- 是否存在 vision 未提及的功能模块

**若发现不一致：**
- 明确陈述不一致的地方
- 询问用户：**"以上功能超出/偏离了 vision.md 的 v1.0 定义，是调整代码以符合 vision，还是修改 vision.md 以反映当前方向？"**
- 等待用户确认后再继续

#### 维度 8 — Qt 对象管理

读取源码，检查：
- Qt 父子对象关系是否正确（new 出来的 QObject 子类是否设置了 parent）
- 有无裸指针未纳入 Qt 对象树导致潜在内存泄漏
- 信号槽连接是否与 UI 解耦（UI 层只发信号，不直接调用业务逻辑）
- 有无跨线程信号槽连接未使用 Qt::QueuedConnection 的风险

#### 维度 9 — tmp_handoff 流程完整性

检查 `tmp_handoff/` 中：
- 有 report 但无对应 review 的阶段（流程断裂）
- 有 plan 但无对应 report 的阶段（计划发出但工程师未回执）

列出所有流程断裂的阶段，供用户知晓。

### Step 3 — 汇总全量 Findings

所有维度检查完成后，汇总为统一 Findings 列表，格式与单阶段模式一致：

```
[级别] <问题描述>
维度：<维度编号和名称>
位置：<文件名:行号 或 类名/方法名>
说明：<具体原因或证据>
建议：<修改方向>
```

### Step 4 — 给出全量 Review 结论

结论级别与单阶段一致：`Approved` / `Approved with comments` / `Changes requested`

### Step 5 — 输出全量 review 文件

在 `tmp_handoff/` 中创建文件，命名格式：

`review_fullscan_<yyyymmdd>_<hhmmss>.md`

结构与单阶段 review 文件一致，额外包含各维度独立的检查结果章节。

---

## 通用禁止行为

- 禁止在未读取源码的情况下做代码检查
- 禁止跳过红线检查
- 禁止将运行时/实机验证状态作为 Review 结论的依据
- 禁止将 Review 结论只停留在聊天消息中，必须输出 review 文件
- 禁止在用户决策点未确认的情况下继续后续检查

## 与其他流程的关系

- Review 完成后，若结论为 `Changes requested`，工程师修复后需重新提交 `report_*.md`，Claude Code 再次 Review
- Review 完成后，若结论为 `Approved` 或 `Approved with comments`，由用户决定是否进入下一阶段
- 全量模式中用户确认的文档修改决策，应在 Review 完成后立即同步更新对应文档
- Review 文件是 `tmp_handoff/` 中的正式结论，不得修改或删除
