# Plan

Stage: register_rwtab_ui
Status: active
Priority: medium
Source: Claude Code
Date: 2026-04-02 19:00:00
Depends-On: plan_decouple_protocol_20260402_180000.md
Supersedes: none

## Goal

对寄存器读写页（RegisterRwTab）进行 UI 布局改版：
在现有表格下方新增可拖动分割条与批量读写面板，其余布局和逻辑保持不变。

## Scope

1. 修改 `src/tabs/registerrwtab.h`：新增下半部分控件的成员变量声明
2. 修改 `src/tabs/registerrwtab.cpp`：
   - 将主内容区改为垂直 QSplitter（上：RegisterTable，下：批量读写面板）
   - 实现下半部分三个 GroupBox（5:3:3 比例），GroupBox 2、3 留空占位
   - 实现 GroupBox "批量读写" 的 4 行控件布局
3. 修改 `design_spec.md`（已由技术负责人完成，无需工程师操作）

## Non-Goals

- 批量写入 / 批量读出按钮的实际业务逻辑（点击暂不触发任何操作，留空）
- 浏览按钮的文件对话框逻辑（点击暂不触发任何操作，留空）
- 描述框和路径框的持久化
- DEC/HEX 切换、全部读取、全部写入按钮的任何改动
- 现有 RegisterTable 逻辑的任何改动

## Files-In-Scope

- `src/tabs/registerrwtab.h`
- `src/tabs/registerrwtab.cpp`

## Files-Frozen

- `src/widgets/registertable.h / .cpp`
- `src/tabs/configtab.h / .cpp`
- `src/protocol/motor_protocol.h / .cpp`
- `src/ui/style_constants.h`
- `src/mainwindow.h / .cpp`
- `CMakeLists.txt`
- 所有其他文件

## Constraints

### 布局约束

- 主内容区（RegisterTable 所在区域）改为 `QSplitter`，方向 `Qt::Vertical`
- QSplitter 上半部分：现有 RegisterTable 所在的 contentWidget（保持不变）
- QSplitter 下半部分：新建 `bottomWidget`，内含水平布局，放置三个 QGroupBox
- 三个 QGroupBox 比例通过 `QSplitter::setSizes` 或 `setStretchFactor` 实现 5:3:3

### GroupBox "批量读写" 行布局约束

每行控件从左到右顺序：
1. QPushButton（文字"批量写入"或"批量读出"），固定宽度，样式参照现有 Sidebar 主按钮风格
2. QLineEdit（描述框），可拉伸
3. QLineEdit（路径框），可拉伸，`setReadOnly(true)`
4. QPushButton（文字"浏览"），固定宽度，样式参照现有次级按钮风格

行间距、内边距使用现有 `Style::Size` 常量，不得硬编码数字。

### 成员变量命名约定

下半部分控件按行编号（0~3）命名，例如：
- `m_batchBtn[4]`（数组或单独命名 m_batchBtn0~3）
- `m_batchDescEdit[4]`
- `m_batchPathEdit[4]`
- `m_batchBrowseBtn[4]`

### 依赖说明

本计划依赖 `plan_decouple_protocol_20260402_180000.md` 完成后再实施，
避免两次修改 `registerrwtab.cpp` 产生合并冲突。
若协议解耦尚未完成，工程师应等待或向技术负责人确认并行实施方案。

## Acceptance

- 主内容区上下可通过拖动分割条调整比例
- 下半部分显示三个 GroupBox，标题分别为"批量读写"、空（或占位标题）、空
- GroupBox "批量读写" 内有 4 行，每行包含：操作按钮、描述框、路径框、浏览按钮
- 第 1、2 行按钮文字为"批量写入"，第 3、4 行为"批量读出"
- 点击批量写入/读出按钮和浏览按钮暂无任何效果（不崩溃即可）
- 现有读写、全部读取、全部写入、DEC/HEX 功能不受影响
- 编译通过，无新警告

## Notes

- 本次为纯 UI 布局改动，禁止修改任何业务逻辑
- GroupBox 2、3 标题留空或写占位文字（如"（待实现）"），由后续计划补充
