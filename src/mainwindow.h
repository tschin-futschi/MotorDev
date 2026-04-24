#pragma once

#include <QMainWindow>

class ActivityBar;
class CommandDispatcher;
class ConfigTab;
class DeviceContext;
class LogPanel;
class OscilloscopTab;
class QStackedWidget;
class QPushButton;
class RegisterRwTab;
class SerialDebugTab;
class SerialManager;
class TopBar;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();
    void connectSignals();

    SerialManager *m_serialManager = nullptr;
    CommandDispatcher *m_dispatcher = nullptr;
    DeviceContext *m_deviceContext = nullptr;
    TopBar *m_topBar = nullptr;
    ActivityBar *m_activityBar = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    LogPanel *m_logPanel = nullptr;
    QPushButton *m_logToggleButton = nullptr;
    ConfigTab *m_configTab = nullptr;
    RegisterRwTab *m_registerTab = nullptr;
    OscilloscopTab *m_scopeTab = nullptr;
    SerialDebugTab *m_debugTab = nullptr;
};
