#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QToolButton;

class ScopeToolBar : public QWidget {
    Q_OBJECT

public:
    enum class ViewMode {
        Overlay,
        Stacked
    };

    explicit ScopeToolBar(QWidget *parent = nullptr);

    ViewMode viewMode() const;

signals:
    void viewModeChanged(ScopeToolBar::ViewMode mode);
    void samplingToggleRequested(bool running);
    void styleToggleRequested();

public slots:
    void setRunning(bool running);
    void setViewMode(ScopeToolBar::ViewMode mode);

private:
    void setupUi();
    void connectSignals();
    QToolButton *createToolButton(const QString &text, const QString &toolTip);
    void refreshState();

    QToolButton *m_overlayButton = nullptr;
    QToolButton *m_stackedButton = nullptr;
    QToolButton *m_styleButton = nullptr;
    QPushButton *m_samplingButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_running = false;
    ViewMode m_viewMode = ViewMode::Overlay;
};
