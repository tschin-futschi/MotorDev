// =============================================================================
// @file    generatorservice.h
// @brief   信号发生器服务（服务层）— Linear/Cosine 发生器协议命令发送与状态管理
//
// 封装信号发生器的启动（Linear/Cosine）和停止协议命令。
// 波形实际由 STM32 执行，上位机仅下发参数和启停命令。
// 通过 CommandDispatcher 提交高优先级命令，内部管理 ACK 超时。
// =============================================================================
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
    void startSawtooth(quint16 addr, qint16 min, qint16 max, qint16 step);
    void stop();

signals:
    void runningChanged(bool running);

private:
    void onResponse(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void handleTimeout();

    CommandDispatcher *m_dispatcher = nullptr;
    QTimer *m_ackTimeoutTimer = nullptr;

    enum class Mode { None, Linear, Cosine, Sawtooth };
    enum class PendingOp { None, StartLinear, StartCosine, StartSawtooth, Stop };
    Mode m_mode = Mode::None;
    PendingOp m_pendingOp = PendingOp::None;
    int m_cosineChannelCount = 0;
};
