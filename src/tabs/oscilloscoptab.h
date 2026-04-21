#pragma once

#include "ui/scopeviewmode.h"
#include "widgets/scopegeneratorpanel.h"

#include <QKeyEvent>
#include <QString>
#include <QStringList>
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
    void onGeneratorLinearStart(quint16 addr, qint16 min, qint16 max, qint16 step, int intervalMs);
    void onGeneratorCosineStart(qint16 amplitude, qint16 offset, double frequencyHz, int intervalMs,
                                const QVector<ScopeGeneratorCosineChannel> &channels);
    void onGeneratorStop();
    void onGeneratorTick();
    void refreshMarqueeStatus();

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
    QTimer *m_generatorTimer = nullptr;
    QWidget *m_fullscreenWindow = nullptr;
    int m_plotLayoutIndex = -1;
    int m_cyclicWriteIndex = 0;

    struct LinearState {
        quint16 addr = 0;
        qint16 min = 0;
        qint16 max = 0;
        qint16 step = 1;
        qint16 current = 0;
        bool ascending = true;
    };

    struct CosineState {
        qint16 amplitude = 0;
        qint16 offset = 0;
        double frequencyHz = 1.0;
        QVector<ScopeGeneratorCosineChannel> channels;
        qint64 startTimeMs = 0;
    };

    enum class GeneratorMode { None, Linear, Cosine };

    LinearState m_linearState;
    CosineState m_cosineState;
    GeneratorMode m_generatorMode = GeneratorMode::None;
    bool m_scopeRunning = false;
    bool m_cyclicWriteRunning = false;
    QString m_sampleIntervalText = QStringLiteral("1000us");

    uint8_t m_sampleIntervalIndex = 0x05;
    int m_displayWindowMs = 50;
};
