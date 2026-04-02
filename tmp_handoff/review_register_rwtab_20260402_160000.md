# Review

Stage: register_rwtab
Status: changes_requested
Source: Claude Code
Date: 2026-04-02 16:00:00
Related-Report: report_register_rwtab_20260402_111000.md
Related-Plan: plan_register_rwtab_20260402_170000.md
Supersedes: review_register_rwtab_20260402_153000.md

## Summary

运行时验证发现 3 处问题：ActivityBar 导航按钮未随串口状态禁用、HEX 模式值显示缺少 0x 前缀、
值列数据重启后丢失。需修正后重新提交 report。

## Findings

1. [severity: major] 串口未连接时仍可通过 ActivityBar 切换到其他页面
   File: src/widgets/activitybar.h, src/widgets/activitybar.cpp, src/mainwindow.cpp
   Detail: 当前对非配置页只调用了 widget 的 setEnabled(false)，使内容灰化，但 ActivityBar
   的导航按钮（m_registerButton / m_flashButton / m_scopeButton）没有对应禁用，
   用户仍可点击切换。需在 ActivityBar 暴露按钮启用控制接口，并在连接状态变化时调用。
   Required:
   - activitybar.h：新增公共方法：
     void setPageEnabled(int page, bool enabled);
   - activitybar.cpp：实现该方法，根据 page 值定位对应按钮并调用 setEnabled(enabled)：
     ```
     void ActivityBar::setPageEnabled(int page, bool enabled) {
         switch (page) {
         case RegisterPage: m_registerButton->setEnabled(enabled); break;
         case FlashPage:    m_flashButton->setEnabled(enabled);    break;
         case ScopePage:    m_scopeButton->setEnabled(enabled);    break;
         default: break;
         }
     }
     ```
   - mainwindow.cpp：
     在 serialConnected lambda 中补加：
       m_activityBar->setPageEnabled(ActivityBar::RegisterPage, true);
       m_activityBar->setPageEnabled(ActivityBar::FlashPage, true);
       m_activityBar->setPageEnabled(ActivityBar::ScopePage, true);
     在 serialDisconnected lambda 中补加：
       m_activityBar->setPageEnabled(ActivityBar::RegisterPage, false);
       m_activityBar->setPageEnabled(ActivityBar::FlashPage, false);
       m_activityBar->setPageEnabled(ActivityBar::ScopePage, false);
     在构造函数末尾初始状态处补加：
       m_activityBar->setPageEnabled(ActivityBar::RegisterPage, false);
       m_activityBar->setPageEnabled(ActivityBar::FlashPage, false);
       m_activityBar->setPageEnabled(ActivityBar::ScopePage, false);

2. [severity: minor] HEX 模式值显示缺少 0x 前缀
   File: src/widgets/registertable.cpp
   Detail: updateRowValue 在 HEX 模式下输出 "1A2B" 格式，用户期望显示 "0x1A2B"。
   parseNumericText 已支持 "0x" 前缀解析，显示格式补全即可，不影响输入解析。
   Required:
   将 updateRowValue 中 Hex 分支的格式字符串改为：
   ```
   item->setText(QStringLiteral("0x") + QString::number(value, 16).toUpper().rightJustified(4, '0'));
   ```
   Note: 不能用 QStringLiteral("0x%1").arg(...).toUpper()，toUpper() 会把 "0x" 也变成 "0X"。
   前缀 "0x" 固定小写，数字部分单独 toUpper() 后拼接。

3. [severity: minor] 值列数据重启后丢失
   File: src/widgets/registertable.cpp
   Detail: saveConfig 只保存了 "desc" 和 "addr"，"val" 未写入 JSON，导致重启后值列清空。
   Required:
   - saveConfig 中，取 valueItem 并加入 entry：
     ```
     auto *valueItem = m_table->item(row, valueCol(group));
     entry[QStringLiteral("val")] = valueItem != nullptr ? valueItem->text() : QString{};
     ```
   - loadConfig 中，读取 "val" 并 setText 到值列（在 QSignalBlocker 范围内）：
     ```
     if (auto *valueItem = m_table->item(row, valueCol(group)); valueItem != nullptr) {
         valueItem->setText(entry.value(QStringLiteral("val")).toString());
     }
     ```
   Note: 旧 JSON 文件无 "val" 字段时，value() 返回 QJsonValue::Undefined，toString() 返回空字符串，
   不会崩溃，兼容旧文件。

## Test-Result

- 4组×20行布局：✓
- 列宽、按钮文字 R/W：✓
- DEC/HEX 值转换逻辑：✓（切换时数字正确重算）
- HEX 显示格式（0x 前缀）：✗（Finding 2）
- 串口未连接时页面内容灰化：✓
- 串口未连接时 ActivityBar 导航按钮禁用：✗（Finding 1）
- 值列持久化：✗（Finding 3）

## Decision

changes_requested。Finding 1 优先，涉及 3 个文件；Finding 2、3 均在 registertable.cpp，改动极小，可同步完成。
修正后重新提交 report_register_rwtab_*.md。
