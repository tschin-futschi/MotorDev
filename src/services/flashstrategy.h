// =============================================================================
// @file    flashstrategy.h
// @brief   烧录策略接口 — 不同 IC 的烧录算法抽象基类
//
// 每款马达驱动 IC 实现一个具体策略子类（见 flashstrategies/ 子目录）。
// flash() 由用户后续填入真实算法；当前所有子类均提供 stub 实现以便联调。
//
// flash() 调用约定：
// - 同步阻塞，运行在 worker 线程
// - 通过 progress 回调上报已发送字节数（不必每字节，按合理粒度即可）
// - cancelFlag 由外部翻转为 true 时应在下一安全点退出并返回 false
// - 失败时填充 errorMessage（成功时不写）
// =============================================================================
#pragma once

#include <QByteArray>
#include <QString>

#include <atomic>
#include <functional>

class FlashStrategy {
public:
    virtual ~FlashStrategy() = default;

    /// @brief 下拉框显示的 IC 型号文本
    virtual QString icModel() const = 0;

    /// @brief IC 描述（次要文字，可选）
    virtual QString icDescription() const { return {}; }

    /// @brief 烧录入口，由 FwFlashService 在 worker 线程调用
    /// @param firmware     连续二进制（已由 FirmwareParser 合并/校验）
    /// @param progress     已发送字节数回调；nullptr-safe
    /// @param cancelFlag   外部取消标志；翻为 true 时应尽快返回 false
    /// @param errorMessage 失败原因输出参数；成功时不写
    /// @return true=烧录成功；false=失败/取消（含 cancelFlag 触发）
    virtual bool flash(const QByteArray &firmware,
                       std::function<void(qint64 sentBytes)> progress,
                       const std::atomic<bool> &cancelFlag,
                       QString *errorMessage) = 0;
};
