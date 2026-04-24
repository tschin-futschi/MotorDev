#include "services/generatorservice.h"

#include "protocol/motor_protocol.h"
#include "services/commanddispatcher.h"

#include <QLoggingCategory>
#include <QPointer>
#include <QtMath>

Q_LOGGING_CATEGORY(lcGenerator, "motordev.generator")

namespace {
QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}

QString formatWord(quint16 value) {
    return QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
}
} // namespace

GeneratorService::GeneratorService(CommandDispatcher *dispatcher, QObject *parent)
    : QObject(parent)
    , m_dispatcher(dispatcher) {
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
    if (m_dispatcher == nullptr) {
        return;
    }

    const quint16 interval = static_cast<quint16>(qBound(1, intervalMs, 65535));
    const QByteArray payload = MotorProtocol::encodeStartLinearGen(addr, min, max, step, interval);
    qCInfo(lcGenerator).noquote()
        << QStringLiteral("%1 TX addr=%2 min=%3 max=%4 step=%5 intervalMs=%6 payload=%7")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdStartLinearGen)))
               .arg(formatWord(addr))
               .arg(min)
               .arg(max)
               .arg(step)
               .arg(interval)
               .arg(formatPayloadHex(payload));
    QPointer<GeneratorService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdStartLinearGen, payload, CommandDispatcher::High,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            if (guard != nullptr) {
                guard->onResponse(cmd, seq, data);
            }
        });
}

void GeneratorService::startCosine(qint16 amplitude, qint16 offset, double frequencyHz,
                                   const QVector<ScopeGeneratorCosineChannel> &channels) {
    if (m_dispatcher == nullptr || channels.isEmpty()) {
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
    const QByteArray payload = MotorProtocol::encodeStartCosineGen(
        amplitude, offset, freqX100, channelCount, channelPairs);
    qCInfo(lcGenerator).noquote()
        << QStringLiteral("%1 TX amplitude=%2 offset=%3 freqX100=%4 channels=%5 payload=%6")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdStartCosineGen)))
               .arg(amplitude)
               .arg(offset)
               .arg(freqX100)
               .arg(boundedCount)
               .arg(formatPayloadHex(payload));
    QPointer<GeneratorService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdStartCosineGen, payload, CommandDispatcher::High,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            if (guard != nullptr) {
                guard->onResponse(cmd, seq, data);
            }
        });
}

void GeneratorService::stop() {
    if (m_dispatcher == nullptr) {
        return;
    }

    const QByteArray payload = MotorProtocol::encodeStopGenerator();
    qCInfo(lcGenerator).noquote()
        << QStringLiteral("%1 TX payload=%2")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdStopGenerator)))
               .arg(formatPayloadHex(payload));
    QPointer<GeneratorService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdStopGenerator, payload, CommandDispatcher::High,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            if (guard != nullptr) {
                guard->onResponse(cmd, seq, data);
            }
        });
}

void GeneratorService::onResponse(uint8_t cmd, uint8_t /*seq*/, const QByteArray &data) {
    if (cmd == MotorProtocol::CmdStartLinearGen && data.isEmpty()) {
        m_mode = Mode::Linear;
        m_cosineChannelCount = 0;
        qCInfo(lcGenerator).noquote()
            << QStringLiteral("%1 RX ACK mode=Linear")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)));
        emit runningChanged(true);
        return;
    }

    if (cmd == MotorProtocol::CmdStartCosineGen && data.isEmpty()) {
        m_mode = Mode::Cosine;
        qCInfo(lcGenerator).noquote()
            << QStringLiteral("%1 RX ACK mode=Cosine channels=%2")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(m_cosineChannelCount);
        emit runningChanged(true);
        return;
    }

    if (cmd == MotorProtocol::CmdStopGenerator && data.isEmpty()) {
        m_mode = Mode::None;
        m_cosineChannelCount = 0;
        qCInfo(lcGenerator).noquote()
            << QStringLiteral("%1 RX ACK generator stopped")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)));
        emit runningChanged(false);
        return;
    }

    if (cmd == MotorProtocol::CmdErrorResponse) {
        qCWarning(lcGenerator).noquote()
            << QStringLiteral("%1 RX errorCode=0x%2 payload=%3")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(MotorProtocol::decodeErrorCode(data), 2, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(data))
                   .toUpper();
    }
}
