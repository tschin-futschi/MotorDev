// =============================================================================
// @file    aw86006_strategy.cpp
// @brief   AW86006 烧录策略实现 — 仅声明型号 / 描述,算法在基类
// =============================================================================
#include "services/flashstrategies/aw86006_strategy.h"

#include <utility>

AW86006Strategy::AW86006Strategy(SerialManager *serialManager,
                                  AwLocalIspStrategy::LogSink logSink)
    : AwLocalIspStrategy(serialManager, std::move(logSink)) {}

QString AW86006Strategy::icModel() const {
    return QStringLiteral("AW86006");
}

QString AW86006Strategy::icDescription() const {
    return QStringLiteral("Awinic 电机驱动 IC");
}
