#pragma once

#include <QColor>
#include <QWidget>
#include <QVector>

class ScopeBottomPanel;
class ScopePlotWidget;
class ScopeStylePanel;
class QTimer;
class QSplitter;
class TopBar;
class QWidget;

class ScopePreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit ScopePreviewWidget(QWidget *parent = nullptr);
    ~ScopePreviewWidget() override;

private:
    struct ChannelState {
        bool enabled = false;
        QString description;
        QString addressText;
        QColor color;
        qreal lineWidth = 4.0;
        Qt::PenStyle lineStyle = Qt::SolidLine;
        bool showDataPoints = false;
    };

    void setupUi();
    void connectSignals();
    void refreshPlotData();
    void setRunning(bool running);
    void appendPreviewFrame();
    void resetStyleDefaults();
    int sampleIntervalUsForText(const QString &text) const;
    int displayWindowMsForText(const QString &text) const;
    uint8_t currentChannelMask() const;

    TopBar *m_topBar = nullptr;
    QSplitter *m_splitter = nullptr;
    QWidget *m_plotArea = nullptr;
    ScopePlotWidget *m_plotWidget = nullptr;
    QWidget *m_bottomPanelHost = nullptr;
    ScopeStylePanel *m_stylePanel = nullptr;
    ScopeBottomPanel *m_bottomPanel = nullptr;
    QTimer *m_previewTimer = nullptr;
    QVector<ChannelState> m_channels;
    QVector<ChannelState> m_defaultChannels;
    int m_sampleIntervalUs = 1000;
    int m_displayWindowMs = 50;
    int m_phase = 0;
    bool m_running = false;
};
