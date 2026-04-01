# Review

Stage: ui_refactor
Status: changes_requested
Source: Claude Code
Date: 2026-04-01 21:30:00
Related-Report: report_ui_refactor_20260401_201000.md
Related-Plan: plan_ui_refactor_20260331_220000.md
Supersedes: review_ui_refactor_20260401_210000.md

## Summary

ConfigTab 当前所有控件都是构造函数或匿名 namespace 函数中的局部变量，无法从类外部访问，无法接入业务逻辑。违反 CLAUDE.md 中"UI 与业务逻辑严格分离"的架构约束。需要重构为分层结构。本次重构仅调整代码结构，不改变 UI 外观和布局。

## Findings

1. [severity: critical] ConfigTab 控件引用丢失，无法接入业务逻辑
   File: src/tabs/configtab.cpp, src/tabs/configtab.h
   Detail: portCombo、baudRateCombo、slaveIdEdit、三个 voltage spinbox、所有按钮等均为局部变量，创建后引用丢失。后续实现扫描、连接、PMIC 配置等功能时无法操作这些控件。
   Required:
   - 将以下控件提升为 `ConfigTab` 的成员变量（在 `configtab.h` 中声明）：
     - `m_icCombo` — IC 选择下拉框
     - `m_slaveIdEdit` — Slave ID 输入框
     - `m_portCombo` — 端口下拉框
     - `m_baudRateCombo` — 波特率下拉框
     - `m_scanButton` — 扫描按钮
     - `m_connectButton` — 连接按钮
     - `m_drvddSpin` — DRVDD 电压输入
     - `m_vcmvddSpin` — VCMVDD 电压输入
     - `m_iovddSpin` — IOVDD 电压输入
     - `m_pmicConfigButton` — 配置 PMIC 按钮
     - `m_fileCombo` — 配置文件选择框
     - `m_browseButton` — 浏览按钮
     - `m_writeButton` — 写入按钮
     - `m_readButton` — 读入按钮

2. [severity: critical] UI 构建和信号连接未分离
   File: src/tabs/configtab.cpp, src/tabs/configtab.h
   Detail: 当前所有 UI 构建逻辑在构造函数中一次完成，没有结构化分离。
   Required:
   - 将构造函数拆分为以下私有方法：
     - `setupUi()` — 纯 UI 构建：创建控件、设置样式、组装布局，将控件赋值给成员变量
     - `connectSignals()` — 信号槽连接（本阶段可为空方法体，留作后续逻辑接入）
   - 构造函数简化为：
     ```cpp
     ConfigTab::ConfigTab(QWidget *parent) : QWidget(parent) {
         setupUi();
         connectSignals();
     }
     ```
   - 匿名 namespace 中的通用工厂函数（`makeFormLabel`、`makeCombo`、`makeVoltageSpinBox`、`makePrimaryButton`、`applyPanelShadow`、`makePanelGroup`）可保留在匿名 namespace 中，它们是纯样式工具函数
   - 匿名 namespace 中的 Group 构建函数（`makeMotorIcGroup`、`makeSerialGroup`、`makePmicGroup`、`makeConfigFileGroup`）应改为 `ConfigTab` 的私有方法，以便在函数内部将控件赋值给成员变量。方法签名建议：
     - `QGroupBox *createIcGroup()`
     - `QGroupBox *createSerialGroup()`
     - `QGroupBox *createPmicGroup()`
     - `QGroupBox *createConfigFileGroup()`

3. [severity: major] 其他 3 个 Tab 同样需要重构
   File: src/tabs/registerrwtab.cpp, src/tabs/fwflashtab.cpp, src/tabs/oscilloscoptab.cpp 及对应 .h
   Detail: RegisterRwTab、FwFlashTab、OscilloscopTab 当前结构与 ConfigTab 相同，也是所有控件在构造函数中创建后丢失引用。虽然当前只有占位内容，但应尽早统一结构。
   Required:
   - 每个 Tab 的构造函数拆分为 `setupUi()` + `connectSignals()`
   - 当前占位控件无需提升为成员变量，但结构框架必须一致

## 约束

- **不改变任何 UI 外观和布局** — 本次是纯代码结构重构，用户看到的界面必须和重构前完全一致
- 通用样式工厂函数保留在匿名 namespace
- 新增的成员变量初始化为 `nullptr`
- 不引入任何业务逻辑代码，`connectSignals()` 方法体可为空

## Test-Result

- 本轮为结构重构，需工程师编译验证，确保 UI 外观不变

## Decision

- 修复 Finding #1 + #2 + #3 后提交新的 `report_ui_refactor_*.md`
