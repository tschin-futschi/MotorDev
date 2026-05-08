// =============================================================================
// @file    fwflashservice.cpp
// @brief   固件烧录服务实现
//
// 状态切换约定：所有 setState 调用均发生在主线程
//   - startFlash → Preparing → StoppingScope → ... → Flashing  （主线程同步切换）
//   - worker 内部仅运行 flash()，结束后通过 invokeMethod 回主线程切换 Completed/Failed/Cancelled
//
// 前置序列均为 fire-and-forget：任一回调即便抛异常也只记 warn，绝不阻断烧录
// （由产品确认；Service 层不假设这些远端动作必须成功）。
// =============================================================================
#include "services/fwflashservice.h"

#include "services/flashstrategy.h"
#include "services/flashstrategyregistry.h"

#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>
#include <QThread>

namespace {
Q_LOGGING_CATEGORY(lcFwFlash, "motordev.fwflash")
}

FwFlashService::FwFlashService(FlashStrategyRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry) {
}

FwFlashService::~FwFlashService() {
    shutdownWorker();
}

void FwFlashService::shutdownWorker() {
    if (m_cancelFlag) m_cancelFlag->store(true);
    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            m_workerThread->quit();
            m_workerThread->wait();
        }
    }
}

bool FwFlashService::isBusy() const {
    switch (m_state) {
    case State::Preparing:
    case State::StoppingScope:
    case State::StoppingGenerator:
    case State::StoppingCyclic:
    case State::DisablingPmic:
    case State::Flashing:
        return true;
    default:
        return false;
    }
}

void FwFlashService::setStopScopeCallback(StopCallback cb) { m_stopScope = std::move(cb); }
void FwFlashService::setStopGeneratorCallback(StopCallback cb) { m_stopGenerator = std::move(cb); }
void FwFlashService::setStopCyclicWriteCallback(StopCallback cb) { m_stopCyclic = std::move(cb); }
void FwFlashService::setDisablePmicCallback(StopCallback cb) { m_disablePmic = std::move(cb); }

void FwFlashService::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

void FwFlashService::emitLog(LogLevel level, const QString &message) {
    emit logMessage(level, message);
}

void FwFlashService::startFlash(const QString &icModel,
                                const QByteArray &firmware,
                                qint64 totalBytes) {
    if (isBusy()) {
        emitLog(LogLevel::Warn, QStringLiteral("已有烧录任务在执行，忽略本次请求"));
        return;
    }
    if (m_registry == nullptr) {
        emitLog(LogLevel::Error, QStringLiteral("策略注册中心未初始化"));
        emit finished(false, QStringLiteral("内部错误：策略注册中心未初始化"));
        return;
    }
    FlashStrategy *strategy = m_registry->find(icModel);
    if (strategy == nullptr) {
        emitLog(LogLevel::Error, QStringLiteral("未找到 IC 型号：%1").arg(icModel));
        emit finished(false, QStringLiteral("IC 型号未注册：%1").arg(icModel));
        return;
    }
    if (firmware.isEmpty()) {
        emitLog(LogLevel::Error, QStringLiteral("固件内容为空"));
        emit finished(false, QStringLiteral("固件内容为空"));
        return;
    }

    // 旧 worker 回收（防御性，正常路径下应已退出）
    shutdownWorker();

    m_totalBytes = totalBytes > 0 ? totalBytes : firmware.size();
    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);

    setState(State::Preparing);
    emit stageMessage(QStringLiteral("准备中..."));
    emit progressUpdated(0, m_totalBytes);
    emitLog(LogLevel::Info,
            QStringLiteral("开始烧录序列 (%1, %2 KB)")
                .arg(icModel)
                .arg(QString::number(firmware.size() / 1024.0, 'f', 1)));

    runPreflightAndFlash(strategy, firmware);
}

void FwFlashService::cancelFlash() {
    if (!isBusy()) return;
    if (m_cancelFlag) m_cancelFlag->store(true);
    emitLog(LogLevel::Warn, QStringLiteral("正在取消烧录..."));
}

namespace {

/// @brief 触发一个停止回调。回调内可能直接调用某 Service 的 stop 方法，
/// 也可能完全没设置；缺省时跳过即可（fire-and-forget 语义）。
void invokeIfSet(const FwFlashService::StopCallback &cb) {
    if (cb) cb();
}

}  // namespace

