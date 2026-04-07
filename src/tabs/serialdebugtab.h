#pragma once

#include <QByteArray>
#include <QWidget>

#include <cstdint>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;
class SimulatorSerial;

class SerialDebugTab : public QWidget {
    Q_OBJECT

public:
    explicit SerialDebugTab(QWidget *parent = nullptr);
    ~SerialDebugTab() override;

private:
    void setupUi();
    void connectSignals();
    QWidget *createConnectionBar();
    QWidget *createLeftPanel();
    QWidget *createLogPanel();
    static QString describeIncoming(uint8_t cmd, const QByteArray &data);

    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void dispatchWithDelay(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void handleHeartbeat(uint8_t seq);
    void handleI2cScan(uint8_t seq, const QByteArray &data);
    void handleSetIcAddr(uint8_t seq, const QByteArray &data);
    void handleReadRegister(uint8_t seq, const QByteArray &data);
    void handleWriteRegister(uint8_t seq, const QByteArray &data);
    void handleUnknownCommand(uint8_t cmd, uint8_t seq);

    void sendResponseFrame(uint8_t seq, uint8_t cmd, const QByteArray &data);
    void sendErrorFrame(uint8_t seq, uint8_t errorCode);
    void appendSysLog(const QString &message, bool isError = false);
    void appendLog(const QString &dir, uint8_t cmd, uint8_t seq, const QByteArray &data, const QString &note = {});
    void refreshPortList();
    void setConnectedState(bool connected);
    qint16 registerValueAt(quint16 addr) const;

    SimulatorSerial *m_simulator = nullptr;

    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QPushButton *m_scanButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_statusBadge = nullptr;

    QLineEdit *m_scanAddrEdit = nullptr;
    QComboBox *m_icAddrResultCombo = nullptr;
    QLineEdit *m_regReadValueEdit = nullptr;
    QComboBox *m_writeResultCombo = nullptr;
    QSpinBox *m_delaySpinBox = nullptr;

    QTextEdit *m_logEdit = nullptr;
    QPushButton *m_clearLogButton = nullptr;

    bool m_isConnected = false;
};
