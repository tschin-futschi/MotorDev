// =============================================================================
// @file    cyclicwriteservice.cpp
// @brief   循环写入服务实现 — 定时器驱动的逐行写入、响应跟踪、错误计数
//
// 工作流程：
//   1. start() 初始化状态并启动定时器
//   2. 每个 tick 通过 m_rowDataProvider 获取当前行的 (addr, value)
//   3. 调用 RegisterService::writeSingleRow() 下发写入命令
//   4. 等待 rowWriteOk / rowWriteError 信号确认响应
//   5. 连续错误达到 MaxConsecutiveErrors 时自动停止
// =============================================================================
#include "services/cyclicwriteservice.h"

#include "services/commanddispatcher.h"
#include "services/registerservice.h"

#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(lcCyclicWrite, "motordev.cyclicwrite")

// =============================================================================
// 构造 / 析构
// =============================================================================

/// @brief 构造循环写入服务，绑定 RegisterService 的写入结果信号。
///
/// @param regService  寄存器服务指针，用于实际执行写入命令
/// @param parent      父对象
CyclicWriteService::CyclicWriteService(RegisterService *regService, QObject *parent)
    : QObject(parent)
    , m_regService(regService) {
    // ---------- 定时器：驱动周期性写入节奏 ----------
    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &CyclicWriteService::onTick);

    if (m_regService != nullptr) {
        // --- 写入成功：清除等待标记，重置连续错误计数 ---
        connect(m_regService, &RegisterService::rowWriteOk, this, [this](int globalRow) {
            if (!m_running || !m_waitingResponse || globalRow != m_lastWrittenRow) {
                return;                                 // 忽略不属于当前周期的响应
            }
            m_waitingResponse = false;
            m_consecutiveErrors = 0;
        });

        // --- 写入失败：累计连续错误，超限则自动停止 ---
        connect(m_regService, &RegisterService::rowWriteError, this, [this](int globalRow) {
            if (!m_running || !m_waitingResponse || globalRow != m_lastWrittenRow) {
                return;
            }
            m_waitingResponse = false;
            ++m_consecutiveErrors;
            qCWarning(lcCyclicWrite).noquote()
                << QStringLiteral("Write error row=%1 consecutive=%2")
                       .arg(globalRow)
                       .arg(m_consecutiveErrors);
            if (m_consecutiveErrors >= MaxConsecutiveErrors) {
                stopInternal("too many consecutive errors");
            }
        });
    }
}

CyclicWriteService::~CyclicWriteService() = default;

// =============================================================================
// 公共查询接口
// =============================================================================

/// @brief 返回循环写入是否正在运行。
bool CyclicWriteService::isRunning() const {
    return m_running;
}

/// @brief 返回当前定时器间隔（毫秒），未启动时返回 0。
int CyclicWriteService::intervalMs() const {
    return m_timer != nullptr ? m_timer->interval() : 0;
}

// =============================================================================
// 配置接口
// =============================================================================

/// @brief 设置参与循环写入的行数，并重置索引指针。
void CyclicWriteService::setRowCount(int count) {
    m_rowCount = qMax(1, count);
    m_currentIndex = 0;
}

/// @brief 设置行数据提供回调。
///
/// @p provider 签名: bool(int row, quint16 &addr, quint16 &value)
/// 返回 false 表示该行无有效数据，应跳过。
void CyclicWriteService::setRowDataProvider(RowDataProvider provider) {
    m_rowDataProvider = std::move(provider);
}

// =============================================================================
// 启停控制
// =============================================================================

/// @brief 启动循环写入，立即执行第一次 tick。
///
/// @param intervalMs  写入间隔（毫秒）
void CyclicWriteService::start(int intervalMs) {
    if (m_timer == nullptr || m_regService == nullptr || !m_rowDataProvider) {
        return;
    }

    // 重置所有运行时状态
    m_currentIndex = 0;
    m_lastWrittenRow = -1;
    m_waitingResponse = false;
    m_consecutiveErrors = 0;

    m_timer->setInterval(intervalMs);
    m_timer->start();
    m_running = true;
    qCInfo(lcCyclicWrite).noquote()
        << QStringLiteral("Start cyclic write intervalMs=%1 rowCount=%2")
               .arg(intervalMs)
               .arg(m_rowCount);
    emit runningChanged(true);

    onTick();                                           // 立即执行第一轮写入
}

/// @brief 手动停止循环写入。
void CyclicWriteService::stop() {
    stopInternal("manual");
}

/// @brief 内部停止实现，携带停止原因用于日志。
///
/// @param reason  停止原因字符串（"manual" / "too many consecutive errors"）
void CyclicWriteService::stopInternal(const char *reason) {
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    m_waitingResponse = false;
    m_lastWrittenRow = -1;

    if (!m_running) {
        return;                                         // 避免重复发射信号
    }
    m_running = false;
    qCInfo(lcCyclicWrite).noquote()
        << QStringLiteral("Stop cyclic write reason=%1").arg(QString::fromLatin1(reason));
    emit runningChanged(false);
}

// =============================================================================
// 定时器回调
// =============================================================================

/// @brief 每个 tick 尝试写入下一个有效行。
///
/// 算法：
///   1. 若上一次写入的响应尚未回来，跳过本次 tick（流控）
///   2. 从 m_currentIndex 开始轮询所有行，找到第一个有效行
///   3. 调用 writeSingleRow 下发写入，并标记等待响应
///   4. 若所有行都无有效数据，本次 tick 空转
void CyclicWriteService::onTick() {
    if (m_regService == nullptr || !m_rowDataProvider || m_rowCount <= 0) {
        return;
    }
    // 流控：上一次命令尚未应答，跳过本次
    if (m_waitingResponse) {
        qCDebug(lcCyclicWrite) << "Skipped tick: waiting response";
        return;
    }

    // 从当前索引开始，遍历所有行寻找有效数据
    for (int offset = 0; offset < m_rowCount; ++offset) {
        const int row = (m_currentIndex + offset) % m_rowCount;
        quint16 addr = 0;
        quint16 value = 0;
        if (!m_rowDataProvider(row, addr, value)) {
            continue;                                   // 该行无有效数据，跳过
        }

        // 下发低优先级写入命令
        m_regService->writeSingleRow(row, addr, static_cast<qint16>(value), CommandDispatcher::Low);
        m_currentIndex = (row + 1) % m_rowCount;        // 推进到下一行
        m_lastWrittenRow = row;
        m_waitingResponse = true;
        return;                                         // 每次 tick 只写一行
    }
}
