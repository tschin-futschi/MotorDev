#pragma once

#include <QWidget>

#include <cstdint>

class ConfigService;
class DeviceContext;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;
class SerialManager;
class QSplitter;

class ConfigTab : public QWidget {
    Q_OBJECT

public:
    explicit ConfigTab(SerialManager *serialManager, DeviceContext *deviceContext, QWidget *parent = nullptr);
    ~ConfigTab() override;

signals:
    void serialConnected(const QString &port, qint32 baudRate);
    void serialDisconnected();

private:
    void setupUi();
    void connectSignals();
    void refreshAvailablePorts();
    void setSerialControlsConnected(bool connected);

    ConfigService *m_service = nullptr;
    DeviceContext *m_deviceContext = nullptr;
    QWidget *m_sidebarContent = nullptr;
    QWidget *m_mainContent = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    QGroupBox *m_icGroup = nullptr;
    QGroupBox *m_serialGroup = nullptr;
    QGroupBox *m_pmicGroup = nullptr;

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