void FwFlashService::runPreflightAndFlash(FlashStrategy *strategy, const QByteArray &firmware) {
    setState(State::StoppingScope);
    emit stageMessage(QStringLiteral("停止采样..."));
    emitLog(LogLevel::Info, QStringLiteral("停止采样"));
    invokeIfSet(m_stopScope);

    setState(State::StoppingGenerator);
    emit stageMessage(QStringLiteral("停止信号发生器..."));
    emitLog(LogLevel::Info, QStringLiteral("停止信号发生器"));
    invokeIfSet(m_stopGenerator);

    setState(State::StoppingCyclic);
    emit stageMessage(QStringLiteral("停止循环写入..."));
    emitLog(LogLevel::Info, QStringLiteral("停止循环写入"));
    invokeIfSet(m_stopCyclic);

    setState(State::DisablingPmic);
    emit stageMessage(QStringLiteral("关闭 PMIC..."));
    emitLog(LogLevel::Info, QStringLiteral("关闭 PMIC"));
    invokeIfSet(m_disablePmic);

    // 前置期间用户已点取消则提前结束
    if (m_cancelFlag->load()) {
        setState(State::Cancelled);
        emit stageMessage(QStringLiteral("已取消"));
        emitLog(LogLevel::Info, QStringLiteral("已取消"));
        emit finished(false, QStringLiteral("已取消"));
        setState(State::Idle);
        return;
    }

    setState(State::Flashing);
    emit stageMessage(QStringLiteral("烧录中..."));
    emitLog(LogLevel::Info,
            QStringLiteral("调用 %1::flash() (%2 KB)")
                .arg(strategy->icModel())
                .arg(QString::number(firmware.size() / 1024.0, 'f', 1)));

    // worker 线程：仅跑 flash()，结果通过 invokeMethod 回主线程
    auto cancelFlag = m_cancelFlag;       // shared_ptr 拷贝，保证 worker 内访问安全
    const qint64 total = m_totalBytes;
    QPointer<FwFlashService> self(this);  // 防御 service 在 worker 完成前被销毁

    m_workerThread = QThread::create([self, strategy, firmware, cancelFlag, total]() {
        QString errorMsg;
        auto progress = [self, total](qint64 sent) {
            if (self.isNull()) return;
            // 跨线程 emit：AutoConnection 自动转 QueuedConnection 到主线程
            emit self->progressUpdated(sent, total);
            const double pct = total > 0 ? (sent * 100.0 / total) : 0.0;
            emit self->stageMessage(
                QStringLiteral("烧录中 %1 KB / %2 KB (%3%)")
                    .arg(QString::number(sent / 1024.0, 'f', 1))
                    .arg(QString::number(total / 1024.0, 'f', 1))
                    .arg(QString::number(pct, 'f', 1)));
        };
        const bool ok = strategy->flash(firmware, progress, *cancelFlag, &errorMsg);
        const bool wasCancelled = cancelFlag->load();

        if (self.isNull()) return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, ok, wasCancelled, errorMsg, total]() {
                if (self.isNull()) return;
                if (ok) {
                    emit self->progressUpdated(total, total);
                    self->setState(State::Completed);
                    emit self->stageMessage(QStringLiteral("烧录完成"));
                    self->emitLog(LogLevel::Ok, QStringLiteral("烧录完成"));
                    emit self->finished(true, QStringLiteral("成功"));
                } else if (wasCancelled) {
                    self->setState(State::Cancelled);
                    emit self->stageMessage(QStringLiteral("已取消"));
                    self->emitLog(LogLevel::Info, QStringLiteral("已取消"));
                    emit self->finished(false, QStringLiteral("已取消"));
                } else {
                    self->setState(State::Failed);
                    emit self->stageMessage(QStringLiteral("失败：%1").arg(errorMsg));
                    self->emitLog(LogLevel::Error, QStringLiteral("烧录失败：%1").arg(errorMsg));
                    emit self->finished(false, errorMsg);
                }
                self->setState(State::Idle);
            },
            Qt::QueuedConnection);
    });

    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
    QPointer<QThread> threadPtr(m_workerThread);
    connect(m_workerThread, &QThread::finished, this, [this, threadPtr]() {
        if (m_workerThread == threadPtr.data()) m_workerThread = nullptr;
    });
    m_workerThread->start();
}
