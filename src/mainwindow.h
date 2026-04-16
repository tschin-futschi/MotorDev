#pragma once

#include <memory>
#include <QMainWindow>

class ConfigTab;
class DeviceContext;
class OscilloscopTab;
class RegisterRwTab;
class SerialDebugTab;
class SerialManager;
class QWidget;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();
    void connectSignals();

    std::unique_ptr<Ui::MainWindow> ui;
    SerialManager *m_serialManager = nullptr;
    DeviceContext *m_deviceContext = nullptr;
    ConfigTab *m_configTab = nullptr;
    RegisterRwTab *m_registerTab = nullptr;
    OscilloscopTab *m_scopeTab = nullptr;
    SerialDebugTab *m_debugTab = nullptr;
};
