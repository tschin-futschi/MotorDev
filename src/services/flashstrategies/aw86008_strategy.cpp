// =============================================================================
// @file    aw86008_strategy.cpp
// @brief   AW86008 烧录策略实现 — 仅声明型号 / 描述,算法在基类
// =============================================================================
#include "services/flashstrategies/aw86008_strategy.h"

#include <utility>

AW86008Strategy::AW86008Strategy(SerialManager *serialManager,
                                  AwLocalIspStrategy::LogSink logSink)
    : AwLocalIspStrategy(serialManager, std::move(logSink)) {}

QString AW86008Strategy::icModel() const {
    return QStringLiteral("AW86008");
}

QString AW86008Strategy::icDescription() const {
    return QStringLiteral("Awinic 电机驱动 IC");
}
