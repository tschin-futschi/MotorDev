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

    RegisterService *m_regService = nullptr;
    QTimer *m_timer = nullptr;
    RowDataProvider m_rowDataProvider;
    int m_rowCount = 8;
    int m_currentIndex = 0;
    bool m_running = false;
};
