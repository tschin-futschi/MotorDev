#pragma once

#include <QVector>
#include <QWidget>

#include <cstdint>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QSplitter;
class QTextEdit;
class SimulatorService;

class SerialDebugTab : public QWidget {
    Q_OBJECT

public:
    explicit SerialDebugTab(QWidget *parent = nullptr);
    ~SerialDebugTab() override;

signals:
    void debugStreamGenerated(uint8_t channelMask, const QVector<int16_t> &samples);
    void debugStreamActiveChanged(bool active);

private:
    void setupUi();
    void connectSignals();
    void refreshPortList();
    void setConnectedState(bool connected);
    void appendSysLog(const QString &message, bool isError = false);
    void appendLog(const QString &dir, uint8_t cmd, uint8_t seq, const QByteArray &data, const QString &note = {});

    SimulatorService *m_service = nullptr;
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
    QSplitter *m_mainSplitter = nullptr;
    QTextEdit *m_logEdit = nullptr;
    QPushButton *m_clearLogButton = nullptr;
};
