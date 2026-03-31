#pragma once

#include <QMainWindow>

class ActivityBar;
class Sidebar;
class TopBar;
class QLabel;
class QPushButton;
class QStackedWidget;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void updateSidebarToggleButton();

    TopBar *m_topBar = nullptr;
    ActivityBar *m_activityBar = nullptr;
    Sidebar *m_sidebar = nullptr;
    QPushButton *m_sidebarToggleButton = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    QWidget *m_statusBarWidget = nullptr;
    QLabel *m_statusLabel = nullptr;
};
