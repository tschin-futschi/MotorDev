// =============================================================================
// @file    flashstoreservice.cpp
// @brief   STM32 自身 FLASH 文件存储服务实现
//
// 三种 fast-path 流程在同一个工作线程内串行执行：
//   - Write: 0x39 BEGIN → 0x3A DATA × N → 0x3B END，全程心跳暂停
//   - Read:  0x3C READ_BEGIN → 0x3D READ_DATA × N → 落盘 + CRC 校验
//   - Info:  单帧 0x3E INFO
//
// 任何路径成功 / 失败 / 取消都会回主线程切到 Idle 并恢复心跳。
// =============================================================================
#include "services/flashstoreservice.h"

#include "protocol/firmware_parser.h"
#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>

#include <algorithm>

namespace {
Q_LOGGING_CATEGORY(lcFlashStore, "motordev.flashstore")

// -------- 协议常量（与 protocol.md §4.3.6 + STM32 端 app_flashstore.h 对齐）--------
constexpr int      kBeginTimeoutMs    = 15000;  ///< 0x39 含整区擦 3-7s，留 2x 余量
constexpr int      kDataTimeoutMs     = 5000;   ///< 0x3A / 0x3D 单包响应
constexpr int      kShortTimeoutMs    = 2000;   ///< 0x3B / 0x3C / 0x3E
constexpr int      kChunkBytes        = 252;    ///< 与 0x33 / 协议 §4.3.6 一致
constexpr quint32  kMaxFileBytes      = 917488U; ///< = 896 KB - 16 B（与 STM32 FS_MAX_FILE_BYTES 一致）

// -------- 进度节流 --------
constexpr int    kProgressMinIntervalMs = 16;
constexpr qint64 kProgressMinBytes      = 1024;

}  // namespace

FlashStoreService::FlashStoreService(SerialManager *serialManager, QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager) {}

FlashStoreService::~FlashStoreService() {
    // 翻 cancelFlag 让 fast-path 在下一个安全点退出；不强行 quit SerialManager
    if (m_cancelFlag) m_cancelFlag->store(true);
}

bool FlashStoreService::isBusy() const {
    switch (m_state) {
    case State::WritePreparing:
    case State::WriteBeginning:
    case State::WriteSending:
    case State::WriteEnding:
    case State::ReadBeginning:
    case State::ReadFetching:
    case State::QueryingInfo:
        return true;
    default:
        return false;
    }
}

void FlashStoreService::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

void FlashStoreService::emitLog(LogLevel level, const QString &message) {
    emit logMessage(level, message);
}

bool FlashStoreService::armBusy() {
    if (isBusy()) {
        emitLog(LogLevel::Warn, QStringLiteral("已有 FLASH 存储任务在执行，忽略本次请求"));
        return false;
    }
    if (m_serialManager == nullptr) {
        emitLog(LogLevel::Error, QStringLiteral("SerialManager 未注入"));
        emit finished(false, QStringLiteral("内部错误：SerialManager 未注入"));
        return false;
    }
    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);

    // 心跳暂停：fast-path 同步执行期间 SerialManager 工作线程 event loop 被卡死，
    // 心跳定时器无法触发。QueuedConnection 同序入队，保证 stopHeartbeat 先于
    // fast-path 任务执行。
    QMetaObject::invokeMethod(m_serialManager, [sm = m_serialManager]() {
        sm->stopHeartbeat();
    }, Qt::QueuedConnection);

    m_flashInFlight = true;
    return true;
}

void FlashStoreService::releaseBusy(bool success, bool wasCancelled, const QString &summary) {
    m_flashInFlight = false;

    if (success) {
        setState(State::Completed);
        emitLog(LogLevel::Ok, summary.isEmpty() ? QStringLiteral("完成") : summary);
        emit finished(true, summary.isEmpty() ? QStringLiteral("成功") : summary);
    } else if (wasCancelled) {
        setState(State::Cancelled);
        emitLog(LogLevel::Info, QStringLiteral("已取消"));
        emit finished(false, QStringLiteral("已取消"));
    } else {
        setState(State::Failed);
        emitLog(LogLevel::Error, summary);
        emit finished(false, summary);
    }
    setState(State::Idle);

    // 恢复心跳（无论成功/失败/取消都恢复）
    if (m_serialManager != nullptr) {
        QMetaObject::invokeMethod(m_serialManager, [sm = m_serialManager]() {
            sm->startHeartbeat();
        }, Qt::QueuedConnection);
    }
}

