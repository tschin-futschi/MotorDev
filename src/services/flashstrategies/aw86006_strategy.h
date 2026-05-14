// =============================================================================
// @file    aw86006_strategy.h
// @brief   AW86006 烧录策略（Awinic）
//
// 烧录算法封装在 AwSdkStrategy 共用基类内（5 步流程 + DLL 动态加载 + 回调）。
// 本类只声明 IC 型号字串、描述与 DLL 文件名；与 AW86100 烧录算法完全相同，
// 二者共用同一份 AW86100.dll，UI 只通过 icModel() 区分。
// =============================================================================
#pragma once

#include "services/flashstrategies/aw_sdk_strategy.h"

class SerialManager;

class AW86006Strategy : public AwSdkStrategy {
public:
    AW86006Strategy(SerialManager *serialManager,
                    AwSdkStrategy::LogSink logSink,
                    AwSdkStrategy::AddrProvider addrProvider);

    QString icModel() const override;
    QString icDescription() const override;

protected:
    QString dllPath() const override;
};
