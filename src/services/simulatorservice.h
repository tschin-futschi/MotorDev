// =============================================================================
// @file    simulatorservice.h
// @brief   模拟器服务（服务层）— 协议命令分发、模拟响应生成、波形数据流线程
//
// 在 SerialDebugTab 中模拟 STM32 侧的行为：
// - 接收上位机命令并模拟响应（I2C 扫描、寄存器读写、PMIC 配置等）
// - 采样启动后，在独立 std::thread 中按采样间隔生成正弦波形数据
// - 通过 debugStreamGenerated 信号将模拟数据注入示波器
//
// 线程模型：
// - SimulatorSerial 在独立 QThread 中运行
// - 波形生成在 std::thread (streamWorkerLoop) 中运行，通过 mutex + condition_variable 同步
// - 主线程处理命令分发和 UI 交互
// =============================================================================
#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QVector>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

class SimulatorSerial;

class SimulatorService : public QObject {
    Q_OBJECT

public:
    explicit SimulatorService(QObject *parent = nullptr);
    ~SimulatorService() override;

    SimulatorSerial *simulator() const { return m_simulator; }

    void connectToPort(const QString &portName, qint32 baudRate);
    void disconnectPort();
    bool isConnected() const;

    void setResponseDelay(int delayMs) { m_responseDelayMs = delayMs; }
    void setScanAddresses(const QString &text);
    void setIcAddrResultSuccess(bool success);
    void setWriteResultSuccess(bool success);
    void setRegisterReadValue(const QString &text);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void logEntry(const QString &dir, uint8_t cmd, uint8_t seq, const QByteArray &data, const QString &note);
    void sysLogEntry(const QString &message, bool isError);
    void debugStreamGenerated(uint8_t channelMask, const QVector<int16_t> &samples);
    void debugStreamActiveChanged(bool active);

private slots:
    void onSimulatorConnected();
    void onSimulatorDisconnected();
    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);

private:
    struct PendingDebugFrame {
        uint8_t channelMask = 0x00;
        QVector<int16_t> samples;
    };

    static QString describeIncoming(uint8_t cmd, const QByteArray &data);

    void dispatchWithDelay(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void handleHeartbeat(uint8_t seq);
    void handleI2cScan(uint8_t seq, const QByteArray &data);
    void handleSetIcAddr(uint8_t seq, const QByteArray &data);
    void handleReset(uint8_t seq);
    void handleMotorTest(uint8_t seq);
    void handlePmicEnable(uint8_t seq);
    void handlePmicDisable(uint8_t seq);
    void handleSetPmicVoltage(uint8_t seq, const QByteArray &data);
    void handleReadRegister(uint8_t seq, const QByteArray &data);
    void handleWriteRegister(uint8_t seq, const QByteArray &data);
    void handleStartSampling(uint8_t seq);
    void handleStopSampling(uint8_t seq);
    void handleSetSampleInterval(uint8_t seq, const QByteArray &data);
    void handleSetSampleChannels(uint8_t seq, const QByteArray &data);
    void handleSetChannelRegisterMap(uint8_t seq, const QByteArray &data);
    void handleStartLinearGen(uint8_t seq, const QByteArray &data);
    void handleStartCosineGen(uint8_t seq, const QByteArray &data);
    void handleStopGenerator(uint8_t seq);
    void handleUnknownCommand(uint8_t cmd, uint8_t seq);

    void streamWorkerLoop();
    void emitOneStreamFrame();
    void sendResponseFrame(uint8_t seq, uint8_t cmd, const QByteArray &data);
    void sendErrorFrame(uint8_t seq, uint8_t errorCode);
    void sendDebugInfo(const QString &message);
    void queueDebugStreamFrame(uint8_t channelMask, const QVector<int16_t> &samples);
    void flushPendingDebugStream();
    qint16 registerReadValue() const;

    SimulatorSerial *m_simulator = nullptr;
    bool m_isConnected = false;
    int m_responseDelayMs = 0;
    QString m_scanAddressText = QStringLiteral("0x5A");
    bool m_icAddrSuccess = true;
    bool m_writeSuccess = true;
    bool m_pmicEnabled = false;
    quint16 m_pmicDrvvdd = 180;
    quint16 m_pmicIovdd = 280;
    quint16 m_pmicVcmvdd = 320;
    qint16 m_regReadValue = 0;
    qint16 m_streamBaseValue = 0;
    bool m_sampling = false;
    uint8_t m_sampleIntervalIndex = 0x04;
    uint8_t m_channelMask = 0x01;
    QVector<quint16> m_channelRegisterMap;
    quint32 m_streamTick = 0;
    QElapsedTimer m_streamElapsedTimer;
    qint64 m_lastStreamActualUs = -1;
    qint64 m_streamActualUsAccumulator = 0;
    int m_streamActualUsSamples = 0;
    std::thread m_streamThread;
    mutable std::mutex m_mutex;
    std::condition_variable m_streamCv;
    bool m_streamStopRequested = false;
    QVector<PendingDebugFrame> m_pendingDebugFrames;
    bool m_debugFlushQueued = false;
};
