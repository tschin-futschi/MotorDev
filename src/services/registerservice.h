// =============================================================================
// @file    registerservice.h
// @brief   寄存器读写服务（服务层）— 命令队列、协议收发、超时管理
//
// 封装寄存器单次/批量读写的完整流程：
// - 队列管理：单条命令串行执行（同一时刻仅一条在途）。单行读写在已有 in-flight 命令时
//   **插入队首优先处理**（优先响应最新用户交互）；批量读写按提交顺序入队
// - 超时处理：每条命令 500ms 超时，超时后自动跳到下一条
// - 会话管理：通过 sessionId 防止过期响应被误处理
//
// 被 RegisterRwTab 和 CyclicWriteService 使用。
// =============================================================================
#pragma once

#include "services/commanddispatcher.h"

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QVector>

#include <cstdint>

class QTimer;

class RegisterService : public QObject {
    Q_OBJECT

public:
    explicit RegisterService(CommandDispatcher *dispatcher, QObject *parent = nullptr);
    ~RegisterService() override;

    struct RowRequest {
        int globalRow = -1;
        quint16 addr = 0;
        qint16 value = 0;
        bool isWrite = false;
    };

    void readSingleRow(int globalRow, quint16 addr);
    void writeSingleRow(int globalRow, quint16 addr, qint16 value,
                        CommandDispatcher::Priority priority = static_cast<CommandDispatcher::Priority>(2));
    void readBatch(const QVector<RowRequest> &rows);
    void writeBatch(const QVector<RowRequest> &rows);

signals:
    void rowReadOk(int globalRow, qint16 value);
    void rowReadError(int globalRow);
    void rowWriteOk(int globalRow);
    void rowWriteError(int globalRow);
    void rowPending(int globalRow);
    void queueFinished(bool wasWrite);

private slots:
    void onCommandDispatched(uint32_t ticket, uint8_t cmd, uint8_t seq);
    void onTimeout();

private:
    void enqueueAndStart(const QVector<RowRequest> &rows, bool isWrite,
                         CommandDispatcher::Priority priority = static_cast<CommandDispatcher::Priority>(2));
    void submitCurrentRequest();
    void handleResponse(uint8_t cmd, uint8_t seq, const QByteArray &data, uint32_t sessionId);
    void processNextInQueue();
    void clearQueue();

    CommandDispatcher *m_dispatcher = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QQueue<RowRequest> m_pendingQueue;
    RowRequest m_currentRequest;
    uint32_t m_activeTicket = 0;
    uint32_t m_sessionId = 0;
    bool m_hasPending = false;
    bool m_isWriteOp = false;
    CommandDispatcher::Priority m_priority = static_cast<CommandDispatcher::Priority>(2);
};
