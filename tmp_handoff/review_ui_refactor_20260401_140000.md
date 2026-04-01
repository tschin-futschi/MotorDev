# Review

Stage: ui_refactor
Status: changes_requested
Source: Claude Code
Date: 2026-04-01 14:00:00
Related-Report: report_ui_refactor_20260401_104000.md
Related-Plan: plan_ui_refactor_20260331_220000.md
Supersedes: review_ui_refactor_20260401_120000.md

## Summary

结构方案正确，活动栏、Tab 架构、全局 Sidebar 移除均符合预期。发现 1 个动画 bug、1 个 tr() 误用，以及用户对 ConfigTab 布局提出新需求，需整体调整。

## Findings

### 代码缺陷

1. [severity: major] Sidebar 收缩动画只驱动 minimumWidth，未同步 maximumWidth
   File: src/widgets/sidebar.cpp:15-16, 76-89
   Detail: 构造函数同时设置了 `setMinimumWidth` 和 `setMaximumWidth` 为 `SidebarTotalWidth`，但 `QPropertyAnimation` 只动画 `minimumWidth` 属性。收缩时 `minimumWidth` 变为 `SidebarHandleWidth`，而 `maximumWidth` 仍为 `SidebarTotalWidth`，导致 layout 不会真正收缩侧边栏宽度。
   Required: 动画需同时驱动 `maximumWidth`。推荐方案：将 animation target 改为 `maximumWidth`，同时在动画的 `valueChanged` 信号中同步设置 `minimumWidth`，确保 min == max 始终成立。

2. [severity: minor] tr() 包裹了不应翻译的技术参数
   File: src/tabs/configtab.cpp:116-123
   Detail: `tr("9600")`, `tr("COM1")`, `tr("8")`, `tr("1")`, `tr("None")` 等技术参数不应翻译。
   Required: 纯技术参数字符串改用 `QStringLiteral`，仅 UI 标签文字保留 `tr()`。

### ConfigTab 布局重做（用户需求变更）

3. [severity: critical] ConfigTab ContentArea 布局需按以下方案重做
   File: src/tabs/configtab.cpp

   **Sidebar（侧边栏）**：清空内容，保留侧边栏结构，留作以后使用。

   **ContentArea 分为上下两部分**：

   ```
   ┌─────────────────────────────────────────────┐
   │  UpperArea (上部, 占 60%) — QGridLayout 3×4  │
   │  ┌──────────┬──────────┬──────┬──────┐      │
   │  │MotorIC   │ Serial   │(空)  │(空)  │ R1   │
   │  │Group     │ Group    │      │      │      │
   │  ├──────────┼──────────┼──────┼──────┤      │
   │  │  (空)    │  (空)    │(空)  │(空)  │ R2   │
   │  ├──────────┼──────────┼──────┼──────┤      │
   │  │  (空)    │  (空)    │(空)  │(空)  │ R3   │
   │  └──────────┴──────────┴──────┴──────┘      │
   ├─────────────────────────────────────────────┤
   │  LowerArea (下部, 占 40%) — QVBoxLayout      │
   │  ┌─────────────────────────────────────┐    │
   │  │ ConfigFileGroup (配置文件读写)        │    │
   │  └─────────────────────────────────────┘    │
   │  （后续可继续往下追加 Group）                 │
   └─────────────────────────────────────────────┘
   ```

   **UpperArea / LowerArea 的 60%/40% 分割**：使用 QSplitter 或 stretch ratio 实现。

   **G1 — MotorICGroup（马达IC选择）**：
   - 一个 QComboBox 下拉框
   - 默认选项：`AW86006`, `DW9786`, `DW9788`

   **G2 — SerialGroup（COM口配置）**：
   - 端口下拉框（默认为空，通过扫描填充）
   - 波特率下拉框（保留原有选项：9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600，默认 115200）
   - 扫描按钮（扫描可用 COM 口）
   - 连接按钮
   - **删除**数据位、停止位、校验位

   **G3-G12**：空占位，不放内容。

   **ConfigFileGroup（配置文件读写）**：
   - 配置文件选择框（QComboBox 或 QLineEdit + 浏览按钮）
   - 写入按钮
   - 读入按钮

   **删除**原有的 PMIC 配置和 I2C 配置占位组。

   Required: 按上述方案重写 `configtab.cpp`，侧边栏内容清空，ContentArea 按 Upper/Lower 分区重做。所有控件只做 UI 摆放，不接功能逻辑。新增色值/尺寸常量仍放 `style_constants.h`。

### 格式问题

4. [severity: minor] report 缺少 Based-On 字段
   File: tmp_handoff/report_ui_refactor_20260401_104000.md
   Detail: 缺少 `Based-On` 字段，`Author` 应为 `Source`。
   Required: 补充 `Based-On: plan_ui_refactor_20260331_220000.md`，`Author` 改为 `Source`。

## Test-Result

- [PASS] 活动栏 4 按钮顺序正确
- [PASS] Tab 切换正常
- [PASS] 每个 Tab 有独立 Sidebar
- [CONCERN] 侧边栏收缩动画有 bug（Finding #1）
- [PASS] MainWindow 无全局 Sidebar
- [PASS] 冻结文件未被修改
- [FAIL] ConfigTab 布局不符合用户最新需求（Finding #3）

## Decision

- Finding #1 (major) + #2 (minor) + #3 (critical) + #4 (minor)：必须全部修复后重提
- 修复后提交新的 `report_ui_refactor_*.md`
