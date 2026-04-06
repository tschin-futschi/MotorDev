#pragma once

#include <QWidget>

class QLabel;
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
    void startRequested();
    void stopRequested();
    void zoomInRequested();
    void zoomOutRequested();
    void panLeftRequested();
    void panRightRequested();
    void fitRequested();
    void exportCsvRequested();
    void screenshotRequested();
    void fullScaleRequested();
    void settingsRequested();
    void viewModeChanged(ScopeToolBar::ViewMode mode);

public slots:
    void setRunning(bool running);
    void setViewMode(ScopeToolBar::ViewMode mode);

private:
    void setupUi();
    void connectSignals();
    QToolButton *createToolButton(const QString &text, const QString &toolTip);
    void refreshState();

    QToolButton *m_startButton = nullptr;
    QToolButton *m_stopButton = nullptr;
    QToolButton *m_zoomInButton = nullptr;
    QToolButton *m_zoomOutButton = nullptr;
    QToolButton *m_panLeftButton = nullptr;
    QToolButton *m_panRightButton = nullptr;
    QToolButton *m_fitButton = nullptr;
    QToolButton *m_overlayButton = nullptr;
    QToolButton *m_stackedButton = nullptr;
    QToolButton *m_exportButton = nullptr;
    QToolButton *m_screenshotButton = nullptr;
    QToolButton *m_fullScaleButton = nullptr;
    QToolButton *m_settingsButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_running = false;
    ViewMode m_viewMode = ViewMode::Overlay;
};
