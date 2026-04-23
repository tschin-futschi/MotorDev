#pragma once

#include "widgets/scopegeneratorpanel.h"

#include <QByteArray>
#include <QObject>

class SerialManager;

class GeneratorService : public QObject {
    Q_OBJECT

public:
    explicit GeneratorService(SerialManager *serialManager, QObject *parent = nullptr);
    ~GeneratorService() override;

    bool isRunning() const;
    QString modeLabel() const;
    int cosineChannelCount() const;

public slots:
    void startLinear(quint16 addr, qint16 min, qint16 max, qint16 step, int intervalMs);
    void startCosine(qint16 amplitude, qint16 offset, double frequencyHz,
                     const QVector<ScopeGeneratorCosineChannel> &channels);
    void stop();

signals:
    void runningChanged(bool running);

private slots:
    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);

private:
    SerialManager *m_serialManager = nullptr;

    enum class Mode { None, Linear, Cosine };
    Mode m_mode = Mode::None;
    int m_cosineChannelCount = 0;
};
