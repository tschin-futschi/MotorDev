# Review

Stage: register_rwtab
Status: changes_requested
Source: Claude Code
Date: 2026-04-02 17:00:00
Related-Report: report_register_rwtab_20260402_111000.md
Related-Plan: plan_register_rwtab_20260402_170000.md
Supersedes: review_register_rwtab_20260402_160000.md

## Summary

Finding 1（ActivityBar 导航禁用）、Finding 2（HEX 0x 前缀）、Finding 3（val 字段写入 JSON）
均已正确实现。但值持久化的 save 触发逻辑未覆盖完整，导致重启后值仍会丢失，需补充修正。

## Already-Verified

- activitybar.h/cpp：setPageEnabled 接口实现正确 ✓
- mainwindow.cpp：serialConnected / serialDisconnected / 初始状态三处均调用 setPageEnabled ✓
- registertable.cpp：updateRowValue HEX 分支输出 "0x" + 大写数字 ✓
- registertable.cpp：saveConfig 已写入 "val"，loadConfig 已读取 "val" ✓

## Findings

1. [severity: major] 值列变化未触发 save，重启后值仍丢失
   File: src/widgets/registertable.cpp, src/tabs/registerrwtab.cpp
   Detail: saveConfig / loadConfig 的 val 字段已正确读写，但 save 从未在值变化时被触发：
   - registertable.cpp:203：configChanged 只在 columnMod==0（描述）或 1（地址）时 emit，
     columnMod==2（值）不触发，用户手动编辑值列不会自动保存
   - updateRowValue 有 QSignalBlocker，设备读回来的值不会触发 itemChanged，也不会保存
   - processNextInQueue 队列清空时不触发 save
   因此：用户读完寄存器，值显示在表格中，但 save 从未被调用，重启后值消失。

   Required:
   （A）registertable.cpp:203，将 configChanged 触发条件扩展到包含值列：
   ```
   if (columnMod == 0 || columnMod == 1 || columnMod == 2) {
       emit configChanged();
   }
   ```
   此改动覆盖用户手动编辑值列的场景。

   （B）registerrwtab.cpp：processNextInQueue 中队列清空且为读操作时，主动触发一次保存：
   ```
   if (m_pendingQueue.isEmpty()) {
       m_pendingRow = -1;
       if (!m_isWriteOp) {
           m_registerTable->saveConfig(configFilePath());
       }
       return;
   }
   ```
   此改动覆盖单行读、全部读取完成后自动保存的场景。
   写操作不需要触发保存（写操作不改变值列显示内容）。

## Test-Result

- ActivityBar 导航禁用：✓
- HEX 显示 0x 前缀：✓
- val 字段 JSON 读写：✓
- 值变化后 save 触发：✗（Finding 1）

## Decision

changes_requested。仅 1 条，涉及 2 个文件，改动量小。修正后重新提交 report。
