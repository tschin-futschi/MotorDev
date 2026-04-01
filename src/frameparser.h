#pragma once

#include <QByteArray>
#include <QVector>

#include <cstdint>

struct ControlFrame {
    uint8_t seq = 0;
    uint8_t cmd = 0;
    QByteArray data;
};

struct StreamFrame {
    uint8_t channelMask = 0;
    QVector<int16_t> samples;
};

class FrameParser {
public:
    enum class FrameType {
        None,
        Control,
        Stream
    };

    static QByteArray encodeControlFrame(uint8_t seq, uint8_t cmd, const QByteArray &data);
    static uint16_t calculateCrc16(const QByteArray &data);

    FrameType feedByte(uint8_t byte);

    const ControlFrame &controlFrame() const;
    const StreamFrame &streamFrame() const;

    void reset();

private:
    enum class State {
        WaitHeader,
        WaitHeader2,
        WaitSeq,
        WaitCmd,
        WaitLen,
        WaitData,
        WaitCrcHigh,
        WaitCrcLow,
        WaitChannelMask,
        WaitStreamLen,
        WaitStreamData,
        WaitXor
    };

    void resetControlState();
    void resetStreamState();
    FrameType finalizeControlFrame(uint8_t crcLow);
    FrameType finalizeStreamFrame(uint8_t xorValue);

    State m_state = State::WaitHeader;

    uint8_t m_seq = 0;
    uint8_t m_cmd = 0;
    uint8_t m_len = 0;
    QByteArray m_data;
    uint8_t m_crcHigh = 0;
    ControlFrame m_controlFrame;

    uint8_t m_channelMask = 0;
    uint8_t m_streamLen = 0;
    QByteArray m_streamData;
    StreamFrame m_streamFrame;
};
