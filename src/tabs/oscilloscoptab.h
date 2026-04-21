#pragma once

#include "ui/scopeviewmode.h"

#include <QKeyEvent>
#include <QString>
#include <QVector>
#include <QWidget>

class ScopeBottomPanel;
class ScopeChannelModel;
class ScopePlotWidget;
class RegisterService;
class ScopeService;
class ScopeStylePanel;
class SerialManager;
class QTimer;
class QSplitter;
class QVBoxLayout;

class OscilloscopTab : public QWidget {
    Q_OBJECT

public:
    explicit OscilloscopTab(SerialManager *serialManager, QWidget *parent = nullptr);
    ~OscilloscopTab() override;

signals:
    void runningChanged(bool running);
    void viewModeChanged(int mode);

public slots:
    void setDebugStreamActive(bool active);
    void ingestDebugStream(uint8_t channelMask, const QVector<int16_t> &samples);
    void onViewModeChangeRequested(int mode);
    void onSamplingToggleRequested(bool running);
    void onStyleToggleRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUi();
    void connectSignals();
    void refreshPlotData();
    void toggleStylePanel();
    void togglePlotFullscreen();
    void onPerfTimerTick();
    void onRegisterReadRequested(int row);
    void onRegisterWriteRequested(int row);
    void onRegisterStartRequested();
    void onRegisterStopRequested();
    void onRegisterClearRequested();
    void onCyclicWriteTick();

    static ScopeViewMode viewModeFromInt(int mode);
    static int viewModeToInt(ScopeViewMode mode);
    static bool parseRegisterNumber(const QString &text, quint16 *out);

    SerialManager *m_serialManager = nullptr;
    ScopeChannelModel *m_channelModel = nullptr;
    ScopeService *m_service = nullptr;
    RegisterService *m_regService = nullptr;
    QSplitter *m_splitter = nullptr;
    QWidget *m_plotArea = nullptr;
    QVBoxLayout *m_plotLayout = nullptr;
    ScopePlotWidget *m_plotWidget = nullptr;
    QWidget *m_bottomPanelHost = nullptr;
    ScopeStylePanel *m_stylePanel = nullptr;
    ScopeBottomPanel *m_bottomPanel = nullptr;
    ScopeViewMode m_viewMode = ScopeViewMode::Overlay;
    QTimer *m_perfTimer = nullptr;
    QTimer *m_cyclicWriteTimer = nullptr;
    QWidget *m_fullscreenWindow = nullptr;
    int m_plotLayoutIndex = -1;
    int m_cyclicWriteIndex = 0;

    uint8_t m_sampleIntervalIndex = 0x05;
    int m_displayWindowMs = 50;
};
