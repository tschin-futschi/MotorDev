// =============================================================================
// @file    aw86006_strategy.cpp
// @brief   AW86006 烧录策略实现 — 仅声明型号 / 描述 / DLL 文件名，算法在基类
// =============================================================================
#include "services/flashstrategies/aw86006_strategy.h"

#include <utility>

AW86006Strategy::AW86006Strategy(SerialManager *serialManager,
                                  AwSdkStrategy::LogSink logSink,
                                  AwSdkStrategy::AddrProvider addrProvider)
    : AwSdkStrategy(serialManager, std::move(logSink), std::move(addrProvider)) {
}

QString AW86006Strategy::icModel() const {
    return QStringLiteral("AW86006");
}

QString AW86006Strategy::icDescription() const {
    return QStringLiteral("Awinic 电机驱动 IC");
}

QString AW86006Strategy::dllPath() const {
    return QStringLiteral("AW86100.dll");  // AW86006 / AW86100 共用同一份 DLL
}
