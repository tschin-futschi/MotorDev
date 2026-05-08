// =============================================================================
// @file    aw86006_strategy.h
// @brief   AW86006 烧录策略（Awinic）
//
// flash() 当前为联调用模拟实现，由用户后续填入真实烧录算法。
// 同时作为 AW86100Strategy 的基类（AW86100 烧录算法与本类完全等同）。
// =============================================================================
#pragma once

#include "services/flashstrategy.h"

class AW86006Strategy : public FlashStrategy {
public:
    QString icModel() const override;
    QString icDescription() const override;

    bool flash(const QByteArray &firmware,
               std::function<void(qint64 sentBytes)> progress,
               const std::atomic<bool> &cancelFlag,
               QString *errorMessage) override;
};
