#pragma once

#include <QMainWindow>

class ActivityBar;
class TopBar;
class QLabel;
class QStackedWidget;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    TopBar *m_topBar = nullptr;
    ActivityBar *m_activityBar = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    QWidget *m_statusBarWidget = nullptr;
    QLabel *m_statusLabel = nullptr;
};
