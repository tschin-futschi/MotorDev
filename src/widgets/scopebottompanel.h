#pragma once

#include <memory>
#include <QWidget>

class ScopeChannelStrip;
class QEvent;
class ScopeGeneratorPanel;
class ScopeRegisterPanel;
class QMenu;
class QWidget;
namespace Ui {
class ScopeBottomPanel;
}

class ScopeBottomPanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeBottomPanel(QWidget *overlayHost, QWidget *parent = nullptr);
    ~ScopeBottomPanel() override;
    void setRunning(bool running);

signals:
    void registerReadRequested(int row);
    void registerWriteRequested(int row);
    void clearPanelRequested();
    void loadParamsRequested();
    void channelToggled(int index, bool enabled);
    void channelDescriptionChanged(int index, const QString &text);
    void channelAddressChanged(int index, const QString &text);
    void sampleIntervalChanged(const QString &text);
    void displayWindowChanged(const QString &text);
    void runningToggled(bool running);
    void captureNoteChanged(const QString &text);
    void yAxisAutoRequested();
    void yAxisManualRequested(double minValue, double maxValue);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void connectSignals();
    QWidget *createOverlayWindow(const QString &title, QWidget *content, const QSize &size);
    void refreshPanels();
    void refreshYAxisButton();
    bool promptManualYAxisRange(double &minValue, double &maxValue);

    std::unique_ptr<Ui::ScopeBottomPanel> ui;
    QWidget *m_overlayHost = nullptr;
    ScopeRegisterPanel *m_registerPanel = nullptr;
    ScopeGeneratorPanel *m_generatorPanel = nullptr;
    ScopeChannelStrip *m_channels[8] = {};
    QWidget *m_registerWindow = nullptr;
    QWidget *m_generatorWindow = nullptr;
    QMenu *m_yAxisMenu = nullptr;
    bool m_channelsVisible = false;
    bool m_running = false;
    bool m_yAxisAuto = true;
    double m_manualYMin = -1000.0;
    double m_manualYMax = 1000.0;
};

