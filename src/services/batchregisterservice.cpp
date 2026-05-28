// =============================================================================
// @file    batchregisterservice.cpp
// @brief   寄存器批量读写服务实现
// =============================================================================
#include "services/batchregisterservice.h"

#include "services/registerservice.h"

#include <QFileInfo>
#include <QLoggingCategory>

namespace {
Q_LOGGING_CATEGORY(lcBatchSvc, "motordev.registerrw.batchsvc")
}  // namespace

BatchRegisterService::BatchRegisterService(CommandDispatcher *dispatcher, QObject *parent)
    : QObject(parent)
    , m_innerService(new RegisterService(dispatcher, this)) {
    // 内部 RegisterService 的 globalRow 字段被复用为 batch entry index (0..N-1)
    // 由于本 service 与 RegisterTable 的 service 完全独立（RegisterTable 用另一个实例），
    // globalRow 在此处只是一个不透明 ID，不会引起串扰
    connect(m_innerService, &RegisterService::rowReadOk,
            this, &BatchRegisterService::onInnerReadOk);
    connect(m_innerService, &RegisterService::rowReadError,
            this, &BatchRegisterService::onInnerReadError);
    connect(m_innerService, &RegisterService::rowWriteOk,
            this, &BatchRegisterService::onInnerWriteOk);
    connect(m_innerService, &RegisterService::rowWriteError,
            this, &BatchRegisterService::onInnerWriteError);
}

BatchRegisterService::~BatchRegisterService() = default;

bool BatchRegisterService::isBusy() const {
    return m_state == State::Parsing || m_state == State::Writing || m_state == State::Reading;
}

QString BatchRegisterService::opName() const {
    return m_isWrite ? QStringLiteral("批量写入") : QStringLiteral("批量读出");
}

void BatchRegisterService::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

// -----------------------------------------------------------------------------
// 公开槽
// -----------------------------------------------------------------------------

void BatchRegisterService::startWrite(int slotIndex, const QString &filePath) {
    start(slotIndex, filePath, /*isWrite=*/true);
}

void BatchRegisterService::startRead(int slotIndex, const QString &filePath) {
    start(slotIndex, filePath, /*isWrite=*/false);
}

void BatchRegisterService::start(int slotIndex, const QString &filePath, bool isWrite) {
    if (isBusy()) {
        emit logMessage(LogLevel::Warn,
                        QStringLiteral("已有批量任务在执行，忽略本次请求"));
        return;
    }

    m_isWrite = isWrite;
    m_activeSlotIndex = slotIndex;
    const int slotNo = slotIndex + 1;  // 1-based for display

    if (filePath.isEmpty()) {
        const QString msg = QStringLiteral("%1 #%2：请先浏览选择配置文件")
                                .arg(opName()).arg(slotNo);
        emit stageMessage(msg);
        emit logMessage(LogLevel::Warn, msg);
        emit finished(false, msg);
        return;
    }

    // 解析阶段（严格全或无）
    setState(State::Parsing);
    const BatchRegisterFile::ParseResult pr = BatchRegisterFile::parseFile(filePath);
    if (pr.status != BatchRegisterFile::ParseStatus::Ok) {
        const QString msg = QStringLiteral("%1 #%2：%3")
                                .arg(opName()).arg(slotNo).arg(pr.errorMessage);
        emit stageMessage(msg);
        emit logMessage(LogLevel::Warn, msg);
        setState(State::Failed);
        emit finished(false, msg);
        setState(State::Idle);
        return;
    }

    m_activeFilePath = filePath;
    m_activeRawLines = pr.rawLines;
    m_activeEntries = pr.entries;
    m_activeIndex = 0;

    setState(isWrite ? State::Writing : State::Reading);

    const int total = m_activeEntries.size();
    const QString startMsg = QStringLiteral("%1 #%2 启动：%3 条数据行")
                                 .arg(opName()).arg(slotNo).arg(total);
    emit stageMessage(startMsg);
    emit logMessage(LogLevel::Info, startMsg);
    qCInfo(lcBatchSvc).noquote() << startMsg << "file=" << filePath;

    submitNextEntry();
}

void BatchRegisterService::submitNextEntry() {
    if (m_activeIndex < 0 || m_activeIndex >= m_activeEntries.size()) return;
    const BatchRegisterFile::Entry &entry = m_activeEntries.at(m_activeIndex);

    if (m_isWrite) {
        // entry.value (quint16) → qint16 位重解释
        const qint16 sv = static_cast<qint16>(entry.value);
        m_innerService->writeSingleRow(m_activeIndex, entry.addr, sv);
    } else {
        m_innerService->readSingleRow(m_activeIndex, entry.addr);
    }
}

// -----------------------------------------------------------------------------
// 内部 RegisterService 信号回调
// -----------------------------------------------------------------------------

