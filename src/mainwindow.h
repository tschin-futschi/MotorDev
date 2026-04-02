#pragma once

#include <QMainWindow>

class ActivityBar;
class ConfigTab;
class DeviceContext;
class LogPanel;
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
    SerialManager *m_serialManager = nullptr;
    DeviceContext *m_deviceContext = nullptr;
    TopBar *m_topBar = nullptr;
    ActivityBar *m_activityBar = nullptr;
    ConfigTab *m_configTab = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    LogPanel *m_logPanel = nullptr;
    QWidget *m_statusBarWidget = nullptr;
    QPushButton *m_logToggleButton = nullptr;
};
