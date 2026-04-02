#pragma once

#include <QWidget>

class QLabel;
class QComboBox;
class QSvgWidget;

class TopBar : public QWidget {
    Q_OBJECT

public:
    explicit TopBar(QWidget *parent = nullptr);

public slots:
    void onSerialConnected(const QString &port, qint32 baudRate);
    void onSerialDisconnected();

private:
    QLabel *m_connectionIndicator = nullptr;
    QLabel *m_portValueLabel = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QComboBox *m_languageCombo = nullptr;
};
