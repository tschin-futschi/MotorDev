#include "services/cyclicwriteservice.h"

#include "services/registerservice.h"

#include <QTimer>

CyclicWriteService::CyclicWriteService(RegisterService *regService, QObject *parent)
    : QObject(parent)
    , m_regService(regService) {
    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &CyclicWriteService::onTick);
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
    m_timer->setInterval(intervalMs);
    m_timer->start();
    m_running = true;
    emit runningChanged(true);
    onTick();
}

void CyclicWriteService::stop() {
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    if (!m_running) {
        return;
    }
    m_running = false;
    emit runningChanged(false);
}

void CyclicWriteService::onTick() {
    if (m_regService == nullptr || !m_rowDataProvider || m_rowCount <= 0) {
        return;
    }

    for (int offset = 0; offset < m_rowCount; ++offset) {
        const int row = (m_currentIndex + offset) % m_rowCount;
        quint16 addr = 0;
        quint16 value = 0;
        if (!m_rowDataProvider(row, addr, value)) {
            continue;
        }

        m_regService->writeSingleRow(row, addr, static_cast<qint16>(value));
        m_currentIndex = (row + 1) % m_rowCount;
        return;
    }
}
