#pragma once

#include "widgets/scopegeneratorpanel.h"

#include <QObject>
#include <QVector>

class RegisterService;
class QTimer;

class GeneratorService : public QObject {
    Q_OBJECT

public:
    explicit GeneratorService(RegisterService *regService, QObject *parent = nullptr);
    ~GeneratorService() override;

    bool isRunning() const;
    int intervalMs() const;
    QString modeLabel() const;
    int cosineChannelCount() const;

public slots:
    void startLinear(quint16 addr, qint16 min, qint16 max, qint16 step, int intervalMs);
    void startCosine(qint16 amplitude, qint16 offset, double frequencyHz, int intervalMs,
                     const QVector<ScopeGeneratorCosineChannel> &channels);
    void stop();

signals:
    void runningChanged(bool running);
    void linearTicked(quint16 addr, qint16 value);
    void cosineTicked(int channelCount);

private:
    void onTick();

    RegisterService *m_regService = nullptr;
    QTimer *m_timer = nullptr;

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

    enum class Mode { None, Linear, Cosine };

    LinearState m_linearState;
    CosineState m_cosineState;
    Mode m_mode = Mode::None;
};
