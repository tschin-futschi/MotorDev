// =============================================================================
// @file    flashstrategyregistry.cpp
// @brief   烧录策略注册中心实现
// =============================================================================
#include "services/flashstrategyregistry.h"

#include "services/flashstrategies/aw86006_strategy.h"
#include "services/flashstrategies/aw86100_strategy.h"
#include "services/flashstrategies/dw9786_strategy.h"
#include "services/flashstrategies/dw9788_strategy.h"
#include "services/flashstrategy.h"

#include <utility>

FlashStrategyRegistry::FlashStrategyRegistry(CommandDispatcher *dispatcher,
                                              AwSdkStrategy::LogSink awLogSink)
    : m_dispatcher(dispatcher)
    , m_awLogSink(std::move(awLogSink)) {
    registerBuiltins();
}

FlashStrategyRegistry::~FlashStrategyRegistry() = default;

void FlashStrategyRegistry::registerBuiltins() {
    add(std::make_unique<AW86006Strategy>(m_dispatcher, m_awLogSink));
    add(std::make_unique<AW86100Strategy>(m_dispatcher, m_awLogSink));
    add(std::make_unique<DW9786Strategy>());
    add(std::make_unique<DW9788Strategy>());
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
