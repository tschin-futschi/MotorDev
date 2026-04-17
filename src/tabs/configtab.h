#pragma once

#include <QByteArray>
#include <QWidget>
#include <memory>

#include <cstdint>

class DeviceContext;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class SerialManager;

namespace Ui {
class ConfigTab;
}

class ConfigTab : public QWidget {
    Q_OBJECT

public:
    explicit ConfigTab(SerialManager *serialManager, DeviceContext *deviceContext, QWidget *parent = nullptr);
    ~ConfigTab() override;

signals:
    void serialConnected(const QString &port, qint32 baudRate);
    void serialDisconnected();

private:
    void connectSignals();
    void refreshAvailablePorts();
    void setSerialControlsConnected(bool connected);
    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void handleScanResponse(const QByteArray &data);
    void handleSetAddrResponse();
    void handleErrorResponse(const QByteArray &data);

    std::unique_ptr<Ui::ConfigTab> ui;
    SerialManager *m_serialManager = nullptr;
    DeviceContext *m_deviceContext = nullptr;
    bool m_isSerialConnected = false;

    QComboBox *m_icCombo = nullptr;
    QComboBox *m_slaveIdCombo = nullptr;
    QPushButton *m_icScanButton = nullptr;
    QPushButton *m_icConnectButton = nullptr;
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudRateCombo = nullptr;
    QPushButton *m_scanButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QDoubleSpinBox *m_drvddSpin = nullptr;
    QDoubleSpinBox *m_vcmvddSpin = nullptr;
    QDoubleSpinBox *m_iovddSpin = nullptr;
    QPushButton *m_pmicConfigButton = nullptr;
    QComboBox *m_fileCombo = nullptr;
    QPushButton *m_browseButton = nullptr;
    QPushButton *m_writeButton = nullptr;
    QPushButton *m_readButton = nullptr;
};
