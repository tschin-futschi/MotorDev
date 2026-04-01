#pragma once

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLineEdit;
class QPushButton;

class ConfigTab : public QWidget {
    Q_OBJECT

public:
    explicit ConfigTab(QWidget *parent = nullptr);

private:
    void setupUi();
    void connectSignals();

    QGroupBox *createIcGroup();
    QGroupBox *createSerialGroup();
    QGroupBox *createPmicGroup();
    QGroupBox *createConfigFileGroup();

    QComboBox *m_icCombo = nullptr;
    QLineEdit *m_slaveIdEdit = nullptr;
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
