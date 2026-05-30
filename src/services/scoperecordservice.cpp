// =============================================================================
// @file    scoperecordservice.cpp
// @brief   示波器数据记录服务实现 — CSV 持续写入、与采样生命周期绑定
// =============================================================================
#include "services/scoperecordservice.h"

#include "models/scopechannelmodel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QTextStream>
#include <QTimer>

Q_LOGGING_CATEGORY(lcScopeRecord, "motordev.scoperecord")

namespace {
constexpr int kFlushIntervalMs = 1000;     ///< 周期 flush 间隔（崩溃安全）
}

// =============================================================================
// 构造 / 析构
// =============================================================================

ScopeRecordService::ScopeRecordService(ScopeChannelModel *channelModel, QObject *parent)
    : QObject(parent)
    , m_channelModel(channelModel) {
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(kFlushIntervalMs);
    connect(m_flushTimer, &QTimer::timeout, this, [this]() {
        if (m_stream != nullptr) {
            m_stream->flush();
        }
    });
}

ScopeRecordService::~ScopeRecordService() {
    stopRecording();
}

// =============================================================================
// 配置
// =============================================================================

void ScopeRecordService::setRecordDirectory(const QString &dir) {
    m_recordDir = dir;
}

void ScopeRecordService::onAcquisitionConfigured(int sampleIntervalUs, int displayWindowMs) {
    Q_UNUSED(displayWindowMs);
    m_sampleIntervalUs = sampleIntervalUs;
}

/// @brief 记录目录下最新的 Scope_*.csv（文件名含起始时间戳，字典序最大即最新）。
QString ScopeRecordService::latestRecordFile() const {
    if (m_recordDir.trimmed().isEmpty()) {
        return QString();
    }
    QDir dir(m_recordDir);
    if (!dir.exists()) {
        return QString();
    }
    const QStringList files = dir.entryList(QStringList{QStringLiteral("Scope_*.csv")},
                                            QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        return QString();
    }
    return dir.filePath(files.last());
}

// =============================================================================
// 采样生命周期 → 录制启停
// =============================================================================

void ScopeRecordService::onRunningChanged(bool running) {
    if (running) {
        startRecording();
    } else {
        stopRecording();
    }
}

/// @brief 开始录制：校验目录 → 快照通道列 → 建文件 → 写表头。
void ScopeRecordService::startRecording() {
    if (m_recording) {
        return;
    }

    // ---------- 目录校验：未设 / 无效则不录并提示 ----------
    if (m_recordDir.trimmed().isEmpty()) {
        emit recordError(tr("未设置记录目录，本次采样未记录"));
        return;
    }
    QDir dir(m_recordDir);
    if (!dir.exists()) {
        emit recordError(tr("记录目录无效，本次采样未记录：%1").arg(m_recordDir));
        return;
    }

    // ---------- 记录通道 = 实际采样通道（与 currentChannelMask 一致）----------
    const uint8_t mask = m_channelModel != nullptr ? m_channelModel->currentChannelMask() : 0;
    m_recordedChannels.clear();
    for (int ch = 0; ch < 8; ++ch) {
        if ((mask & (1u << ch)) != 0) {
            m_recordedChannels.append(ch);
        }
    }
    if (m_recordedChannels.isEmpty()) {
        emit recordError(tr("无有效采样通道，本次采样未记录"));
        return;
    }

    // ---------- 文件名 Scope_YYYYMMDD_HHMMSS.csv，同秒冲突追加 _N ----------
    const QDateTime now = QDateTime::currentDateTime();
    const QString stamp = now.toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString base = QStringLiteral("Scope_%1.csv").arg(stamp);
    int suffix = 1;
    while (dir.exists(base)) {
        base = QStringLiteral("Scope_%1_%2.csv").arg(stamp).arg(suffix++);
    }
    const QString fullPath = dir.filePath(base);

    auto *file = new QFile(fullPath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit recordError(tr("无法创建记录文件：%1").arg(fullPath));
        delete file;
        return;
    }
    m_file = file;
    m_stream = new QTextStream(m_file);

    // ---------- 表头：元信息注释 + 列名 ----------
    *m_stream << "# MotorDev scope record\n";
    *m_stream << "# interval_us=" << (m_sampleIntervalUs > 0 ? m_sampleIntervalUs : 0)
              << "  start=" << now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n";

    QString chMeta = QStringLiteral("#");
    QString header = QStringLiteral("time_us");
    for (int ch : m_recordedChannels) {
        const QString label = QStringLiteral("CH%1").arg(ch + 1);
        header += QLatin1Char(',');
        header += label;
        const ScopeChannelState &c = m_channelModel->channel(ch);
        chMeta += QLatin1Char(' ');
        chMeta += label;
        if (!c.description.isEmpty()) {
            chMeta += QStringLiteral("(%1,%2)").arg(c.description, c.addressText);
        } else {
            chMeta += QStringLiteral("(%1)").arg(c.addressText);
        }
    }
    *m_stream << chMeta << "\n";
    *m_stream << header << "\n";

    m_rowIndex = 0;
    m_recording = true;
    m_currentFileName = base;
    m_flushTimer->start();
    qCInfo(lcScopeRecord).noquote() << QStringLiteral("Start recording -> %1").arg(fullPath);
    emit recordingChanged(true, base);
}

/// @brief 停止录制：最终 flush + 关文件。
void ScopeRecordService::stopRecording() {
    if (!m_recording) {
        return;
    }
    m_recording = false;
    if (m_flushTimer != nullptr) {
        m_flushTimer->stop();
    }
    if (m_stream != nullptr) {
        m_stream->flush();
        delete m_stream;
        m_stream = nullptr;
    }
    if (m_file != nullptr) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    const QString finished = m_currentFileName;
    m_currentFileName.clear();
    qCInfo(lcScopeRecord).noquote()
        << QStringLiteral("Stop recording (%1 rows) -> %2").arg(m_rowIndex).arg(finished);
    emit recordingChanged(false, finished);
}

// =============================================================================
// 数据写入
// =============================================================================

/// @brief 收到一帧采样：解出各通道值，写一行 time_us + 记录通道值。
///
/// 通道值提取镜像 ScopePlotWidget::appendSamples：按 mask 位序逐个消费 samples。
void ScopeRecordService::onSamplesReceived(uint8_t channelMask, const QVector<int16_t> &samples) {
    if (!m_recording || m_stream == nullptr) {
        return;
    }

    int chValue[8] = {};
    bool chHas[8] = {};
    int sampleIndex = 0;
    for (int ch = 0; ch < 8; ++ch) {
        if ((channelMask & (1u << ch)) == 0) {
            continue;
        }
        if (sampleIndex >= samples.size()) {
            break;
        }
        chValue[ch] = samples.at(sampleIndex++);
        chHas[ch] = true;
    }

    const qint64 timeUs = m_rowIndex * static_cast<qint64>(qMax(0, m_sampleIntervalUs));
    QString line = QString::number(timeUs);
    for (int ch : m_recordedChannels) {
        line += QLatin1Char(',');
        if (chHas[ch]) {
            line += QString::number(chValue[ch]);
        }
    }
    *m_stream << line << "\n";
    ++m_rowIndex;
}
