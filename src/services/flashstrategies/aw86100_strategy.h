// =============================================================================
// @file    aw86100_strategy.h
// @brief   AW86100 烧录策略（Awinic）
//
// 与 AW86006 烧录算法完全相同，二者共用同一份 AW86100.dll。
// 本类不再继承 AW86006Strategy（v1 plan 决议），直接继承 AwSdkStrategy 基类，
// 仅 override 型号 / 描述 / DLL 文件名。
// =============================================================================
#pragma once

#include "services/flashstrategies/aw_sdk_strategy.h"

class AW86100Strategy : public AwSdkStrategy {
public:
    AW86100Strategy(CommandDispatcher *dispatcher,
                    AwSdkStrategy::LogSink logSink,
                    AwSdkStrategy::AddrProvider addrProvider);

    QString icModel() const override;
    QString icDescription() const override;

protected:
    QString dllPath() const override;
};
