#pragma once

#include <QMainWindow>

class ActivityBar;
class ConfigTab;
class DeviceContext;
class LogPanel;
class OscilloscopTab;
class RegisterRwTab;
class SerialDebugTab;
class TopBar;
class QPushButton;
class QStackedWidget;
class SerialManager;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void setupUi();
    void connectSignals();

    SerialManager *m_serialManager = nullptr;
    DeviceContext *m_deviceContext = nullptr;
    TopBar *m_topBar = nullptr;
    ActivityBar *m_activityBar = nullptr;
    ConfigTab *m_configTab = nullptr;
    RegisterRwTab *m_registerTab = nullptr;
    OscilloscopTab *m_scopeTab = nullptr;
    SerialDebugTab *m_debugTab = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    LogPanel *m_logPanel = nullptr;
    QWidget *m_statusBarWidget = nullptr;
    QPushButton *m_logToggleButton = nullptr;
};
