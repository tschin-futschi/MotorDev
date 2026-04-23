#include "services/generatorservice.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QMetaObject>
#include <QtMath>

GeneratorService::GeneratorService(SerialManager *serialManager, QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager) {
    if (m_serialManager != nullptr) {
        connect(m_serialManager, &SerialManager::frameReceived, this, &GeneratorService::onFrameReceived);
    }
}

GeneratorService::~GeneratorService() = default;

bool GeneratorService::isRunning() const {
    return m_mode != Mode::None;
}

QString GeneratorService::modeLabel() const {
    switch (m_mode) {
    case Mode::Linear:
        return QStringLiteral("Linear");
    case Mode::Cosine:
        return QStringLiteral("Cosine");
    case Mode::None:
        return QString();
    }
    return QString();
}

int GeneratorService::cosineChannelCount() const {
    return m_cosineChannelCount;
}

void GeneratorService::startLinear(quint16 addr, qint16 min, qint16 max, qint16 step, int intervalMs) {
    if (m_serialManager == nullptr) {
        return;
    }

    const quint16 interval = static_cast<quint16>(qBound(1, intervalMs, 65535));
    const QByteArray payload = MotorProtocol::encodeStartLinearGen(addr, min, max, step, interval);
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
                              Q_ARG(uint8_t, MotorProtocol::CmdStartLinearGen), Q_ARG(QByteArray, payload));
}

void GeneratorService::startCosine(qint16 amplitude, qint16 offset, double frequencyHz,
                                   const QVector<ScopeGeneratorCosineChannel> &channels) {
    if (m_serialManager == nullptr || channels.isEmpty()) {
        return;
    }

    const int boundedCount = qBound(1, channels.size(), 3);
    const uint8_t channelCount = static_cast<uint8_t>(boundedCount);
    const quint16 freqX100 = static_cast<quint16>(qBound(1.0, frequencyHz * 100.0, 65535.0));

    QVector<QPair<quint16, qint16>> channelPairs;
    channelPairs.reserve(boundedCount);
    for (int i = 0; i < boundedCount; ++i) {
        const ScopeGeneratorCosineChannel &channel = channels.at(i);
        const qint16 phaseX10 = static_cast<qint16>(qBound(-3200.0, channel.phaseDeg * 10.0, 3200.0));
        channelPairs.push_back(qMakePair(channel.addr, phaseX10));
    }

    m_cosineChannelCount = boundedCount;
    const QByteArray payload = MotorProtocol::encodeStartCosineGen(amplitude, offset, freqX100, channelCount,
                                                                    channelPairs);
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
                              Q_ARG(uint8_t, MotorProtocol::CmdStartCosineGen), Q_ARG(QByteArray, payload));
}

void GeneratorService::stop() {
    if (m_serialManager == nullptr) {
        return;
    }

    const QByteArray payload = MotorProtocol::encodeStopGenerator();
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
                              Q_ARG(uint8_t, MotorProtocol::CmdStopGenerator), Q_ARG(QByteArray, payload));
}

void GeneratorService::onFrameReceived(uint8_t cmd, uint8_t /*seq*/, const QByteArray &data) {
    if (cmd == MotorProtocol::CmdStartLinearGen && data.isEmpty()) {
        m_mode = Mode::Linear;
        m_cosineChannelCount = 0;
        emit runningChanged(true);
        return;
    }

    if (cmd == MotorProtocol::CmdStartCosineGen && data.isEmpty()) {
        m_mode = Mode::Cosine;
        emit runningChanged(true);
        return;
    }

    if (cmd == MotorProtocol::CmdStopGenerator && data.isEmpty()) {
        m_mode = Mode::None;
        m_cosineChannelCount = 0;
        emit runningChanged(false);
        return;
    }

    if (cmd == MotorProtocol::CmdErrorResponse) {
        return;
    }
}
