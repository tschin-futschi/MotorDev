// =============================================================================
// @file    simulatorserial.h
// @brief   模拟器专用串口驱动（传输层）
//
// 接口与 SerialManager 对称（openPort/closePort/sendRawFrame），
// 在独立线程运行，仅供 SerialDebugTab / SimulatorService 使用。
// 不含重试和心跳逻辑，只做原始帧的收发。
// =============================================================================
#pragma once

#include "frameparser.h"

#include <QByteArray>
#include <QObject>
#include <QStringList>

#include <cstdint>

class QSerialPort;
class QThread;

class SimulatorSerial : public QObject {
    Q_OBJECT

public:
    explicit SimulatorSerial(QObject *parent = nullptr);
    ~SimulatorSerial() override;

    static QStringList availablePorts();

public slots:
    void init();
    void openPort(const QString &portName, qint32 baudRate);
    void closePort();
    void sendRawFrame(const QByteArray &frame);

signals:
    void connected();
    void disconnected();
    void frameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    void onSerialErrorOccurred(int error);

private:
    void closePortInternal(bool emitDisconnected);

    QThread *m_thread = nullptr;
    QSerialPort *m_serial = nullptr;
    FrameParser m_parser;
};
