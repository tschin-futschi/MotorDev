// =============================================================================
// @file    blockreadservice.cpp
// @brief   寄存器块读取服务实现
// =============================================================================
#include "services/blockreadservice.h"

#include "services/registerservice.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QTime>

namespace {
Q_LOGGING_CATEGORY(lcBlockSvc, "motordev.registerrw.blocksvc")

/// @brief 4 位大写 hex（无 0x 前缀）格式化辅助
QString formatHex4(quint16 v) {
    return QString::number(v, 16).toUpper().rightJustified(4, QLatin1Char('0'));
}
}  // namespace

BlockReadService::BlockReadService(CommandDispatcher *dispatcher, QObject *parent)
    : QObject(parent)
    , m_innerService(new RegisterService(dispatcher, this)) {
    // 内部 RegisterService 的 globalRow 字段被复用为 block read index (0..count-1)，
    // 与 RegisterTable / BatchRegisterService 完全隔离（独立实例 → 不会串扰）
    connect(m_innerService, &RegisterService::rowReadOk,
            this, &BlockReadService::onInnerReadOk);
    connect(m_innerService, &RegisterService::rowReadError,
            this, &BlockReadService::onInnerReadError);
}

BlockReadService::~BlockReadService() = default;

bool BlockReadService::isBusy() const {
    return m_state == State::Reading || m_state == State::WritingFile;
}

void BlockReadService::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

// -----------------------------------------------------------------------------
// 公开槽
// -----------------------------------------------------------------------------

void BlockReadService::start(quint16 startAddr, int count, const QString &targetDir) {
    if (isBusy()) {
        emit logMessage(LogLevel::Warn,
                        QStringLiteral("块读取任务已在执行，忽略本次请求"));
        return;
    }

    if (count < 1) {
        const QString msg = QStringLiteral("块读取：寄存器个数必须 ≥ 1");
        emit stageMessage(msg);
        emit logMessage(LogLevel::Warn, msg);
        emit finished(false, msg, QString());
        return;
    }

    // 地址自卫：最后一个读地址 = startAddr + 2*(count-1)，不得越过 16-bit 上限。
    // 不纯依赖 UI 校验（service 入口独立防御，避免 submitNextRead 处 quint16 静默回绕）。
    const long long lastAddr = static_cast<long long>(startAddr) + 2LL * (count - 1);
    if (lastAddr > 0xFFFF) {
        const QString msg = QStringLiteral("块读取：地址范围越界（0x%1 + %2×2 超出 0xFFFF）")
                                .arg(formatHex4(startAddr)).arg(count);
        emit stageMessage(msg);
        emit logMessage(LogLevel::Warn, msg);
        emit finished(false, msg, QString());
        return;
    }

    const QFileInfo dirInfo(targetDir);
    if (!dirInfo.isDir()) {
        const QString msg = QStringLiteral("块读取：保存目录不存在");
        emit stageMessage(msg);
        emit logMessage(LogLevel::Warn, msg);
        emit finished(false, msg, QString());
        return;
    }

    m_startAddr = startAddr;
    m_count = count;
    m_targetDir = targetDir;
    m_results.clear();
    m_results.reserve(count);
    m_currentIndex = 0;
    m_cancelFlag = false;

    setState(State::Reading);
    emit progress(0, m_count);
    const QString startMsg = QStringLiteral("块读取启动：起始 0x%1，共 %2 个寄存器")
                                 .arg(formatHex4(m_startAddr)).arg(m_count);
    emit stageMessage(startMsg);
    emit logMessage(LogLevel::Info, startMsg);
    qCInfo(lcBlockSvc).noquote() << startMsg << "targetDir=" << m_targetDir;

    submitNextRead();
}

void BlockReadService::cancel() {
    if (m_state != State::Reading) return;
    m_cancelFlag = true;
    emit stageMessage(QStringLiteral("正在取消..."));
    emit logMessage(LogLevel::Info, QStringLiteral("块读取：收到取消请求"));
}

// -----------------------------------------------------------------------------
// 内部读取流程
// -----------------------------------------------------------------------------

void BlockReadService::submitNextRead() {
    if (m_currentIndex < 0 || m_currentIndex >= m_count) return;
    const quint16 addr = static_cast<quint16>(m_startAddr + 2 * m_currentIndex);
    m_innerService->readSingleRow(m_currentIndex, addr);
}

