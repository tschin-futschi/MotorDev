// =============================================================================
// @file    aw86006_strategy.h
// @brief   AW86006 烧录策略(Awinic)
//
// 烧录算法封装在 AwLocalIspStrategy 共用基类内(协议 0x32~0x37 本地 ISP 流程)。
// 本类只声明 IC 型号字串与描述;AW86006 / AW86100 共用同一份 STM32 端 ISP 驱动,
// UI 只通过 icModel() 区分。
// =============================================================================
#pragma once

#include "services/flashstrategies/aw_local_isp_strategy.h"

class SerialManager;

class AW86006Strategy : public AwLocalIspStrategy {
public:
    AW86006Strategy(SerialManager *serialManager,
                    AwLocalIspStrategy::LogSink logSink);

    QString icModel() const override;
    QString icDescription() const override;
};