void FlashStoreService::cancel() {
    if (!isBusy()) return;
    if (m_cancelFlag) m_cancelFlag->store(true);
    emitLog(LogLevel::Warn, QStringLiteral("正在取消..."));
}

// -----------------------------------------------------------------------------
// startWrite
// -----------------------------------------------------------------------------

void FlashStoreService::startWrite(const QString &srcFilePath) {
    if (!armBusy()) return;
    setState(State::WritePreparing);

    QFile f(srcFilePath);
    if (!f.open(QIODevice::ReadOnly)) {
        emitLog(LogLevel::Error,
                QStringLiteral("无法打开文件：%1（%2）").arg(srcFilePath, f.errorString()));
        releaseBusy(false, false, QStringLiteral("文件打开失败"));
        return;
    }
    const QByteArray data = f.readAll();
    f.close();

    if (data.isEmpty()) {
        emitLog(LogLevel::Error, QStringLiteral("文件内容为空"));
        releaseBusy(false, false, QStringLiteral("文件为空"));
        return;
    }
    if (static_cast<quint32>(data.size()) > kMaxFileBytes) {
        emitLog(LogLevel::Error,
                QStringLiteral("文件 %1 字节超出 STM32 Flash 存储区上限 %2 字节（%3 KB）")
                    .arg(data.size()).arg(kMaxFileBytes)
                    .arg(QString::number(kMaxFileBytes / 1024.0, 'f', 2)));
        releaseBusy(false, false, QStringLiteral("文件过大"));
        return;
    }

    const quint32 crc32 = FirmwareParser::computeCrc32(data);
    emitLog(LogLevel::Info,
            QStringLiteral("准备上传：%1（%2 字节，CRC32 0x%3）")
                .arg(QFileInfo(srcFilePath).fileName())
                .arg(data.size())
                .arg(crc32, 8, 16, QLatin1Char('0')));

    dispatchOp(Op::Write, data, crc32);
}

// -----------------------------------------------------------------------------
// startRead
// -----------------------------------------------------------------------------

void FlashStoreService::startRead(const QString &dstDirPath) {
    if (!armBusy()) return;

    QDir dir(dstDirPath);
    if (!dir.exists()) {
        emitLog(LogLevel::Error, QStringLiteral("目标目录不存在：%1").arg(dstDirPath));
        releaseBusy(false, false, QStringLiteral("目录不存在"));
        return;
    }

    setState(State::ReadBeginning);
    emitLog(LogLevel::Info, QStringLiteral("准备下载到：%1").arg(dstDirPath));
    dispatchOp(Op::Read, {}, 0, dir.absolutePath());
}

// -----------------------------------------------------------------------------
// refreshInfo
// -----------------------------------------------------------------------------

void FlashStoreService::refreshInfo() {
    if (!armBusy()) return;
    setState(State::QueryingInfo);
    dispatchOp(Op::Info);
}

// -----------------------------------------------------------------------------
// startWipe
// -----------------------------------------------------------------------------

void FlashStoreService::startWipe() {
    if (!armBusy()) return;
    // 复用 WriteBeginning 状态（语义近似：都是整区擦），不引入新状态
    setState(State::WriteBeginning);
    emit stageMessage(tr("擦除 Flash 区域（3-7 秒，不可取消）..."));
    emitLog(LogLevel::Info, QStringLiteral("开始清空 Flash 文件存储区"));
    dispatchOp(Op::Wipe);
}

// -----------------------------------------------------------------------------
// 文件名生成（同秒冲突时追加 _N）
// -----------------------------------------------------------------------------