void BatchRegisterService::onInnerReadOk(int batchIndex, qint16 value) {
    if (m_state != State::Reading) return;
    if (batchIndex != m_activeIndex) return;
    if (batchIndex < 0 || batchIndex >= m_activeEntries.size()) return;

    // 更新 entry.value 为新读到的值（用于稍后回写文件）
    BatchRegisterFile::Entry &entry = m_activeEntries[batchIndex];
    entry.value = static_cast<quint16>(value);

    const int slotNo = m_activeSlotIndex + 1;
    const int total = m_activeEntries.size();
    const int done = batchIndex + 1;
    const QString progressMsg = QStringLiteral("批量读出 #%1：%2/%3 0x%4 = 0x%5 OK")
                                     .arg(slotNo)
                                     .arg(done).arg(total)
                                     .arg(entry.addr, 4, 16, QLatin1Char('0'))
                                     .arg(entry.value, 4, 16, QLatin1Char('0'));
    emit stageMessage(progressMsg);
    emit logMessage(LogLevel::Info, progressMsg);

    m_activeIndex++;
    if (m_activeIndex < total) {
        submitNextEntry();
        return;
    }

    // 全部读出成功 → 回写文件
    QString writeErr;
    const bool wroteOk = BatchRegisterFile::writeBackValues(
        m_activeFilePath, m_activeRawLines, m_activeEntries, &writeErr);
    if (wroteOk) {
        const QString finalMsg = QStringLiteral("批量读出 #%1 完成：%2/%2 成功，已写回 %3")
                                       .arg(slotNo)
                                       .arg(total)
                                       .arg(QFileInfo(m_activeFilePath).fileName());
        finish(true, finalMsg);
    } else {
        const QString finalMsg = QStringLiteral("批量读出 #%1：%2/%2 读取成功，但回写文件失败：%3")
                                       .arg(slotNo).arg(total).arg(writeErr);
        finish(false, finalMsg);
    }
}

void BatchRegisterService::onInnerReadError(int batchIndex) {
    if (m_state != State::Reading) return;
    if (batchIndex != m_activeIndex) return;
    if (batchIndex < 0 || batchIndex >= m_activeEntries.size()) return;

    // 决策 #7 全或无：中止后原文件完全不动
    const int slotNo = m_activeSlotIndex + 1;
    const int total = m_activeEntries.size();
    const int errAt = batchIndex + 1;
    const BatchRegisterFile::Entry &entry = m_activeEntries.at(batchIndex);
    const QString finalMsg = QStringLiteral("批量读出 #%1 中止：第 %2/%3 条 0x%4 读取失败（原文件未修改）")
                                   .arg(slotNo)
                                   .arg(errAt).arg(total)
                                   .arg(entry.addr, 4, 16, QLatin1Char('0'));
    finish(false, finalMsg);
}

void BatchRegisterService::onInnerWriteOk(int batchIndex) {
    if (m_state != State::Writing) return;
    if (batchIndex != m_activeIndex) return;
    if (batchIndex < 0 || batchIndex >= m_activeEntries.size()) return;

    const int slotNo = m_activeSlotIndex + 1;
    const int total = m_activeEntries.size();
    const int done = batchIndex + 1;
    const BatchRegisterFile::Entry &entry = m_activeEntries.at(batchIndex);
    const QString progressMsg = QStringLiteral("批量写入 #%1：%2/%3 0x%4 = 0x%5 OK")
                                     .arg(slotNo)
                                     .arg(done).arg(total)
                                     .arg(entry.addr, 4, 16, QLatin1Char('0'))
                                     .arg(entry.value, 4, 16, QLatin1Char('0'));
    emit stageMessage(progressMsg);
    emit logMessage(LogLevel::Info, progressMsg);

    m_activeIndex++;
    if (m_activeIndex < total) {
        submitNextEntry();
        return;
    }

    const QString finalMsg = QStringLiteral("批量写入 #%1 完成：%2/%2 成功")
                                   .arg(slotNo).arg(total);
    finish(true, finalMsg);
}

void BatchRegisterService::onInnerWriteError(int batchIndex) {
    if (m_state != State::Writing) return;
    if (batchIndex != m_activeIndex) return;
    if (batchIndex < 0 || batchIndex >= m_activeEntries.size()) return;

    // 决策 #6 遇错即停（已写入到芯片的部分无法回滚）
    const int slotNo = m_activeSlotIndex + 1;
    const int total = m_activeEntries.size();
    const int errAt = batchIndex + 1;
    const BatchRegisterFile::Entry &entry = m_activeEntries.at(batchIndex);
    const QString finalMsg = QStringLiteral("批量写入 #%1 中止：第 %2/%3 条 0x%4 写入失败")
                                   .arg(slotNo)
                                   .arg(errAt).arg(total)
                                   .arg(entry.addr, 4, 16, QLatin1Char('0'));
    finish(false, finalMsg);
}

// -----------------------------------------------------------------------------
// 收尾
// -----------------------------------------------------------------------------

void BatchRegisterService::finish(bool success, const QString &finalStatusText) {
    emit stageMessage(finalStatusText);
    if (success) {
        emit logMessage(LogLevel::Ok, finalStatusText);
        qCInfo(lcBatchSvc).noquote() << finalStatusText;
    } else {
        emit logMessage(LogLevel::Error, finalStatusText);
        qCWarning(lcBatchSvc).noquote() << finalStatusText;
    }

    setState(success ? State::Completed : State::Failed);
    emit finished(success, finalStatusText);

    // 清理任务上下文，回到 Idle
    m_isWrite = false;
    m_activeSlotIndex = -1;
    m_activeFilePath.clear();
    m_activeRawLines.clear();
    m_activeEntries.clear();
    m_activeIndex = 0;
    setState(State::Idle);
}
