#pragma once

#include <memory>
#include <QtTypes>
#include <QWidget>

namespace Ui {
class TopBar;
}

class TopBar : public QWidget {
    Q_OBJECT

public:
    explicit TopBar(QWidget *parent = nullptr);
    ~TopBar() override;

signals:
    void viewModeChanged(int mode);
    void samplingToggleRequested(bool running);
    void styleToggleRequested();

public slots:
    void onSerialConnected(const QString &port, qint32 baudRate);
    void onSerialDisconnected();
    void setScopeControlsVisible(bool visible);
    void setRunning(bool running);
    void setViewMode(int mode);

private:
    void connectSignals();

    std::unique_ptr<Ui::TopBar> ui;
    bool m_running = false;
    int m_viewMode = 0;
};