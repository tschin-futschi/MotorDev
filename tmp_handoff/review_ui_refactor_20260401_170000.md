# Review

Stage: ui_refactor
Status: changes_requested
Source: Claude Code
Date: 2026-04-01 17:00:00
Related-Report: report_ui_refactor_20260401_145500.md
Related-Plan: plan_ui_refactor_20260331_220000.md
Supersedes: review_ui_refactor_20260401_160000.md

## Summary

功能 Review 已通过，用户视觉确认后要求改进 ConfigTab 主内容区的观感。本轮为纯视觉调整，不涉及功能或结构变更。

## Findings

1. [severity: major] 空占位格子产生视觉噪音
   File: src/tabs/configtab.cpp:212-218
   Detail: 当前 3×4 网格中 10 个空位放置了带边框的 QFrame，看起来像未完成的空表格。
   Required:
   - 删除 `makeEmptyGridCell` 函数及其所有调用
   - 网格中空位不放任何 widget，保留 QGridLayout 3×4 结构不变（方便后续扩展）
   - MotorICGroup 和 SerialGroup 保持在 (0,0) 和 (0,1) 位置
   - 行列 stretch 保留，使空位自然留白

2. [severity: major] 有内容的 Group 控件被拉伸，缺乏紧凑感和视觉层次
   File: src/tabs/configtab.cpp:76-128
   Detail: Group 内部 `addStretch()` 导致控件在垂直方向被分散；Group 外观与空位没有主次区分。
   Required:
   - MotorICGroup 和 SerialGroup 内部去掉末尾的 `addStretch()`，改为设置 layout 的 `setAlignment(Qt::AlignTop)`，使控件顶部对齐紧凑排列
   - 给有内容的 Group 添加轻微的 `QGraphicsDropShadowEffect`（偏移 0-1px，模糊半径 4-6px，颜色半透明灰），使其从背景中浮起
   - 如果 `QGraphicsDropShadowEffect` 在当前环境不可用或效果不理想，备选方案为：加粗边框为 2px 并使用比 `DefaultBorder` 更深的颜色（在 `style_constants.h` 中新增 `ActivePanelBorder` 常量）
   - ConfigFileGroup 同样应用相同的阴影/边框效果，保持一致

3. [severity: minor] 可删除不再使用的 EmptyPanelRadius 常量
   File: src/ui/style_constants.h:69
   Detail: 删除空占位格子后，`EmptyPanelRadius` 常量不再被引用。
   Required: 删除 `EmptyPanelRadius` 常量。

## Test-Result

- 本轮为视觉调整，需工程师修改后编译验证并建议用户视觉确认

## Decision

- 修复 Finding #1 + #2 + #3 后提交新的 `report_ui_refactor_*.md`
