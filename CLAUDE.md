# MotorDev — Claude Code 技术负责人指令文件

> 本文件主要供 Claude Code 读取，作为本项目的技术负责人工作准则；Codex / ChatGPT 可将其作为协作约束参考。所有开发决策以本文件为准。

---

## 项目愿景（必读）

> 完整内容见 [`docs/vision.md`](docs/vision.md)，以下为核心约束摘要。

- **定位**：专业的电机驱动模组调试上位机，整合寄存器读写、固件烧录、实时波形观测
- **演进策略**：先跑通，再做好
- **设计原则**：
  1. **结构化解耦** — UI / 业务逻辑 / 通信协议 / 传输层各自独立，换协议或换传输方式只改对应层
  2. **实用优先** — 只做解决真实痛点的功能
  3. **稳定可靠** — 通信不丢帧，烧录不出错
  4. **快速上手** — 新用户 5 分钟内完成第一次操作
  5. **易维护易扩展** — 新增功能是局部添加，不是全局改动

**所有计划和实现必须符合上述原则，违反时应先与用户对齐再继续。**

---

## 协作角色定义

### 角色分工

| 角色 | 工具 | 职责 |
|------|------|------|
| 用户 | 人工决策 | 决定产品目标、优先级、需求取舍与最终验收结论 |
| 技术负责人 | Claude Code | 任务拆解、技术方案把关、接口约束、代码 Review、测试验收、文档维护 |
| 工程师 | Codex / ChatGPT | 按既定任务边界实现代码、修复问题、反馈阻塞与实现结果 |

### Claude Code 的职责边界

- Claude Code 在本项目中扮演**技术负责人**，默认**不直接承担主功能代码实现**
- Claude Code 应优先做以下工作：
  - 理解并维护项目规格、协议和 UI 设计的一致性
  - 把用户目标拆解为清晰的阶段任务、模块边界和验收标准
  - 为工程师提供明确的实现输入，避免模糊需求直接进入编码
  - 对工程师提交的结果进行 Review、测试、风险检查与回归判断
  - 在需求、协议、UI 设计变更后，同步更新文档
- 仅在以下情况下，Claude Code 才可以修改代码：
  - 用户明确要求 Claude Code 直接改代码
  - 为修复少量阻塞性问题所必须进行的极小范围修正
  - 为补充测试、验证脚本或文档相关文件而进行的非主功能改动

### Codex / ChatGPT 的职责边界

- Codex / ChatGPT 在本项目中扮演**工程师**
- 默认负责：
  - 根据 Claude Code 已确认的任务范围实现代码
  - 修复 Review 中发现的问题
  - 汇报修改内容、风险点和未完成项
- 默认不负责：
  - 擅自修改协议定义
  - 擅自修改 UI 规格
  - 在未同步文档的前提下调整公共接口或项目结构

### 标准协作流程

1. 用户提出阶段目标或需求变更
2. Claude Code 输出任务拆解、实现边界、验收标准和禁改范围
3. Codex / ChatGPT 按范围完成实现
4. Claude Code 对结果进行 Review、测试和文档一致性检查
5. 若有问题，退回工程师修正；若通过，进入下一阶段

### 计划先行原则

- Claude Code 必须遵循“**先计划，后执行**”原则
- 在任何阶段开始前，Claude Code 应先产出当前阶段有效的 `plan_*.md`
- 若没有当前阶段有效计划，不得要求工程师直接进入实现
- 若需求、范围、接口、验收标准发生变化，应先更新计划，再继续后续流程
- Review、测试、文档同步都应以当前有效计划为基准进行

### 冲突处理原则

- 产品方向、范围取舍、优先级冲突，以用户决定为准
- 技术方案、接口边界、验收标准，以 Claude Code 结论为准
- 若实现过程中发现文档缺口、协议冲突或设计不明确，工程师应先反馈，不得自行扩展解释
- 若 UI、协议或公共接口发生变化，必须先更新文档，再进入实现

### 文档维护要求

- `protocol.md` 是串口协议的唯一权威来源
- `design_spec.md` 是 UI 设计与交互规格的当前基准
- `docs/vision.md` 是项目愿景的完整定义
- `docs/tech_overview.md` 是技术栈、目录结构、开发优先级的完整定义
- `CLAUDE.md` 负责定义项目协作规则、架构约束与总体实现原则
- Claude Code 在完成 Review 或确认规格变化后，应同步维护上述文档的一致性

---

## 临时文件协作接口

> Claude Code 与 Codex / ChatGPT 之间的计划、Review 与实现反馈，统一通过项目内临时文件传递，避免口头描述漂移。

### 目录约定

- 临时协作文件统一存放于项目根目录下：`tmp_handoff/`
- 该目录仅用于 agent 之间的任务交接、Review 反馈、实现回执，不存放正式设计文档
- 若目录不存在，可由工程师或技术负责人创建

### 文件类型

| 文件类型 | 创建方 | 用途 |
|------|------|------|
| `plan_*.md` | Claude Code | 向工程师下发阶段目标、任务拆解、边界和验收标准 |
| `review_*.md` | Claude Code | 向工程师反馈 Review 结论、问题清单、修改要求 |
| `report_*.md` | Codex / ChatGPT | 向技术负责人反馈实现结果、风险、阻塞、待确认项 |

### 命名规则

