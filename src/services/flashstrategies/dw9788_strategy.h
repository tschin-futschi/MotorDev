// =============================================================================
// @file    dw9788_strategy.h
// @brief   DW9788 烧录策略（Dongwoon）
//
// flash() 当前为联调用模拟实现，由用户后续填入真实烧录算法。
// =============================================================================
#pragma once

#include "services/flashstrategy.h"

class DW9788Strategy : public FlashStrategy {
public:
    QString icModel() const override;
    QString icDescription() const override;

    bool flash(const QByteArray &firmware,
               std::function<void(qint64 sentBytes)> progress,
               const std::atomic<bool> &cancelFlag,
               QString *errorMessage) override;
};
