// =============================================================================
// @file    aw86100_strategy.cpp
// @brief   AW86100 烧录策略实现 — 仅声明型号 / 描述,算法在基类
// =============================================================================
#include "services/flashstrategies/aw86100_strategy.h"

#include <utility>

AW86100Strategy::AW86100Strategy(SerialManager *serialManager,
                                  AwLocalIspStrategy::LogSink logSink)
    : AwLocalIspStrategy(serialManager, std::move(logSink)) {}

QString AW86100Strategy::icModel() const {
    return QStringLiteral("AW86100");
}

QString AW86100Strategy::icDescription() const {
    return QStringLiteral("Awinic 电机驱动 IC");
}
