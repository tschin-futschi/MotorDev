#pragma once

#include <QtTypes>
#include <QWidget>
class QComboBox;
class QFrame;
class QLabel;
class QToolButton;
class QSvgWidget;

class TopBar : public QWidget {
    Q_OBJECT

public:
    explicit TopBar(QWidget *parent = nullptr);
    ~TopBar() override;

signals:
    void viewModeChanged(int mode);
    void styleToggleRequested();

public slots:
    void onSerialConnected(const QString &port, qint32 baudRate);
    void onSerialDisconnected();
    void setScopeControlsVisible(bool visible);
    void setViewMode(int mode);

private:
    void setupUi();
    void connectSignals();

    QSvgWidget *m_logo = nullptr;
    QLabel *m_portValueLabel = nullptr;
    QLabel *m_connectionIndicator = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QToolButton *m_viewModeButton = nullptr;
    QToolButton *m_styleButton = nullptr;
    QComboBox *m_languageCombo = nullptr;
    int m_viewMode = 0;
};
