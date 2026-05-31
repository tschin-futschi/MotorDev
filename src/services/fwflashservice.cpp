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

#include "protocol/motor_protocol.h"
#include "serialmanager.h"
#include "services/flashstrategy.h"
#include "services/flashstrategyregistry.h"

#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>

namespace {
Q_LOGGING_CATEGORY(lcFwFlash, "motordev.fwflash")
}

FwFlashService::FwFlashService(FlashStrategyRegistry *registry,
                               SerialManager *serialManager,
                               QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_serialManager(serialManager) {
}

FwFlashService::~FwFlashService() {
    shutdownWorker();
}

void FwFlashService::shutdownWorker() {
    // 烧录任务跑在 SerialManager 工作线程内（fast-path）。析构时只能尽力让任务自我退出：
    // 翻 cancelFlag 让 strategy 在下一次 I2C 透传后协作式中止；不能强行 quit 该线程
    // （那是 SerialManager 自己的工作线程，析构后整个应用都不工作了）。
    if (m_cancelFlag) m_cancelFlag->store(true);
}

bool FwFlashService::isBusy() const {
    switch (m_state) {
    case State::Preparing:
    case State::StoppingScope:
    case State::StoppingGenerator:
    case State::StoppingCyclic:
    case State::Flashing:
        return true;
    default:
        return false;
    }
}

void FwFlashService::setStopScopeCallback(StopCallback cb) { m_stopScope = std::move(cb); }
void FwFlashService::setStopGeneratorCallback(StopCallback cb) { m_stopGenerator = std::move(cb); }
void FwFlashService::setStopCyclicWriteCallback(StopCallback cb) { m_stopCyclic = std::move(cb); }

void FwFlashService::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

void FwFlashService::emitLog(LogLevel level, const QString &message) {
    emit logMessage(level, message);
}

void FwFlashService::emitProgressPct(int pct) {
    const int clamped = qBound(0, pct, 100);
    const qint64 equiv = (m_totalBytes > 0)
                            ? (m_totalBytes * static_cast<qint64>(clamped) / 100)
                            : 0;
    emit progressUpdated(equiv, m_totalBytes);
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
    emit stageMessage(tr("准备中..."));
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

void FwFlashService::onFlashExecProgress(quint8 phase, quint32 done, quint32 total) {
    // 仅在 Flashing 状态接受进度帧。其他状态收到属于乱序帧（STM32 上次烧录结束太迟
    // 才到的尾帧 / 协议不同步），写一条 warn 日志并丢弃。
    if (m_state != State::Flashing) {
        qCWarning(lcFwFlash) << "ignore stray 0x38 progress: state="
                              << static_cast<int>(m_state)
                              << " phase=" << phase << " done=" << done << " total=" << total;
        return;
    }
    if (total == 0U) {
        // 协议要求 total ≥ 1，这里防御性兜底，避免除零
        return;
    }
    if (done > total) {
        done = total;
    }

    // ERASE 阶段折算：DATA 之后的 5% 区间，即 [kPctData, kPctData + kPctErase]
    // WRITE 阶段折算：ERASE 之后的 70% 区间，即 [kPctData + kPctErase, kPctData + kPctErase + kPctWrite]
    using namespace FwFlashProgress;
    int pct = 0;
    QString stageText;
    switch (phase) {
    case static_cast<quint8>(MotorProtocol::FlashExecPhase::Erase):
        pct = kPctData +
              static_cast<int>(static_cast<qint64>(kPctErase) * done / total);
        stageText = (done < total)
                        ? tr("擦除 Flash...")
                        : tr("擦除完成");
        break;
    case static_cast<quint8>(MotorProtocol::FlashExecPhase::Write):
        pct = kPctData + kPctErase +
              static_cast<int>(static_cast<qint64>(kPctWrite) * done / total);
        stageText = tr("写入 Flash %1 / %2 块").arg(done).arg(total);
        break;
    default:
        qCWarning(lcFwFlash) << "unknown phase in 0x38 progress:" << phase;
        return;
    }
    emitProgressPct(pct);
    emit stageMessage(stageText);
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
    emit stageMessage(tr("停止采样..."));
    emitLog(LogLevel::Info, QStringLiteral("停止采样"));
    invokeIfSet(m_stopScope);

    setState(State::StoppingGenerator);
    emit stageMessage(tr("停止信号发生器..."));
    emitLog(LogLevel::Info, QStringLiteral("停止信号发生器"));
    invokeIfSet(m_stopGenerator);

    setState(State::StoppingCyclic);
    emit stageMessage(tr("停止循环写入..."));
    emitLog(LogLevel::Info, QStringLiteral("停止循环写入"));
    invokeIfSet(m_stopCyclic);

    // PMIC 不在前置序列中关闭：烧录期间 IC 必须保持正常供电

    // 前置期间用户已点取消则提前结束
    if (m_cancelFlag->load()) {
        setState(State::Cancelled);
        emit stageMessage(tr("已取消"));
        emitLog(LogLevel::Info, QStringLiteral("已取消"));
        emit finished(false, QStringLiteral("已取消"));
        setState(State::Idle);
        return;
    }

    setState(State::Flashing);
    emit stageMessage(tr("烧录中..."));
    emitLog(LogLevel::Info,
            QStringLiteral("调用 %1::flash() (%2 KB)")
                .arg(strategy->icModel())
                .arg(QString::number(firmware.size() / 1024.0, 'f', 1)));

    if (m_serialManager == nullptr) {
        setState(State::Failed);
        emitLog(LogLevel::Error, QStringLiteral("SerialManager 未注入，无法启动烧录"));
        emit finished(false, QStringLiteral("内部错误：SerialManager 未注入"));
        setState(State::Idle);
        return;
    }

    // 暂停心跳：协议 v2.7 §4.3.5 明确规定 EXEC 阻塞 5-10s 期间 STM32 主循环停摆，
    // 不响应任何帧（包括 0x00 心跳）。fast-path 同步执行期间 SerialManager 工作线程
    // event loop 也被卡住，心跳定时器无法触发。在投递烧录任务之前用 QueuedConnection
    // 同序入队 stopHeartbeat，保证心跳定时器先停 → fast-path 再执行 → 完成后由主线程
    // finished 回调里 startHeartbeat 恢复。
    QMetaObject::invokeMethod(m_serialManager, [sm = m_serialManager]() {
        sm->stopHeartbeat();
    }, Qt::QueuedConnection);

    // 烧录任务投递到 SerialManager 工作线程同步执行（fast-path：strategy 通过
    // m_serialManager->sendAndWaitResponse 同步发协议 0x32~0x37 命令，全程零跨线程）。
    // QueuedConnection fire-and-forget；结果再通过 invokeMethod 回主线程触发 finished。
    auto cancelFlag = m_cancelFlag;
    const qint64 total = m_totalBytes;
    QPointer<FwFlashService> self(this);

    m_flashInFlight = true;

    QMetaObject::invokeMethod(m_serialManager, [self, strategy, firmware, cancelFlag, total]() {
        if (self.isNull()) return;
        QString errorMsg;
        const bool fullProgress = strategy->reportsFullProgress();
        auto progress = [self, total, fullProgress](qint64 sent) {
            if (self.isNull()) return;
            // strategy 上报的是已发字节，不超过总长；qMin clamp 防御兜底
            const qint64 displaySent = qMin(sent, total);
            // fullProgress=true：strategy 自报全程 0~100%（DW9788/DW9786 vendor 库一气呵成）
            //   直接 emit progressUpdated(sent, total) 保留字节级精度，UI 顺滑
            // fullProgress=false：strategy 只报 DATA 阶段，占总进度 kPctData=20%（AW 路径，
            //   后续 ERASE/WRITE/TAIL 由 STM32 端 0x38 进度帧通过 onFlashExecProgress 推进）
            if (fullProgress) {
                QMetaObject::invokeMethod(self.data(), [self, displaySent, total]() {
                    if (self.isNull()) return;
                    emit self->progressUpdated(displaySent, total);
                }, Qt::QueuedConnection);
            } else {
                const int pct = (total > 0)
                    ? static_cast<int>(displaySent * FwFlashProgress::kPctData / total)
                    : 0;
                QMetaObject::invokeMethod(self.data(), [self, pct]() {
                    if (self.isNull()) return;
                    self->emitProgressPct(pct);
                }, Qt::QueuedConnection);
            }
            emit self->stageMessage(
                QStringLiteral("传输中 %1 KB / %2 KB")
                    .arg(QString::number(displaySent / 1024.0, 'f', 1))
                    .arg(QString::number(total / 1024.0, 'f', 1)));
        };
        const bool ok = strategy->flash(firmware, progress, *cancelFlag, &errorMsg);
        const bool wasCancelled = cancelFlag->load();

        if (self.isNull()) return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, ok, wasCancelled, errorMsg, total]() {
                if (self.isNull()) return;
                self->m_flashInFlight = false;
                if (ok) {
                    self->emitProgressPct(100);
                    self->setState(State::Completed);
                    emit self->stageMessage(FwFlashService::tr("烧录完成"));
                    self->emitLog(LogLevel::Ok, QStringLiteral("烧录完成"));
                    emit self->finished(true, QStringLiteral("成功"));
                } else if (wasCancelled) {
                    self->setState(State::Cancelled);
                    emit self->stageMessage(FwFlashService::tr("已取消"));
                    self->emitLog(LogLevel::Info, QStringLiteral("已取消"));
                    emit self->finished(false, QStringLiteral("已取消"));
                } else {
                    self->setState(State::Failed);
                    emit self->stageMessage(FwFlashService::tr("失败：%1").arg(errorMsg));
                    self->emitLog(LogLevel::Error, QStringLiteral("烧录失败：%1").arg(errorMsg));
                    emit self->finished(false, errorMsg);
                }
                self->setState(State::Idle);

                // 恢复心跳（与启动前的 stopHeartbeat 配对，无论成功/失败/取消都恢复）
                if (self->m_serialManager != nullptr) {
                    QMetaObject::invokeMethod(self->m_serialManager,
                                              [sm = self->m_serialManager]() {
                        sm->startHeartbeat();
                    }, Qt::QueuedConnection);
                }
            },
            Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
