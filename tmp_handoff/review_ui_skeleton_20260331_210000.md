# Review

Stage: ui_skeleton
Status: changes_requested
Source: Claude Code
Date: 2026-03-31 21:00:00
Related-Report: report_ui_skeleton_20260331_184349.md
Related-Plan: plan_ui_skeleton_20260331_200000.md
Supersedes: none

## Summary

整体实现质量良好，文件结构清晰，编译通过无 error 无 warning，基本满足计划要求。以下问题需要修正后重新提交。

## Findings

1. [severity: major] CMakeLists.txt 缺少 SerialPort 和 PrintSupport 模块
   File: CMakeLists.txt
   Detail: `docs/tech_overview.md` 技术栈要求 Qt6::SerialPort 和 Qt6::PrintSupport（QCustomPlot 依赖），当前 `find_package` 和 `target_link_libraries` 只包含 Widgets、Svg、SvgWidgets。虽然本阶段不使用这两个模块，但构建配置应完整声明项目依赖，确保开发环境一次性配通。
   Required: `find_package` 加入 SerialPort PrintSupport 组件，`target_link_libraries` 加入 Qt6::SerialPort Qt6::PrintSupport

2. [severity: major] ActivityBar 设置按钮未放置在底部
   File: src/widgets/activitybar.cpp
   Detail: 计划要求"底部固定项：设置"，`design_spec.md` 也明确设置按钮在底部。当前实现中 `m_settingsButton` 在 `addStretch()` 之前添加，和 3 个功能按钮连续排列在顶部。应将 `m_settingsButton` 放在 stretch 之后，使其固定在底部。
   Required: 将 `m_settingsButton` 的 `addWidget` 调用移至 `addStretch()` 之后

3. [severity: minor] resources.qrc 资源路径可能无法正确解析
   File: resources/resources.qrc
   Detail: QRC 文件位于 `resources/` 目录，但 `<file>` 路径写的是 `motordev_logo.svg`（相对于 QRC 文件所在目录）。需确认 Qt AUTORCC 的路径解析是否正确——若 SVG 文件与 QRC 同目录则可以，但应验证运行时 `QSvgWidget(":/motordev_logo.svg")` 确实能加载到图片。
   Required: 已确认编译通过，但请在运行时验证 Logo 是否正常显示。若不显示，需检查路径或将 QRC `prefix` 调整。

4. [severity: minor] 连接指示灯圆角计算使用整数除法
   File: src/widgets/topbar.cpp:59
   Detail: `Style::Size::IndicatorSize / 2` 为整数除法，IndicatorSize=7 结果为 3 而非 3.5，圆形效果略有偏差。对于 7px 的小圆点影响很小，但作为规范性提醒。
   Required: 可保持现状，或改用 `IndicatorSize / 2 + 1` 使圆角更饱满。非阻塞项。

5. [severity: minor] Sidebar 波特率下拉框默认选中项应为 115200
   File: src/widgets/sidebar.cpp:66
   Detail: 波特率 ComboBox 的默认选中项为第一项 "9600"，但协议默认波特率为 115200（索引 4）。
   Required: 设置 `combo->setCurrentIndex(4)` 使默认选中 115200，或在 `makeCombo` 后额外设置。

## Test-Result

- [PASS] cmake configure 通过
- [PASS] cmake --build 通过，0 error 0 warning
- [PASS] 生成 MotorDev.exe
- [PASS] 文件结构符合计划（18 个文件全部到位）
- [PASS] style_constants.h 集中定义了颜色和尺寸，widget 代码中无硬编码色值
- [PASS] 所有 UI 字符串使用 tr() 包裹
- [PASS] 每个 UI 区域为独立 Widget 类
- [PASS] 侧边栏使用 QPropertyAnimation 实现收缩动画
- [PASS] ActivityBar 点击切换 ContentArea
- [PASS] 未修改冻结文件
- [NOT VERIFIED] 运行时界面视觉效果（需用户手动验收）

## Decision

- Status: changes_requested
- Finding 1、2、5 需要修正后重新提交
- Finding 3、4 为低优先级，可在本轮一并修正，也可延后
- 修正后提交新的 `report_ui_skeleton_*.md`