namespace {
QString generateOutputPath(const QString &dirPath) {
    const QString suffix = QDateTime::currentDateTime().toString(QStringLiteral("HHmmss"));
    QString base = QStringLiteral("FLASH_%1").arg(suffix);
    QString candidate = QStringLiteral("%1/%2.txt").arg(dirPath, base);
    int n = 1;
    while (QFile::exists(candidate)) {
        candidate = QStringLiteral("%1/%2_%3.txt").arg(dirPath, base).arg(n);
        ++n;
    }
    return candidate;
}
}  // namespace

// -----------------------------------------------------------------------------
// fast-path 派发到 SerialManager 工作线程
// -----------------------------------------------------------------------------

void FlashStoreService::dispatchOp(Op op,
                                   const QByteArray &writeData,
                                   quint32 writeCrc,
                                   const QString &readDir) {
    QPointer<FlashStoreService> self(this);
    auto cancelFlag = m_cancelFlag;

    QMetaObject::invokeMethod(m_serialManager,
        [self, op, writeData, writeCrc, readDir, cancelFlag]() {
            if (self.isNull()) return;

            // -------- 通用 send helper（同 AwLocalIspStrategy::sendCmd 风格）--------
            auto sendCmd = [self](uint8_t cmd, const QByteArray &payload,
                                  QByteArray &outData, int timeoutMs,
                                  const char *stage, QString *err) -> bool {
                if (self.isNull() || self->m_serialManager == nullptr) {
                    if (err) *err = QStringLiteral("[%1] 内部状态丢失").arg(QLatin1String(stage));
                    return false;
                }
                uint8_t outCmd = 0;
                uint8_t outSeq = 0;
                const bool ok = self->m_serialManager->sendAndWaitResponse(
                    cmd, payload, outCmd, outSeq, outData, timeoutMs);
                if (!ok) {
                    if (err) *err = QStringLiteral("[%1] 超时或链路失败（timeout=%2 ms）")
                                        .arg(QLatin1String(stage)).arg(timeoutMs);
                    return false;
                }
                if (outCmd == MotorProtocol::CmdErrorResponse) {
                    const uint8_t code = MotorProtocol::decodeErrorCode(outData);
                    if (err) *err = QStringLiteral("[%1] STM32 错误响应：0x%2（可能采样/发生器/循环写入仍在运行）")
                                        .arg(QLatin1String(stage))
                                        .arg(code, 2, 16, QLatin1Char('0'));
                    return false;
                }
                if (outCmd != cmd) {
                    if (err) *err = QStringLiteral("[%1] 响应命令码不匹配：预期 0x%2，实际 0x%3")
                                        .arg(QLatin1String(stage))
                                        .arg(cmd, 2, 16, QLatin1Char('0'))
                                        .arg(outCmd, 2, 16, QLatin1Char('0'));
                    return false;
                }
                return true;
            };

            // -------- 状态切换 helper（必须 marshal 回主线程）--------
            auto setStateOnMain = [self](State s) {
                if (self.isNull()) return;
                QMetaObject::invokeMethod(self.data(), [self, s]() {
                    if (!self.isNull()) self->setState(s);
                }, Qt::QueuedConnection);
            };
            auto logOnMain = [self](LogLevel lv, const QString &msg) {
                if (self.isNull()) return;
                QMetaObject::invokeMethod(self.data(), [self, lv, msg]() {
                    if (!self.isNull()) self->emitLog(lv, msg);
                }, Qt::QueuedConnection);
            };
            auto stageOnMain = [self](const QString &msg) {
                if (self.isNull()) return;
                QMetaObject::invokeMethod(self.data(), [self, msg]() {
                    if (!self.isNull()) emit self->stageMessage(msg);
                }, Qt::QueuedConnection);
            };

            QString errorMsg;
            bool ok = false;
            QString savedReadPath;
            quint32 infoTotal = 0;
            quint32 infoUsed  = 0;

            switch (op) {

            // ============================ WRITE ============================
            case Op::Write: {
                setStateOnMain(State::WriteBeginning);
                stageOnMain(FlashStoreService::tr("擦除 Flash 区域（3-7 秒）..."));

                QByteArray resp;
                if (!sendCmd(MotorProtocol::CmdFlashStoreWriteBegin,
                             MotorProtocol::encodeFlashStoreWriteBegin(static_cast<quint32>(writeData.size())),
                             resp, kBeginTimeoutMs, "WRITE_BEGIN", &errorMsg)) {
                    break;
                }
                uint8_t st = 0;
                if (!MotorProtocol::decodeFlashStoreSimpleStatus(resp, &st) ||
                    st != static_cast<uint8_t>(MotorProtocol::FlashStoreStatus::Ok)) {
                    errorMsg = QStringLiteral("WRITE_BEGIN 失败：%1（status=0x%2）")
                                   .arg(QString::fromLatin1(MotorProtocol::flashStoreStatusName(st)))
                                   .arg(st, 2, 16, QLatin1Char('0'));
                    break;
                }

                // -------- 0x3A DATA 循环 --------
                setStateOnMain(State::WriteSending);
                const qint64 total = writeData.size();
                qint64 offset = 0;
                quint16 pktSeq = 0;
                qint64 lastProgBytes = 0;
                qint64 lastProgMs = QDateTime::currentMSecsSinceEpoch();

                while (offset < total) {
                    if (cancelFlag->load()) { errorMsg = QStringLiteral("用户取消"); break; }
                    const int n = static_cast<int>(std::min<qint64>(kChunkBytes, total - offset));
                    const QByteArray chunk = writeData.mid(static_cast<int>(offset), n);

                    if (!sendCmd(MotorProtocol::CmdFlashStoreWriteData,
                                 MotorProtocol::encodeFlashStoreWriteData(pktSeq, chunk),
                                 resp, kDataTimeoutMs, "WRITE_DATA", &errorMsg)) {
                        errorMsg = QStringLiteral("WRITE_DATA pktSeq=%1 失败：%2")
                                       .arg(pktSeq).arg(errorMsg);
                        break;
                    }
                    quint16 nextSeq = 0;
                    if (!MotorProtocol::decodeFlashStoreWriteDataResponse(resp, &nextSeq) ||
                        nextSeq != static_cast<quint16>(pktSeq + 1)) {
                        errorMsg = QStringLiteral("WRITE_DATA nextSeq 不匹配：预期 %1，实际 %2")
                                       .arg(pktSeq + 1).arg(nextSeq);
                        break;
                    }

                    offset += n;
                    pktSeq = static_cast<quint16>(pktSeq + 1);

                    // 进度上报（节流 + 跨线程 emit）
                    const qint64 now = QDateTime::currentMSecsSinceEpoch();
                    if ((now - lastProgMs) >= kProgressMinIntervalMs ||
                        (offset - lastProgBytes) >= kProgressMinBytes ||
                        offset == total) {
                        const qint64 sent = offset;
                        if (!self.isNull()) {
                            QMetaObject::invokeMethod(self.data(), [self, sent, total]() {
                                if (!self.isNull()) emit self->progressUpdated(sent, total);
                            }, Qt::QueuedConnection);
                        }
                        stageOnMain(FlashStoreService::tr("传输中 %1 KB / %2 KB")
                                        .arg(QString::number(sent / 1024.0, 'f', 1))
                                        .arg(QString::number(total / 1024.0, 'f', 1)));
                        lastProgBytes = offset;
                        lastProgMs = now;
                    }
                }
                if (!errorMsg.isEmpty()) break;

                // -------- 0x3B END --------
                setStateOnMain(State::WriteEnding);
                stageOnMain(FlashStoreService::tr("校验 CRC + 提交元数据..."));
                if (!sendCmd(MotorProtocol::CmdFlashStoreWriteEnd,
                             MotorProtocol::encodeFlashStoreWriteEnd(writeCrc),
                             resp, kShortTimeoutMs, "WRITE_END", &errorMsg)) {
                    break;
                }
                st = 0;
                if (!MotorProtocol::decodeFlashStoreSimpleStatus(resp, &st) ||
                    st != static_cast<uint8_t>(MotorProtocol::FlashStoreStatus::Ok)) {
                    errorMsg = QStringLiteral("WRITE_END 失败：%1（status=0x%2）")
                                   .arg(QString::fromLatin1(MotorProtocol::flashStoreStatusName(st)))
                                   .arg(st, 2, 16, QLatin1Char('0'));
                    break;
                }

                infoUsed = static_cast<quint32>(writeData.size());
                infoTotal = kMaxFileBytes;
                ok = true;
                logOnMain(LogLevel::Ok, QStringLiteral("上传完成：%1 字节").arg(writeData.size()));
                break;
            }

            // ============================ READ =============================
            case Op::Read: {
                QByteArray resp;
                if (!sendCmd(MotorProtocol::CmdFlashStoreReadBegin, {},
                             resp, kShortTimeoutMs, "READ_BEGIN", &errorMsg)) {
                    break;
                }
                uint8_t st = 0;
                quint32 size = 0;
                quint32 crc32 = 0;
                if (!MotorProtocol::decodeFlashStoreReadBeginResponse(resp, &st, &size, &crc32)) {
                    errorMsg = QStringLiteral("READ_BEGIN 响应载荷长度不足（%1 B）").arg(resp.size());
                    break;
                }
                if (st != static_cast<uint8_t>(MotorProtocol::FlashStoreStatus::Ok)) {
                    errorMsg = QStringLiteral("READ_BEGIN：%1（slot 为空或损坏）")
                                   .arg(QString::fromLatin1(MotorProtocol::flashStoreStatusName(st)));
                    break;
                }
                if (size == 0U || size > kMaxFileBytes) {
                    errorMsg = QStringLiteral("READ_BEGIN 返回 size 异常：%1").arg(size);
                    break;
                }
                logOnMain(LogLevel::Info,
                          QStringLiteral("STM32 端文件：%1 字节，CRC32 0x%2")
                              .arg(size).arg(crc32, 8, 16, QLatin1Char('0')));

                // 准备落盘文件
                const QString savePath = generateOutputPath(readDir);
                QFile out(savePath);
                if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    errorMsg = QStringLiteral("无法创建下载文件：%1（%2）")
                                   .arg(savePath, out.errorString());
                    break;
                }

                // -------- 0x3D 循环 --------
                setStateOnMain(State::ReadFetching);
                const quint32 total = size;
                quint32 received = 0;
                quint16 pktSeq = 0;
                qint64 lastProgBytes = 0;
                qint64 lastProgMs = QDateTime::currentMSecsSinceEpoch();

                while (received < total) {
                    if (cancelFlag->load()) { errorMsg = QStringLiteral("用户取消"); break; }
                    if (!sendCmd(MotorProtocol::CmdFlashStoreReadData,
                                 MotorProtocol::encodeFlashStoreReadData(pktSeq),
                                 resp, kDataTimeoutMs, "READ_DATA", &errorMsg)) {
                        errorMsg = QStringLiteral("READ_DATA pktSeq=%1 失败：%2")
                                       .arg(pktSeq).arg(errorMsg);
                        break;
                    }
                    const qint64 wrote = out.write(resp);
                    if (wrote != resp.size()) {
                        errorMsg = QStringLiteral("写入下载文件失败（实际 %1 / 期望 %2）")
                                       .arg(wrote).arg(resp.size());
                        break;
                    }
                    received += static_cast<quint32>(resp.size());
                    pktSeq = static_cast<quint16>(pktSeq + 1);

                    const qint64 now = QDateTime::currentMSecsSinceEpoch();
                    if ((now - lastProgMs) >= kProgressMinIntervalMs ||
                        (static_cast<qint64>(received) - lastProgBytes) >= kProgressMinBytes ||
                        received == total) {
                        const qint64 cur = static_cast<qint64>(received);
                        const qint64 t   = static_cast<qint64>(total);
                        if (!self.isNull()) {
                            QMetaObject::invokeMethod(self.data(), [self, cur, t]() {
                                if (!self.isNull()) emit self->progressUpdated(cur, t);
                            }, Qt::QueuedConnection);
                        }
                        stageOnMain(FlashStoreService::tr("下载中 %1 KB / %2 KB")
                                        .arg(QString::number(cur / 1024.0, 'f', 1))
                                        .arg(QString::number(t / 1024.0, 'f', 1)));
                        lastProgBytes = cur;
                        lastProgMs = now;
                    }
                }
                out.close();

                if (!errorMsg.isEmpty()) {
                    // 失败时删掉半成品文件
                    QFile::remove(savePath);
                    break;
                }

                // 校验本地 CRC（与 STM32 端 metadata 对比，加固防错）
                QFile rb(savePath);
                if (rb.open(QIODevice::ReadOnly)) {
                    const quint32 localCrc = FirmwareParser::computeCrc32(rb.readAll());
                    rb.close();
                    if (localCrc != crc32) {
                        errorMsg = QStringLiteral("下载完成但本地 CRC32 0x%1 与 STM32 端 0x%2 不一致")
                                       .arg(localCrc, 8, 16, QLatin1Char('0'))
                                       .arg(crc32, 8, 16, QLatin1Char('0'));
                        QFile::remove(savePath);
                        break;
                    }
                }

                savedReadPath = savePath;
                infoUsed = total;
                infoTotal = kMaxFileBytes;
                ok = true;
                logOnMain(LogLevel::Ok,
                          QStringLiteral("下载完成（%1 字节，已校验 CRC）→ %2").arg(total).arg(savePath));
                break;
            }

            // ============================ INFO =============================
            case Op::Info: {
                QByteArray resp;
                if (!sendCmd(MotorProtocol::CmdFlashStoreInfo, {},
                             resp, kShortTimeoutMs, "INFO", &errorMsg)) {
                    break;
                }
                if (!MotorProtocol::decodeFlashStoreInfoResponse(resp, &infoTotal, &infoUsed)) {
                    errorMsg = QStringLiteral("INFO 响应载荷长度不足（%1 B）").arg(resp.size());
                    break;
                }
                ok = true;
                break;
            }

            // ============================ WIPE =============================
            case Op::Wipe: {
                QByteArray resp;
                // 复用 WRITE_BEGIN 的 15s 超时（同样擦 7 个 Sector，~3-7s 阻塞）
                if (!sendCmd(MotorProtocol::CmdFlashStoreWipe, {},
                             resp, kBeginTimeoutMs, "WIPE", &errorMsg)) {
                    break;
                }
                uint8_t st = 0;
                if (!MotorProtocol::decodeFlashStoreSimpleStatus(resp, &st) ||
                    st != static_cast<uint8_t>(MotorProtocol::FlashStoreStatus::Ok)) {
                    errorMsg = QStringLiteral("WIPE 失败：%1（status=0x%2）")
                                   .arg(QString::fromLatin1(MotorProtocol::flashStoreStatusName(st)))
                                   .arg(st, 2, 16, QLatin1Char('0'));
                    break;
                }
                // 擦除成功 → slot 即时归零，触发容量 UI 立即刷新
                infoTotal = kMaxFileBytes;
                infoUsed  = 0U;
                ok = true;
                logOnMain(LogLevel::Ok, QStringLiteral("Flash 文件存储区已清空"));
                break;
            }
            }

            // -------- 回主线程统一收尾 --------
            const bool wasCancelled = cancelFlag->load();
            QString summary;
            if (ok) {
                switch (op) {
                case Op::Write: summary = QStringLiteral("上传成功"); break;
                case Op::Read:  summary = QStringLiteral("下载成功"); break;
                case Op::Info:  summary = QStringLiteral("容量查询完成"); break;
                case Op::Wipe:  summary = QStringLiteral("Flash 区域已清空"); break;
                }
            } else {
                summary = wasCancelled ? QStringLiteral("已取消") : errorMsg;
            }

            QMetaObject::invokeMethod(self.data(),
                [self, ok, wasCancelled, summary, infoTotal, infoUsed, savedReadPath]() {
                    if (self.isNull()) return;
                    // 容量信息无论成功失败,只要拿到了就 emit(Info 路径成功才有,Write/Read
                    // 成功时 infoTotal != 0 也 emit)
                    if (ok && infoTotal != 0U) {
                        emit self->infoUpdated(infoTotal, infoUsed);
                    }
                    if (ok && !savedReadPath.isEmpty()) {
                        emit self->readCompleted(savedReadPath);
                    }
                    self->releaseBusy(ok, wasCancelled, summary);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}
