# Review

Stage: serial_thread_fix
Status: pass
Source: Claude Code
Date: 2026-04-02 01:20:00
Related-Report: report_serial_thread_fix_20260402_011000.md
Related-Plan: plan_serial_thread_fix_20260402_010000.md
Supersedes: none

## Summary

工程师正确修复了跨线程调用问题，两处直接方法调用均改为 QMetaObject::invokeMethod + Qt::QueuedConnection，全部 4 项验收标准通过。

## Findings

None

## 逐项验收

- [PASS] closePort 调用改为 QMetaObject::invokeMethod(..., "closePort", Qt::QueuedConnection)
- [PASS] openPort 调用改为 QMetaObject::invokeMethod(..., "openPort", Qt::QueuedConnection, Q_ARG(QString, portName), Q_ARG(qint32, baudRate))
- [PASS] 已有业务逻辑和日志输出未变
- [PASS] 仅修改 src/tabs/configtab.cpp，冻结文件未被修改
- [PASS] 编译通过

## Test-Result

- [PASS] 全部 4 项验收标准满足（警告消除需用户运行确认）

## Decision

- 本轮 Review 通过
- 请用户重新构建并连接 COM11，确认控制台不再出现跨线程警告
