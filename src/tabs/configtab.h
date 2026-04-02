#pragma once

#include <QByteArray>
#include <QWidget>

#include <cstdint>

class DeviceContext;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;
class SerialManager;

class ConfigTab : public QWidget {
    Q_OBJECT

public:
    explicit ConfigTab(SerialManager *serialManager, DeviceContext *deviceContext, QWidget *parent = nullptr);

private:
    void setupUi();
    void connectSignals();
    void refreshAvailablePorts();
    void setSerialControlsConnected(bool connected);
    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void handleScanResponse(const QByteArray &data);
    void handleSetAddrResponse();
    void handleErrorResponse(const QByteArray &data);

    QGroupBox *createIcGroup();
    QGroupBox *createSerialGroup();
    QGroupBox *createPmicGroup();
    QWidget *createConfigFileRow();

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
