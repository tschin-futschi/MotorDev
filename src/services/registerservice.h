// RegisterService — 寄存器读写命令队列、协议收发、超时管理
#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QQueue>

#include <cstdint>

class QTimer;
class SerialManager;

class RegisterService : public QObject {
    Q_OBJECT

public:
    explicit RegisterService(SerialManager *serialManager, QObject *parent = nullptr);
    ~RegisterService() override;

    struct RowRequest {
        int globalRow = -1;
        quint16 addr = 0;
        qint16 value = 0;  // only used for write
    };

    void readSingleRow(int globalRow, quint16 addr);
    void writeSingleRow(int globalRow, quint16 addr, qint16 value);
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
    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void onSerialError(const QString &message);
    void onTimeout();

private:
    void enqueueAndStart(const QVector<RowRequest> &rows, bool isWrite);
    void processNextInQueue();
    void clearQueue();

    SerialManager *m_serialManager = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QQueue<RowRequest> m_pendingQueue;
    RowRequest m_currentRequest;
    bool m_hasPending = false;
    bool m_isWriteOp = false;
};
