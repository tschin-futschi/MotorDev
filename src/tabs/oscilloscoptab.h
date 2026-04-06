#pragma once

#include "widgets/scopetoolbar.h"

#include <QWidget>

class ScopeBottomPanel;
class ScopePlotWidget;
class ScopeToolBar;
class Sidebar;

class OscilloscopTab : public QWidget {
    Q_OBJECT

public:
    explicit OscilloscopTab(QWidget *parent = nullptr);

private:
    void setupUi();
    void connectSignals();
    void logPlaceholderAction(const QString &action);
    QWidget *createSidebarContent();

    Sidebar *m_sidebar = nullptr;
    ScopeToolBar *m_toolBar = nullptr;
    ScopePlotWidget *m_plotWidget = nullptr;
    ScopeBottomPanel *m_bottomPanel = nullptr;
    bool m_running = false;
    ScopeToolBar::ViewMode m_viewMode = ScopeToolBar::ViewMode::Overlay;
};
