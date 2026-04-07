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
