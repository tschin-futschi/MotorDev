#include "services/cyclicwriteservice.h"

#include "services/commanddispatcher.h"
#include "services/registerservice.h"

#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(lcCyclicWrite, "motordev.cyclicwrite")

CyclicWriteService::CyclicWriteService(RegisterService *regService, QObject *parent)
    : QObject(parent)
    , m_regService(regService) {
    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &CyclicWriteService::onTick);
    if (m_regService != nullptr) {
        connect(m_regService, &RegisterService::rowWriteOk, this, [this](int globalRow) {
            if (!m_running || !m_waitingResponse || globalRow != m_lastWrittenRow) {
                return;
            }
            m_waitingResponse = false;
            m_consecutiveErrors = 0;
        });
        connect(m_regService, &RegisterService::rowWriteError, this, [this](int globalRow) {
            if (!m_running || !m_waitingResponse || globalRow != m_lastWrittenRow) {
                return;
            }
            m_waitingResponse = false;
            ++m_consecutiveErrors;
            qCWarning(lcCyclicWrite).noquote()
                << QStringLiteral("Write error row=%1 consecutive=%2")
                       .arg(globalRow)
                       .arg(m_consecutiveErrors);
            if (m_consecutiveErrors >= MaxConsecutiveErrors) {
                stopInternal("too many consecutive errors");
            }
        });
    }
}

CyclicWriteService::~CyclicWriteService() = default;

bool CyclicWriteService::isRunning() const {
    return m_running;
}

int CyclicWriteService::intervalMs() const {
    return m_timer != nullptr ? m_timer->interval() : 0;
}

void CyclicWriteService::setRowCount(int count) {
    m_rowCount = qMax(1, count);
    m_currentIndex = 0;
}

void CyclicWriteService::setRowDataProvider(RowDataProvider provider) {
    m_rowDataProvider = std::move(provider);
}

void CyclicWriteService::start(int intervalMs) {
    if (m_timer == nullptr || m_regService == nullptr || !m_rowDataProvider) {
        return;
    }

    m_currentIndex = 0;
    m_lastWrittenRow = -1;
    m_waitingResponse = false;
    m_consecutiveErrors = 0;
    m_timer->setInterval(intervalMs);
    m_timer->start();
    m_running = true;
    qCInfo(lcCyclicWrite).noquote()
        << QStringLiteral("Start cyclic write intervalMs=%1 rowCount=%2")
               .arg(intervalMs)
               .arg(m_rowCount);
    emit runningChanged(true);
    onTick();
}

void CyclicWriteService::stop() {
    stopInternal("manual");
}

void CyclicWriteService::stopInternal(const char *reason) {
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    m_waitingResponse = false;
    m_lastWrittenRow = -1;
    if (!m_running) {
        return;
    }
    m_running = false;
    qCInfo(lcCyclicWrite).noquote()
        << QStringLiteral("Stop cyclic write reason=%1").arg(QString::fromLatin1(reason));
    emit runningChanged(false);
}

void CyclicWriteService::onTick() {
    if (m_regService == nullptr || !m_rowDataProvider || m_rowCount <= 0) {
        return;
    }
    if (m_waitingResponse) {
        qCDebug(lcCyclicWrite) << "Skipped tick: waiting response";
        return;
    }

    for (int offset = 0; offset < m_rowCount; ++offset) {
        const int row = (m_currentIndex + offset) % m_rowCount;
        quint16 addr = 0;
        quint16 value = 0;
        if (!m_rowDataProvider(row, addr, value)) {
            continue;
        }

        m_regService->writeSingleRow(row, addr, static_cast<qint16>(value), CommandDispatcher::Low);
        m_currentIndex = (row + 1) % m_rowCount;
        m_lastWrittenRow = row;
        m_waitingResponse = true;
        return;
    }
}
