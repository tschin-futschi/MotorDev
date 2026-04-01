# Review

Stage: ui_refactor
Status: changes_requested
Source: Claude Code
Date: 2026-04-01 20:00:00
Related-Report: report_ui_refactor_20260401_171500.md
Related-Plan: plan_ui_refactor_20260331_220000.md
Supersedes: review_ui_refactor_20260401_190000.md

## Summary

用户要求调整 ConfigTab 中 MotorICGroup 和 ConfigFileGroup 的内容和布局，同时包含上一轮未提交的 PMICGroup 新增需求。共 3 项变更。

## Findings

1. [severity: major] MotorICGroup 改名并新增 Slave ID
   File: src/tabs/configtab.cpp (makeMotorIcGroup 函数)
   Detail: 当前 Group 标题为 "MotorIC"，仅含一个 IC 选择下拉框。
   Required:
   - Group 标题从 `tr("MotorIC")` 改为 `tr("IC")`
   - 保留现有 IC 选择下拉框
   - 新增一行：标签 `QStringLiteral("Slave ID")`，输入框使用 `QLineEdit`
   - Slave ID 输入框设置：
     - placeholder 文字如 `QStringLiteral("0x18")`
     - 使用 `QRegularExpressionValidator` 限制输入为十六进制格式（如 `0x[0-9A-Fa-f]{1,2}` 或不带 0x 前缀的纯十六进制）
     - 推荐使用 `QFormLayout` 排列 IC 选择和 Slave ID 两行
   - "Slave ID" 是技术术语，不使用 `tr()`

2. [severity: major] 新增 PMICGroup（PMIC 输出电压配置）
   File: src/tabs/configtab.cpp
   Detail: 在 UpperArea 3×4 网格的 (0,2) 位置新增 PMIC 配置 Group。
   Required:
   - 新建 `makePmicGroup(QWidget *parent)` 函数，使用 `makePanelGroup` + `applyPanelShadow`
   - Group 标题：`tr("PMIC")`
   - 内部使用 QFormLayout，包含 3 行：
     - 标签 `QStringLiteral("DRVDD")`，field 为 `QHBoxLayout`：`QDoubleSpinBox`（范围 0.0-10.0，步长 0.1，小数位 2）+ `QLabel(QStringLiteral("V"))`
     - 标签 `QStringLiteral("VCMVDD")`，同上
     - 标签 `QStringLiteral("IOVDD")`，同上
   - DRVDD / VCMVDD / IOVDD / "V" 均为技术参数，不使用 `tr()`
   - 在输入框下方添加一个按钮 `tr("配置 PMIC")`，使用 `makePrimaryButton` 风格，本阶段不接逻辑
   - layout 使用 `setAlignment(Qt::AlignTop)` 保持顶部对齐
   - 在 `ConfigTab` 构造函数中添加：`upperLayout->addWidget(makePmicGroup(upperArea), 0, 2);`

3. [severity: major] ConfigFileGroup 改为单行布局
   File: src/tabs/configtab.cpp (makeConfigFileGroup 函数)
   Detail: 当前文件选择和按钮分两行排列。
   Required:
   - 将所有控件合并为一行 `QHBoxLayout`：文件选择框(stretch=1) + Browse 按钮 + Write 按钮 + Read 按钮
   - 删除原有的两行布局（fileRow + buttonRow）
   - 控件间距使用 `Style::Size::FormSpacing`

## Test-Result

- 本轮为 UI 调整和新增控件，需工程师编译验证

## Decision

- 修复 Finding #1 + #2 + #3 后提交新的 `report_ui_refactor_*.md`
