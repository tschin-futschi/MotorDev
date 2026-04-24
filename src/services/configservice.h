// ConfigService — 串口连接管理、I2C 扫描、IC 地址设置、帧响应处理
#pragma once

#include <QByteArray>
#include <QObject>
#include <QStringList>

#include <cstdint>

class DeviceContext;
class CommandDispatcher;
class SerialManager;
class QTimer;

class ConfigService : public QObject {
    Q_OBJECT

public:
    explicit ConfigService(SerialManager *serialManager,
                           CommandDispatcher *dispatcher,
                           DeviceContext *deviceContext,
                           QObject *parent = nullptr);
    ~ConfigService() override;

    QStringList availablePorts() const;

    void connectToPort(const QString &portName, qint32 baudRate);
    void disconnectPort();
    void startI2cScan();
    void setMotorIcAddress(uint8_t addr);
    void configurePmic(double drvvddV, double iovddV, double vcmvddV);

    bool isConnected() const { return m_isConnected; }

signals:
    void serialConnected(const QString &port, qint32 baudRate);
    void serialDisconnected();
    void serialError(const QString &message);
    void i2cScanResult(const QList<uint8_t> &addresses);
    void setAddrSuccess();
    void protocolError(uint8_t errorCode);
    void pmicConfigSuccess();
    void pmicConfigFailed(const QString &reason);

private slots:
    void onSerialConnected();
    void onSerialDisconnected();

private:
    enum class PmicState {
        Idle,
        WaitingVoltageAck,
        WaitingEnableAck,
    };

    static QString errorNameForCode(uint8_t errorCode);

    SerialManager *m_serialManager = nullptr;
    CommandDispatcher *m_dispatcher = nullptr;
    DeviceContext *m_deviceContext = nullptr;
    QTimer *m_pmicTimeoutTimer = nullptr;
    bool m_isConnected = false;
    QString m_connectedPort;
    qint32 m_connectedBaud = 0;
    PmicState m_pmicState = PmicState::Idle;
};