void BlockReadService::onInnerReadOk(int batchIndex, qint16 value) {
    if (m_state != State::Reading) return;
    if (batchIndex != m_currentIndex) return;

    // 协作式取消检查点：先 commit 已收到的响应（不算"成功跳过"），再收口
    const quint16 addr = static_cast<quint16>(m_startAddr + 2 * m_currentIndex);
    m_results.append(qMakePair(addr, value));

    const int done = m_currentIndex + 1;
    emit progress(done, m_count);

    if (m_cancelFlag) {
        const QString msg = QStringLiteral("块读取已取消：已成功读取 %1/%2 条")
                                .arg(done).arg(m_count);
        finishAndWriteCsv(false, msg);
        return;
    }

    const QString progressMsg = QStringLiteral("块读取：%1/%2 0x%3 = 0x%4 OK")
                                    .arg(done).arg(m_count)
                                    .arg(formatHex4(addr))
                                    .arg(formatHex4(static_cast<quint16>(value)));
    emit stageMessage(progressMsg);

    m_currentIndex++;
    if (m_currentIndex < m_count) {
        submitNextRead();
        return;
    }

    // 全部成功
    const QString finalMsg = QStringLiteral("块读取完成：%1/%1 全部成功").arg(m_count);
    finishAndWriteCsv(true, finalMsg);
}

void BlockReadService::onInnerReadError(int batchIndex) {
    if (m_state != State::Reading) return;
    if (batchIndex != m_currentIndex) return;

    const quint16 addr = static_cast<quint16>(m_startAddr + 2 * m_currentIndex);
    const int total = m_count;
    const QString failMsg = QStringLiteral("块读取失败于 0x%1（第 %2/%3 条）；已读到的 %4 条仍写入文件")
                                .arg(formatHex4(addr))
                                .arg(m_currentIndex + 1).arg(total)
                                .arg(m_results.size());
    finishAndWriteCsv(false, failMsg);
}

// -----------------------------------------------------------------------------
// CSV 写文件 + 收尾
// -----------------------------------------------------------------------------

QString BlockReadService::resolveOutputPath() const {
    const QString hhmmss = QTime::currentTime().toString(QStringLiteral("HHmmss"));
    const QString basePath = QDir(m_targetDir).filePath(
        QStringLiteral("Bulkread_%1.csv").arg(hhmmss));

    if (!QFileInfo::exists(basePath)) {
        return basePath;
    }

    // 同秒冲突：追加 _1 / _2 / ... 直到不冲突
    for (int n = 1; n <= 999; ++n) {
        const QString candidate = QDir(m_targetDir).filePath(
            QStringLiteral("Bulkread_%1_%2.csv").arg(hhmmss).arg(n));
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QString();  // 999 次冲突仍无可用名
}

void BlockReadService::finishAndWriteCsv(bool success, const QString &finalText) {
    setState(State::WritingFile);

    QString savedPath;
    bool fileWriteOk = false;

    if (!m_results.isEmpty()) {
        const QString outputPath = resolveOutputPath();
        if (outputPath.isEmpty()) {
            emit logMessage(LogLevel::Error,
                            QStringLiteral("块读取：输出文件名冲突 999 次无法解决，放弃写入"));
        } else {
            QSaveFile file(outputPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QByteArray buf;
                buf.reserve(m_results.size() * 12 + 16);
                buf.append("addr,value\n");
                for (const QPair<quint16, qint16> &entry : m_results) {
                    buf.append(formatHex4(entry.first).toLatin1());
                    buf.append(',');
                    buf.append(formatHex4(static_cast<quint16>(entry.second)).toLatin1());
                    buf.append('\n');
                }
                if (file.write(buf) == buf.size() && file.commit()) {
                    fileWriteOk = true;
                    savedPath = outputPath;
                } else {
                    emit logMessage(LogLevel::Error,
                                    QStringLiteral("块读取：写入 CSV 失败：%1").arg(file.errorString()));
                }
            } else {
                emit logMessage(LogLevel::Error,
                                QStringLiteral("块读取：无法创建 CSV 文件：%1").arg(file.errorString()));
            }
        }
    }

    // 最终状态文字 = 调用方传入 finalText + 写文件结果
    QString summary = finalText;
    if (!m_results.isEmpty() && fileWriteOk) {
        summary = QStringLiteral("%1；已写入 %2").arg(finalText, QFileInfo(savedPath).fileName());
    } else if (m_results.isEmpty()) {
        summary = QStringLiteral("%1（无数据可写入）").arg(finalText);
    }

    emit stageMessage(summary);
    if (success) {
        emit logMessage(LogLevel::Ok, summary);
        qCInfo(lcBlockSvc).noquote() << summary;
    } else {
        emit logMessage(LogLevel::Error, summary);
        qCWarning(lcBlockSvc).noquote() << summary;
    }

    // 最终状态：完成 / 失败 / 取消
    State finalState;
    if (success) {
        finalState = State::Completed;
    } else if (m_cancelFlag) {
        finalState = State::Cancelled;
    } else {
        finalState = State::Failed;
    }
    setState(finalState);
    emit finished(success, summary, savedPath);

    // 清理上下文，回到 Idle
    m_startAddr = 0;
    m_count = 0;
    m_targetDir.clear();
    m_results.clear();
    m_currentIndex = 0;
    m_cancelFlag = false;
    setState(State::Idle);
}
