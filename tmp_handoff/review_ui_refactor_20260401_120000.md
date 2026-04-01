# Review

Stage: ui_refactor
Status: changes_requested
Source: Claude Code
Date: 2026-04-01 12:00:00
Related-Report: report_ui_refactor_20260401_104000.md
Related-Plan: plan_ui_refactor_20260331_220000.md
Supersedes: none

## Summary

整体结构正确，4 个 Tab 各自内嵌独立侧边栏，全局 Sidebar 已移除，ConfigTab 包含 COM 口配置。发现 1 个 major 和 2 个 minor 问题需修正。

## Findings

1. [severity: major] Sidebar 收缩动画只驱动 minimumWidth，未同步 maximumWidth
   File: src/widgets/sidebar.cpp:15-16, 76-89
   Detail: 构造函数同时设置了 `setMinimumWidth` 和 `setMaximumWidth` 为 `SidebarTotalWidth`，但 `QPropertyAnimation` 只动画 `minimumWidth` 属性。收缩时 `minimumWidth` 变为 `SidebarHandleWidth`，而 `maximumWidth` 仍为 `SidebarTotalWidth`，导致 layout 不会真正收缩侧边栏宽度（Qt 布局器在 min ≤ width ≤ max 范围内分配空间，max 不变则可能不收缩）。
   Required: 动画需同时驱动 `maximumWidth`，或使用两个并行 `QPropertyAnimation`（一个 minimumWidth 一个 maximumWidth），或改用 `setFixedWidth` + 单属性动画。推荐方案：将 animation target 改为 `maximumWidth`，同时在动画的 `valueChanged` 信号中同步设置 `minimumWidth`，确保 min == max 始终成立。

2. [severity: minor] report 缺少 `Based-On` 和 `Source` 字段格式
   File: tmp_handoff/report_ui_refactor_20260401_104000.md
   Detail: 计划要求 report 使用 `Based-On` 字段指向对应的 plan 文件，实际使用了 `Author` 而非 `Source`，且缺少 `Based-On`。
   Required: 补充 `Based-On: plan_ui_refactor_20260331_220000.md`，将 `Author: Codex` 改为 `Source: Codex`。

3. [severity: minor] ConfigTab COM 口配置中 tr() 包裹了纯数字字符串
   File: src/tabs/configtab.cpp:116-123
   Detail: `tr("9600")`, `tr("COM1")`, `tr("8")`, `tr("1")`, `tr("None")` 等不应翻译的技术参数被 `tr()` 包裹。这些值在任何语言下都不应被翻译，使用 `tr()` 会产生无用的翻译条目。
   Required: 纯技术参数字符串（端口名、波特率数值、数据位数值、停止位数值、校验模式标识）改用 `QStringLiteral` 而非 `tr()`。仅对 UI 标签文字（如"端口"、"波特率"）保留 `tr()`。

## Test-Result

- [PASS] 编译通过（工程师已验证）
- [PASS] 活动栏 4 个功能按钮顺序正确：配置(0) → 读写(1) → 烧录(2) → 示波(3)，底部设置
- [PASS] 点击活动栏按钮通过信号连接切换 QStackedWidget 页面
- [PASS] 每个 Tab 内部有独立的 Sidebar 实例
- [CONCERN] 每个 Tab 的侧边栏收缩/展开 — 动画逻辑有 bug（见 Finding #1）
- [PASS] ConfigTab 主内容区包含 COM 口配置（端口、波特率、数据位、停止位、校验位 + 连接按钮）
- [PASS] ConfigTab 侧边栏显示占位内容（马达厂家、配置文件）
- [PASS] MainWindow 中不再有全局 Sidebar
- [PASS] 无硬编码色值，新增常量均在 style_constants.h 中定义
- [PASS] 冻结文件未被修改

## Decision

- Finding #1 (major)：必须修复后重提
- Finding #2 (minor)：修复 report 格式
- Finding #3 (minor)：随 #1 一并修复
- 修复后提交新的 `report_ui_refactor_*.md`
