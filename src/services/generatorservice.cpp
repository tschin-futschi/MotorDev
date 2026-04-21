#include "services/generatorservice.h"

#include "services/registerservice.h"

#include <QDateTime>
#include <QTimer>
#include <QtMath>

namespace {
constexpr double kTwoPi = 6.28318530717958647692;
}

GeneratorService::GeneratorService(RegisterService *regService, QObject *parent)
    : QObject(parent)
    , m_regService(regService) {
    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &GeneratorService::onTick);
}

GeneratorService::~GeneratorService() = default;

bool GeneratorService::isRunning() const {
    return m_mode != Mode::None;
}

int GeneratorService::intervalMs() const {
    return m_timer != nullptr ? m_timer->interval() : 0;
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
    return m_cosineState.channels.size();
}

void GeneratorService::startLinear(quint16 addr, qint16 min, qint16 max, qint16 step, int intervalMs) {
    stop();

    if (m_timer == nullptr) {
        return;
    }

    m_linearState.addr = addr;
    m_linearState.min = min;
    m_linearState.max = max;
    m_linearState.step = step;
    m_linearState.current = min;
    m_linearState.ascending = true;
    m_mode = Mode::Linear;

    m_timer->setInterval(intervalMs);
    m_timer->start();
    emit runningChanged(true);
    onTick();
}

void GeneratorService::startCosine(qint16 amplitude, qint16 offset, double frequencyHz, int intervalMs,
                                   const QVector<ScopeGeneratorCosineChannel> &channels) {
    stop();

    if (m_timer == nullptr) {
        return;
    }

    m_cosineState.amplitude = amplitude;
    m_cosineState.offset = offset;
    m_cosineState.frequencyHz = frequencyHz;
    m_cosineState.channels = channels;
    m_cosineState.startTimeMs = QDateTime::currentMSecsSinceEpoch();
    m_mode = Mode::Cosine;

    m_timer->setInterval(intervalMs);
    m_timer->start();
    emit runningChanged(true);
    onTick();
}

void GeneratorService::stop() {
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    const bool wasRunning = isRunning();
    m_mode = Mode::None;
    m_cosineState.channels.clear();
    if (wasRunning) {
        emit runningChanged(false);
    }
}

void GeneratorService::onTick() {
    if (m_regService == nullptr) {
        return;
    }

    if (m_mode == Mode::Linear) {
        m_regService->writeSingleRow(-1, m_linearState.addr, m_linearState.current);
        emit linearTicked(m_linearState.addr, m_linearState.current);

        if (m_linearState.ascending) {
            const int nextValue = static_cast<int>(m_linearState.current) + static_cast<int>(m_linearState.step);
            if (nextValue > m_linearState.max) {
                m_linearState.current = m_linearState.max;
                m_linearState.ascending = false;
            } else {
                m_linearState.current = static_cast<qint16>(nextValue);
            }
        } else {
            const int nextValue = static_cast<int>(m_linearState.current) - static_cast<int>(m_linearState.step);
            if (nextValue < m_linearState.min) {
                m_linearState.current = m_linearState.min;
                m_linearState.ascending = true;
            } else {
                m_linearState.current = static_cast<qint16>(nextValue);
            }
        }
        return;
    }

    if (m_mode == Mode::Cosine) {
        const double elapsedSeconds = (QDateTime::currentMSecsSinceEpoch() - m_cosineState.startTimeMs) / 1000.0;
        QVector<RegisterService::RowRequest> writes;
        writes.reserve(m_cosineState.channels.size());

        for (const ScopeGeneratorCosineChannel &channel : m_cosineState.channels) {
            const double phaseRad = qDegreesToRadians(channel.phaseDeg);
            const double rawValue =
                static_cast<double>(m_cosineState.offset) +
                static_cast<double>(m_cosineState.amplitude) *
                    qCos(kTwoPi * m_cosineState.frequencyHz * elapsedSeconds + phaseRad);
            const int clampedValue = qBound(-32768, qRound(rawValue), 32767);

            RegisterService::RowRequest request;
            request.globalRow = -1;
            request.addr = channel.addr;
            request.value = static_cast<qint16>(clampedValue);
            writes.push_back(request);
        }

        if (!writes.isEmpty()) {
            m_regService->writeBatch(writes);
            emit cosineTicked(writes.size());
        }
    }
}
