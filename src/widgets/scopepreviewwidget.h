#pragma once

#include <memory>
#include <QColor>
#include <QWidget>
#include <QVector>

class ScopeBottomPanel;
class QTimer;

namespace Ui {
class ScopePreviewWidget;
}

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

    void connectSignals();
    void refreshPlotData();
    void setRunning(bool running);
    void appendPreviewFrame();
    void resetStyleDefaults();
    int sampleIntervalUsForText(const QString &text) const;
    int displayWindowMsForText(const QString &text) const;
    uint8_t currentChannelMask() const;

    std::unique_ptr<Ui::ScopePreviewWidget> ui;
    ScopeBottomPanel *m_bottomPanel = nullptr;
    QTimer *m_previewTimer = nullptr;
    QVector<ChannelState> m_channels;
    QVector<ChannelState> m_defaultChannels;
    int m_sampleIntervalUs = 1000;
    int m_displayWindowMs = 50;
    int m_phase = 0;
    bool m_running = false;
};
