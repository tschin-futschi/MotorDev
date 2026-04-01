# Review

Stage: ui_refactor
Status: changes_requested
Source: Claude Code
Date: 2026-04-01 19:00:00
Related-Report: report_ui_refactor_20260401_171500.md
Related-Plan: plan_ui_refactor_20260331_220000.md
Supersedes: review_ui_refactor_20260401_180000.md

## Summary

用户要求在 ConfigTab UpperArea 网格新增 PMIC 配置 Group。纯 UI 控件添加，不接功能逻辑。

## Findings

1. [severity: major] 新增 PMICGroup（PMIC 输出电压配置）
   File: src/tabs/configtab.cpp
   Detail: 在 UpperArea 3×4 网格的 (0,3) 位置新增一个 PMIC 配置 Group。
   Required:
   - 新建 `makePmicGroup(QWidget *parent)` 函数，风格与 `makeMotorIcGroup` / `makeSerialGroup` 一致（使用 `makePanelGroup` + `applyPanelShadow`）
   - Group 标题：`tr("PMIC")`
   - 内部使用 QFormLayout，包含 3 行：
     - 标签 `QStringLiteral("DRVDD")`，输入框 `QDoubleSpinBox` 或 `QLineEdit`（接受浮点数），后跟 `QLabel("V")`
     - 标签 `QStringLiteral("VCMVDD")`，同上
     - 标签 `QStringLiteral("IOVDD")`，同上
   - 每行布局建议：`QHBoxLayout` 包含输入框 + "V" 标签，作为 QFormLayout 的 field
   - 输入框推荐使用 `QDoubleSpinBox`（范围和精度可先用默认值，如 0.0-10.0，步长 0.1，小数位 2）
   - DRVDD / VCMVDD / IOVDD 是芯片引脚名称，不应使用 `tr()`，用 `QStringLiteral`
   - "V" 单位标签不应使用 `tr()`
   - 在三行输入框下方（底部），添加一个按钮，文字为 `tr("配置 PMIC")`，使用 `makePrimaryButton` 风格，本阶段不接逻辑，留作后续实现
   - layout 使用 `setAlignment(Qt::AlignTop)` 保持顶部对齐
   - 在 `ConfigTab` 构造函数中添加：`upperLayout->addWidget(makePmicGroup(upperArea), 0, 2);`
   - 不接任何功能逻辑，纯控件摆放

## Test-Result

- 本轮为新增控件，需工程师编译验证

## Decision

- 修复 Finding #1 后提交新的 `report_ui_refactor_*.md`
