# Review

Stage: register_rwtab
Status: changes_requested
Source: Claude Code
Date: 2026-04-02 14:30:00
Related-Report: report_register_rwtab_20260402_111000.md
Related-Plan: plan_register_rwtab_20260402_170000.md
Supersedes: none

## Summary

代码审查后，列宽分配、按钮文字 R/W、DEC/HEX 进制切换均已正确实现，rowHasAddress 也已简化。
共 4 处问题需修正：布局 GroupCount 错误（主要问题）、整页禁用缺失、描述列需再缩窄、列头文字不一致。

## Already-Verified

以下各项已实现，无需改动：
- TableRowCount 已改为 20
- DEC/HEX 侧边栏控件（QButtonGroup、makeModeButtonStyle）已实现
- RegisterTable::setValueMode、parseNumericText 已实现
- 表格内 R/W 按钮文字已改
- rowHasAddress 已简化，无 IIFE

## Findings

1. [severity: major] TableGroupCount 错误改为 1，应保持 4
   File: src/ui/style_constants.h
   Detail: 原始需求是"4组 × 4行，只将行数改为 20"，结果为 4组 × 20行。工程师将 TableGroupCount
   改成了 1，导致表格变为 1组 × 20行，与需求不符。
   Required:
   - `TableGroupCount`: 1 → 4
   - 同步影响：TotalRows = 4 × 20 = 80，registertable.h 中数组改为 [80]

2. [severity: major] RegisterRwTab 页面未整体禁用
   File: src/mainwindow.cpp
   Detail: serialConnected / serialDisconnected lambda 只调用了 m_registerTab->setConnected(bool)，
   页面 widget 本身仍可进入。FlashPage / ScopePage 通过 setEnabled(false) 整页禁用，需保持一致。
   Required:
   - serialConnected lambda 中，setConnected(true) 之后补加：
     m_registerTab->setEnabled(true);
   - serialDisconnected lambda 中，setConnected(false) 之后补加：
     m_registerTab->setEnabled(false);
   - 构造函数末尾初始状态处，setConnected(false) 之后补加：
     m_registerTab->setEnabled(false);
   Note: setConnected 本身保留，负责清空队列，不改动。

3. [severity: minor] 描述列宽需再缩窄以适配 4 组布局
   File: src/ui/style_constants.h
   Detail: 4组时总列宽 = 267px × 4 = 1068px，超出默认窗口内容区 1020px，会出现横向滚动条。
   缩窄描述列 12px 后每组 255px，4组合计恰好 1020px。
   Required:
   - `ColDescWidth`: 88 → 76

4. [severity: minor] 列头文字与按钮文字不一致
   File: src/widgets/registertable.cpp:136
   Detail: 按钮已改为 "R" / "W"，列头仍为中文 "读" / "写"，视觉不统一。
   Required:
   将 groupHeaders 中 "读" 和 "写" 改为英文：
   ```
   const QStringList groupHeaders = {tr("描述"), tr("地址"), tr("值"), QStringLiteral("R"), QStringLiteral("W")};
   ```
   Note: R/W 为固定英文缩写，不需要 tr() 包裹。

## Test-Result

- 编译通过（mingw32-make -j4）✓
- 布局 4组×20行：TableGroupCount 错误 ✗（Finding 1）
- 未连接时整页禁用：未实现 ✗（Finding 2）
- DEC/HEX 切换：已实现 ✓
- 列宽：需缩窄描述列 ✗（Finding 3）

## Decision

changes_requested。修正优先级：Finding 1 → Finding 2 → Finding 3 → Finding 4。
Finding 1 和 Finding 3 均在 style_constants.h，建议一并处理。
修正完成后重新提交 report_register_rwtab_*.md。
