// =============================================================================
// @file    dw9788_strategy.cpp
// @brief   DW9788 烧录策略实现
//
// TODO: 由用户后续填入真实烧录算法。当前为联调用模拟实现。
// =============================================================================
#include "services/flashstrategies/dw9788_strategy.h"

#include <QThread>

QString DW9788Strategy::icModel() const {
    return QStringLiteral("DW9788");
}

QString DW9788Strategy::icDescription() const {
    return QStringLiteral("Dongwoon 电机驱动 IC");
}

bool DW9788Strategy::flash(const QByteArray &firmware,
                           std::function<void(qint64 sentBytes)> progress,
                           const std::atomic<bool> &cancelFlag,
                           QString *errorMessage) {
    constexpr qint64 chunkBytes = 1024;
    constexpr int chunkSleepMs = 100;

    const qint64 total = firmware.size();
    qint64 sent = 0;
    while (sent < total) {
        if (cancelFlag.load()) {
            if (errorMessage) *errorMessage = QStringLiteral("用户取消");
            return false;
        }
        QThread::msleep(chunkSleepMs);
        sent = qMin(sent + chunkBytes, total);
        if (progress) progress(sent);
    }
    return true;
}
