// =============================================================================
// @file    aw86100_strategy.cpp
// @brief   AW86100 烧录策略实现 — 仅区分型号文本，烧录算法复用 AW86006
// =============================================================================
#include "services/flashstrategies/aw86100_strategy.h"

QString AW86100Strategy::icModel() const {
    return QStringLiteral("AW86100");
}

QString AW86100Strategy::icDescription() const {
    return QStringLiteral("Awinic 电机驱动 IC（与 AW86006 烧录算法等同）");
}