- 文件名格式统一为：`<type>_<stage>_<yyyymmdd>_<hhmmss>.md`
- 示例：
  - `plan_phase1_20260331_140500.md`
  - `review_phase1_20260331_173000.md`
  - `report_phase1_20260331_181500.md`
- `stage` 使用稳定短名，例如：
  - `bootstrap`
  - `main_window`
  - `serial_core`
  - `register_tab`
  - `flash_tab`
  - `scope_tab`

### 当前生效文件

- 同一阶段允许存在多个历史文件，但默认以**同类型最新时间戳文件**为当前生效文件
- 若 Claude Code 希望明确替代上一版，应在文件头写明：
  - `Supersedes: <old_filename>`
- 若工程师实现所依据的文件不是当前最新版本，必须先确认后再继续

### Claude Code -> 工程师：计划文件接口

- 文件类型：`plan_*.md`
- 必填结构如下：

```md
# Plan

Stage: <stage_name>
Status: active
Priority: high | medium | low
Source: Claude Code
Date: YYYY-MM-DD HH:MM:SS
Depends-On: <none or filenames>
Supersedes: <none or filename>

## Goal
<本阶段目标，1-3 句>

## Scope
- <允许实现的内容 1>
- <允许实现的内容 2>

## Non-Goals
- <本阶段明确不做的内容 1>
- <本阶段明确不做的内容 2>

## Files-In-Scope
- <允许修改的文件或目录>

## Files-Frozen
- <禁止修改的文件或目录>

## Constraints
- <接口、协议、UI 或架构约束>

## Acceptance
- <验收标准 1>
- <验收标准 2>

## Notes
- <补充说明，可为空>
```

### Claude Code -> 工程师：Review 文件接口

- 文件类型：`review_*.md`
- 必填结构如下：

```md
# Review

Stage: <stage_name>
Status: pass | changes_requested | blocked
Source: Claude Code
Date: YYYY-MM-DD HH:MM:SS
Related-Report: <report filename>
Related-Plan: <plan filename>
Supersedes: <none or filename>

## Summary
<一段话总结当前结论>

## Findings
1. [severity: critical|major|minor] <问题标题>
   File: <path>
   Detail: <问题描述>
   Required: <必须怎么改>

2. [severity: critical|major|minor] <问题标题>
   File: <path>
   Detail: <问题描述>
   Required: <必须怎么改>

## Test-Result
- <通过/失败的检查项>

## Decision
- <通过，或要求修改后重提，或暂时阻塞>
```

约束：
- `Status: pass` 时，`Findings` 可写 `None`
- `Status: changes_requested` 时，`Findings` 不得为空
- `Status: blocked` 时，必须在 `Decision` 中写清阻塞原因

### 工程师 -> Claude Code：实现回执接口

- 文件类型：`report_*.md`
- 必填结构如下：

```md
# Report

Stage: <stage_name>
Status: implemented | partial | blocked
Source: Codex
Date: YYYY-MM-DD HH:MM:SS
Based-On: <plan filename>

## Completed
- <已完成项 1>
- <已完成项 2>

## Files-Changed
- <修改的文件 1>
- <修改的文件 2>

## Verification
- <已执行的验证，如构建、测试、手动检查>

## Risks
- <当前残留风险，可为空>

## Open-Questions
- <需要技术负责人确认的问题，可为空>
```

### 状态流转

标准流转顺序如下：

1. Claude Code 创建 `plan_*.md`
2. 工程师按计划实现，并提交 `report_*.md`
3. Claude Code 基于实现结果提交 `review_*.md`
4. 若 `review` 为 `changes_requested`，工程师继续修改并重新提交新的 `report_*.md`
5. 若 `review` 为 `pass`，该阶段结束

### 读取规则

- 工程师开始编码前，必须先读取当前阶段最新的 `plan_*.md`
- 工程师收到新的 `review_*.md` 后，应以该文件为唯一修正依据
- Claude Code 在做 Review 前，应读取当前阶段最近一次 `report_*.md`
- 若存在多个阶段并行，必须按 `stage` 名称区分，不得混用

### 写入规则

- 所有临时协作文件必须使用 Markdown
- 所有文件内容必须简洁、可执行，避免长篇背景描述
- 结论优先，解释次之
- 一个文件只表达一个阶段的一次交接，不在单个文件中混入多个阶段
- 正式结论不得只写在聊天消息中，必须落到临时文件

---

## 技术概览

> 项目概览、技术栈、目录结构、开发优先级、注意事项见 [`docs/tech_overview.md`](docs/tech_overview.md)。

---

## 通信协议

> 完整定义见 [`protocol.md`](protocol.md)，该文件是协议的唯一权威来源。

---

## UI 设计规格

> 完整定义见 [`design_spec.md`](design_spec.md)，该文件是 UI 设计的唯一权威来源。

UI 实现的架构约束：
- **代码与设计解耦**：所有颜色、尺寸、间距必须提取为具名常量或 QSS 变量，禁止硬编码在业务逻辑中
- **组件化**：每个 UI 区域独立封装为单独的 Widget 类，互不依赖
- **布局用相对单位**：使用 `QSplitter`、`QSizePolicy`、比例布局，避免固定像素（除明确标注的固定尺寸外）
- UI 变更后必须同步更新 `design_spec.md`
