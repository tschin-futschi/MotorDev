// =============================================================================
// @file    flashstrategyregistry.cpp
// @brief   烧录策略注册中心实现
// =============================================================================
#include "services/flashstrategyregistry.h"

#include "services/flashstrategies/aw86008_strategy.h"
#include "services/flashstrategies/aw86100_strategy.h"
#include "services/flashstrategies/dw9786_strategy.h"
#include "services/flashstrategies/dw9788_strategy.h"
#include "services/flashstrategy.h"

#include <utility>

FlashStrategyRegistry::FlashStrategyRegistry(SerialManager *serialManager,
                                              AwLocalIspStrategy::LogSink awLogSink)
    : m_serialManager(serialManager)
    , m_awLogSink(std::move(awLogSink)) {
    registerBuiltins();
}

FlashStrategyRegistry::~FlashStrategyRegistry() = default;

void FlashStrategyRegistry::registerBuiltins() {
    add(std::make_unique<AW86008Strategy>(m_serialManager, m_awLogSink));
    add(std::make_unique<AW86100Strategy>(m_serialManager, m_awLogSink));
    // DW9786：复用 AW 的 LogSink 类型；eraseCalibration=false（OTA），
    // slaveId8bit=0x24（vendor _SLV_OIS_ 默认值）
    add(std::make_unique<DW9786Strategy>(m_serialManager, m_awLogSink,
                                          /*eraseCalibration=*/false,
                                          /*slaveId8bit=*/0x24));
    // DW9788：复用 AW 的 LogSink 类型（DW9788Strategy::LogSink 是 AW 的别名）
    // eraseCalibration=false（OTA 默认，保留校准），slaveId8bit=0x48（PDF 示例值）
    add(std::make_unique<DW9788Strategy>(m_serialManager, m_awLogSink,
                                          /*eraseCalibration=*/false,
                                          /*slaveId8bit=*/0x48));
}

void FlashStrategyRegistry::add(std::unique_ptr<FlashStrategy> s) {
    m_strategies.push_back(std::move(s));
}

QList<FlashStrategy *> FlashStrategyRegistry::all() const {
    QList<FlashStrategy *> r;
    r.reserve(static_cast<int>(m_strategies.size()));
    for (const auto &s : m_strategies) r.append(s.get());
    return r;
}

FlashStrategy *FlashStrategyRegistry::find(const QString &icModel) const {
    for (const auto &s : m_strategies) {
        if (s->icModel() == icModel) return s.get();
    }
    return nullptr;
}
