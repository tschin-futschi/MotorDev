#pragma once

#include <QByteArray>
#include <QQueue>
#include <QWidget>

#include <cstdint>

class QPushButton;
class QLineEdit;
class RegisterTable;
class SerialManager;
class QTimer;

class RegisterRwTab : public QWidget {
    Q_OBJECT

public:
    explicit RegisterRwTab(SerialManager *serialManager, QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected);

private slots:
    void onReadRowRequested(int globalRow, quint16 addr);
    void onWriteRowRequested(int globalRow, quint16 addr, qint16 value);
    void onReadAllClicked();
    void onWriteAllClicked();
    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void onSerialError(const QString &message);
    void onConfigChanged();
    void onTimeout();

private:
    void setupUi();
    void connectSignals();
    void processNextInQueue();
    QString configFilePath() const;

    SerialManager *m_serialManager = nullptr;
    RegisterTable *m_registerTable = nullptr;
    QPushButton *m_readAllButton = nullptr;
    QPushButton *m_writeAllButton = nullptr;
    QPushButton *m_decButton = nullptr;
    QPushButton *m_hexButton = nullptr;
    QPushButton *m_batchBtn[4] = {};
    QLineEdit *m_batchDescEdit[4] = {};
    QLineEdit *m_batchPathEdit[4] = {};
    QPushButton *m_batchBrowseBtn[4] = {};
    QTimer *m_timeoutTimer = nullptr;
    QQueue<int> m_pendingQueue;
    int m_pendingRow = -1;
    bool m_isWriteOp = false;
};
