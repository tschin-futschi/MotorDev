// =============================================================================
// @file    aw86100_strategy.h
// @brief   AW86100 烧录策略(Awinic)
//
// 与 AW86008 共用同一份 STM32 端 ISP 驱动(`Flash/AW/AW86008_86100/`),
// 烧录算法在 AwLocalIspStrategy 基类内统一实现。本类只 override 型号 / 描述。
// =============================================================================
#pragma once

#include "services/flashstrategies/aw_local_isp_strategy.h"

class SerialManager;

class AW86100Strategy : public AwLocalIspStrategy {
public:
    AW86100Strategy(SerialManager *serialManager,
                    AwLocalIspStrategy::LogSink logSink);

    QString icModel() const override;
    QString icDescription() const override;
};
