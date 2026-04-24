#pragma once

#include "widgets/scopegeneratorpanel.h"

#include <QByteArray>
#include <QObject>

class CommandDispatcher;
class QTimer;

class GeneratorService : public QObject {
    Q_OBJECT

public:
    explicit GeneratorService(CommandDispatcher *dispatcher, QObject *parent = nullptr);
    ~GeneratorService() override;

    bool isRunning() const;
    QString modeLabel() const;
    int cosineChannelCount() const;

public slots:
    void startLinear(quint16 addr, qint16 min, qint16 max, qint16 step, int intervalMs);
    void startCosine(qint16 amplitude, qint16 offset, double frequencyHz,
                     const QVector<ScopeGeneratorCosineChannel> &channels);
    void stop();

signals:
    void runningChanged(bool running);

private:
    void onResponse(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void handleTimeout();

    CommandDispatcher *m_dispatcher = nullptr;
    QTimer *m_ackTimeoutTimer = nullptr;

    enum class Mode { None, Linear, Cosine };
    enum class PendingOp { None, StartLinear, StartCosine, Stop };
    Mode m_mode = Mode::None;
    PendingOp m_pendingOp = PendingOp::None;
    int m_cosineChannelCount = 0;
};
