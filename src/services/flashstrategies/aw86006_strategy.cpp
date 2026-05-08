// =============================================================================
// @file    aw86006_strategy.cpp
// @brief   AW86006 烧录策略实现
//
// TODO: 由用户后续填入真实烧录算法。
// 当前为联调用模拟实现：
//   - 按 1 KB / 100 ms 节奏推进 progress
//   - 每个 chunk 检查 cancelFlag，true 时立即退出
//   - 不发送任何真实串口数据
// =============================================================================
#include "services/flashstrategies/aw86006_strategy.h"

#include <QThread>

QString AW86006Strategy::icModel() const {
    return QStringLiteral("AW86006");
}

QString AW86006Strategy::icDescription() const {
    return QStringLiteral("Awinic 电机驱动 IC");
}

bool AW86006Strategy::flash(const QByteArray &firmware,
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
