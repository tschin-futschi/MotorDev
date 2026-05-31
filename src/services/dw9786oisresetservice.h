// =============================================================================
// @file    dw9786oisresetservice.h
// @brief   DW9786 上电 OISReset 服务
//
// DW9786 上电后 IC 默认处于 Sleep 状态，若不唤醒则寄存器全读 0xFFFF、不自举。
// 本服务执行供应商称为 OISReset 的固定寄存器序列，把 IC 切到工作态。
//
// 触发方式：由 MainWindow 在「串口连接成功 && 当前 Select IC = DW9786」时调用
// requestOisReset()。序列复用已验证的 DW9786 烧录写通道（Dw9786Bridge + vendor
// RamWriteA → 0x30 I2C 透传），并在 SerialManager 工作线程内同步执行（与烧录
// fast-path 相同的线程契约）。
//
// 线程：requestOisReset() 可在主线程调用，内部经 invokeMethod 投递到 SerialManager
// 工作线程执行 runOnSerialThread()。logMessage / finished 信号回主线程显示。
// =============================================================================

#pragma once

#include <QObject>
#include <QString>

#include <cstdint>

class SerialManager;

/// @brief DW9786 上电唤醒（OISReset）服务
class Dw9786OisResetService : public QObject {
    Q_OBJECT

public:
    /// @param serialManager  串口管理器（运行于独立工作线程）
    /// @param slaveId8bit     DW9786 OIS 从机地址（8-bit 形式，默认 0x24，与
    ///                        DW9786Strategy 默认一致）
    /// @param parent          QObject 父对象（通常为 MainWindow）
    explicit Dw9786OisResetService(SerialManager *serialManager,
                                   uint8_t slaveId8bit = 0x24,
                                   QObject *parent = nullptr);

    /// @brief 触发一次 OISReset（线程安全）
    ///
    /// 内部投递到 SerialManager 工作线程执行；可在主线程调用，不阻塞调用方。
    void requestOisReset();

signals:
    /// @brief 执行过程中的日志行（已封送到接收者线程，UI 端可直接显示）
    void logMessage(const QString &line);

    /// @brief 序列结束
    /// @param ok       true=序列已执行完成；false=前置校验失败/被跳过
    /// @param message  结果摘要
    void finished(bool ok, const QString &message);

private:
    /// @brief 真正执行 9 步 OISReset 序列，必须运行在 SerialManager 工作线程
    void runOnSerialThread();

    SerialManager *m_serialManager = nullptr;  ///< 串口管理器（工作线程）
    uint8_t        m_slaveId8bit   = 0x24;     ///< OIS 从机地址（8-bit）
};
