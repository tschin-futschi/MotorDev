// =============================================================================
// @file    cyclicwriteservice.h
// @brief   循环写入服务（服务层）— 按定时器周期向寄存器逐行写入
//
// 以固定间隔轮询各行，通过 RowDataProvider 回调获取待写数据，
// 通过 RegisterService 发送低优先级写命令。
// 连续错误超过阈值（5 次）自动停止。
// =============================================================================
#pragma once

#include <QObject>
#include <functional>

class RegisterService;
class QTimer;

class CyclicWriteService : public QObject {
    Q_OBJECT

public:
    using RowDataProvider = std::function<bool(int row, quint16 &addr, quint16 &value)>;

    explicit CyclicWriteService(RegisterService *regService, QObject *parent = nullptr);
    ~CyclicWriteService() override;

    bool isRunning() const;
    int intervalMs() const;
    void setRowCount(int count);
    void setRowDataProvider(RowDataProvider provider);

public slots:
    void start(int intervalMs);
    void stop();

signals:
    void runningChanged(bool running);

private:
    void onTick();
    void stopInternal(const char *reason);

    RegisterService *m_regService = nullptr;
    QTimer *m_timer = nullptr;
    RowDataProvider m_rowDataProvider;
    int m_rowCount = 8;
    int m_currentIndex = 0;
    int m_lastWrittenRow = -1;
    int m_consecutiveErrors = 0;
    bool m_waitingResponse = false;
    bool m_running = false;
    static constexpr int MaxConsecutiveErrors = 5;
};
