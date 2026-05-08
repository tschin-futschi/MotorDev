// =============================================================================
// @file    aw86100_strategy.h
// @brief   AW86100 烧录策略（Awinic）
//
// AW86100 与 AW86006 烧录算法完全等同，因此本类直接继承自 AW86006Strategy，
// 仅 override icModel() 与 icDescription() 用于 UI 区分；flash() 不重写。
// =============================================================================
#pragma once

#include "services/flashstrategies/aw86006_strategy.h"

class AW86100Strategy : public AW86006Strategy {
public:
    QString icModel() const override;
    QString icDescription() const override;
    // flash() 不 override，直接复用 AW86006Strategy::flash()
};
