#pragma once

#include <QMainWindow>

class ActivityBar;
class DeviceContext;
class TopBar;
class QLabel;
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
    QStackedWidget *m_contentStack = nullptr;
    QWidget *m_statusBarWidget = nullptr;
    QLabel *m_statusLabel = nullptr;
};
