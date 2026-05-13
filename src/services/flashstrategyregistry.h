// =============================================================================
// @file    flashstrategyregistry.h
// @brief   烧录策略注册中心 — 按 IC 型号查找/枚举已注册策略
//
// 构造时一次性注册所有内置策略（AW86006 / AW86100 / DW9786 / DW9788）。
// 后续新增 IC 仅需在 flashstrategies/ 下新增子类并在 registerBuiltins()
// 加一行 add(...) 即可，UI 自动列出。
//
// AW86006 / AW86100 共用 AwSdkStrategy 基类，需要 CommandDispatcher（同步 I2C 透传）
// 和 AwSdkStrategy::LogSink（OutputLog 转发）；其他 IC 不依赖这些注入。
// =============================================================================
#pragma once

#include "services/flashstrategies/aw_sdk_strategy.h"

#include <QList>
#include <QString>

#include <memory>
#include <vector>

class FlashStrategy;
class CommandDispatcher;

class FlashStrategyRegistry {
public:
    FlashStrategyRegistry(CommandDispatcher *dispatcher,
                          AwSdkStrategy::LogSink awLogSink,
                          AwSdkStrategy::AddrProvider awAddrProvider);
    ~FlashStrategyRegistry();

    FlashStrategyRegistry(const FlashStrategyRegistry &) = delete;
    FlashStrategyRegistry &operator=(const FlashStrategyRegistry &) = delete;

    /// @brief 已注册策略列表，按注册顺序
    QList<FlashStrategy *> all() const;

    /// @brief 按 IC 型号查找；找不到返回 nullptr
    FlashStrategy *find(const QString &icModel) const;

private:
    void registerBuiltins();
    void add(std::unique_ptr<FlashStrategy> s);

    CommandDispatcher *m_dispatcher = nullptr;
    AwSdkStrategy::LogSink m_awLogSink;
    AwSdkStrategy::AddrProvider m_awAddrProvider;
    std::vector<std::unique_ptr<FlashStrategy>> m_strategies;
};
