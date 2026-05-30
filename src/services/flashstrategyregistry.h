// =============================================================================
// @file    flashstrategyregistry.h
// @brief   烧录策略注册中心 — 按 IC 型号查找/枚举已注册策略
//
// 构造时一次性注册所有内置策略(AW86008 / AW86100 / DW9786 / DW9788)。
// 后续新增 IC 仅需在 flashstrategies/ 下新增子类并在 registerBuiltins()
// 加一行 add(...) 即可,UI 自动列出。
//
// AW86008 / AW86100 共用 AwLocalIspStrategy 基类(协议 0x32~0x37 本地 ISP),
// 仅需注入 SerialManager 和 LogSink;其他 IC 不依赖这些注入。
// =============================================================================
#pragma once

#include "services/flashstrategies/aw_local_isp_strategy.h"

#include <QList>
#include <QString>

#include <memory>
#include <vector>

class FlashStrategy;
class SerialManager;

class FlashStrategyRegistry {
public:
    FlashStrategyRegistry(SerialManager *serialManager,
                          AwLocalIspStrategy::LogSink awLogSink);
    ~FlashStrategyRegistry();

    FlashStrategyRegistry(const FlashStrategyRegistry &) = delete;
    FlashStrategyRegistry &operator=(const FlashStrategyRegistry &) = delete;

    /// @brief 已注册策略列表,按注册顺序
    QList<FlashStrategy *> all() const;

    /// @brief 按 IC 型号查找;找不到返回 nullptr
    FlashStrategy *find(const QString &icModel) const;

private:
    void registerBuiltins();
    void add(std::unique_ptr<FlashStrategy> s);

    SerialManager *m_serialManager = nullptr;
    AwLocalIspStrategy::LogSink m_awLogSink;
    std::vector<std::unique_ptr<FlashStrategy>> m_strategies;
};
