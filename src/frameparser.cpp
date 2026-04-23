#include "frameparser.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcFrameParser, "motordev.frame")

namespace {
constexpr uint8_t ControlHeader1 = 0xAA;
constexpr uint8_t ControlHeader2 = 0x55;
constexpr uint8_t StreamHeader = 0xBB;

int countEnabledChannels(uint8_t channelMask) {
    int count = 0;
    for (int bit = 0; bit < 8; ++bit) {
        if ((channelMask & (1u << bit)) != 0) {
            ++count;
        }
    }
    return count;
}

uint8_t calculateXor(uint8_t channelMask, uint8_t streamLen, const QByteArray &data) {
    uint8_t value = channelMask ^ streamLen;
    for (char byte : data) {
        value ^= static_cast<uint8_t>(byte);
    }
    return value;
}

QString formatByte(uint8_t value) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}
} // namespace

QByteArray FrameParser::encodeControlFrame(uint8_t seq, uint8_t cmd, const QByteArray &data) {
    QByteArray frame;
    frame.reserve(data.size() + 7);
    frame.append(static_cast<char>(ControlHeader1));
    frame.append(static_cast<char>(ControlHeader2));
    frame.append(static_cast<char>(seq));
    frame.append(static_cast<char>(cmd));
    frame.append(static_cast<char>(data.size() & 0xFF));
    frame.append(data);

    QByteArray crcInput;
    crcInput.reserve(data.size() + 3);
    crcInput.append(static_cast<char>(seq));
    crcInput.append(static_cast<char>(cmd));
    crcInput.append(static_cast<char>(data.size() & 0xFF));
    crcInput.append(data);

    const uint16_t crc = calculateCrc16(crcInput);
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    frame.append(static_cast<char>(crc & 0xFF));
    return frame;
}

uint16_t FrameParser::calculateCrc16(const QByteArray &data) {
    uint16_t crc = 0xFFFF;
    for (char byte : data) {
        crc ^= static_cast<uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const bool lsbSet = (crc & 0x0001u) != 0;
            crc >>= 1;
            if (lsbSet) {
                crc ^= 0x8005u;
            }
        }
    }
    return crc;
}

FrameParser::FrameType FrameParser::feedByte(uint8_t byte) {
    switch (m_state) {
    case State::WaitHeader:
        if (byte == ControlHeader1) {
            resetControlState();
            m_state = State::WaitHeader2;
        } else if (byte == StreamHeader) {
            resetStreamState();
            m_state = State::WaitChannelMask;
        }
        return FrameType::None;

    case State::WaitHeader2:
        if (byte == ControlHeader2) {
            m_state = State::WaitSeq;
        } else if (byte == ControlHeader1) {
            resetControlState();
            m_state = State::WaitHeader2;
        } else if (byte == StreamHeader) {
            resetStreamState();
            m_state = State::WaitChannelMask;
        } else {
            reset();
        }
        return FrameType::None;

    case State::WaitSeq:
        m_seq = byte;
        m_state = State::WaitCmd;
        return FrameType::None;

    case State::WaitCmd:
        m_cmd = byte;
        m_state = State::WaitLen;
        return FrameType::None;

    case State::WaitLen:
        m_len = byte;
        m_data.clear();
        if (m_len == 0) {
            m_state = State::WaitCrcHigh;
        } else {
            m_state = State::WaitData;
        }
        return FrameType::None;

    case State::WaitData:
        m_data.append(static_cast<char>(byte));
        if (m_data.size() >= m_len) {
            m_state = State::WaitCrcHigh;
        }
        return FrameType::None;

    case State::WaitCrcHigh:
        m_crcHigh = byte;
        m_state = State::WaitCrcLow;
        return FrameType::None;

    case State::WaitCrcLow:
        return finalizeControlFrame(byte);

    case State::WaitChannelMask:
        m_channelMask = byte;
        m_state = State::WaitStreamLen;
        return FrameType::None;

    case State::WaitStreamLen:
        m_streamLen = byte;
        m_streamData.clear();
        if (m_streamLen != countEnabledChannels(m_channelMask) * 2) {
            qCWarning(lcFrameParser).noquote()
                << QStringLiteral("Stream length mismatch: mask=%1 len=%2 expected=%3")
                       .arg(formatByte(m_channelMask))
                       .arg(m_streamLen)
                       .arg(countEnabledChannels(m_channelMask) * 2);
            reset();
            return FrameType::None;
        }
        if (m_streamLen == 0) {
            m_state = State::WaitXor;
        } else {
            m_state = State::WaitStreamData;
        }
        return FrameType::None;

    case State::WaitStreamData:
        m_streamData.append(static_cast<char>(byte));
        if (m_streamData.size() >= m_streamLen) {
            m_state = State::WaitXor;
        }
        return FrameType::None;

    case State::WaitXor:
        return finalizeStreamFrame(byte);
    }

    reset();
    return FrameType::None;
}

const ControlFrame &FrameParser::controlFrame() const {
    return m_controlFrame;
}

const StreamFrame &FrameParser::streamFrame() const {
    return m_streamFrame;
}

void FrameParser::reset() {
    resetControlState();
    resetStreamState();
    m_state = State::WaitHeader;
}

void FrameParser::resetControlState() {
    m_seq = 0;
    m_cmd = 0;
    m_len = 0;
    m_data.clear();
    m_crcHigh = 0;
}

void FrameParser::resetStreamState() {
    m_channelMask = 0;
    m_streamLen = 0;
    m_streamData.clear();
}

FrameParser::FrameType FrameParser::finalizeControlFrame(uint8_t crcLow) {
    QByteArray crcInput;
    crcInput.reserve(m_data.size() + 3);
    crcInput.append(static_cast<char>(m_seq));
    crcInput.append(static_cast<char>(m_cmd));
    crcInput.append(static_cast<char>(m_len));
    crcInput.append(m_data);

    const uint16_t expectedCrc = calculateCrc16(crcInput);
    const uint16_t receivedCrc = static_cast<uint16_t>((static_cast<uint16_t>(m_crcHigh) << 8) | crcLow);

    if (expectedCrc == receivedCrc) {
        m_controlFrame.seq = m_seq;
        m_controlFrame.cmd = m_cmd;
        m_controlFrame.data = m_data;
        reset();
        return FrameType::Control;
    }

    qCWarning(lcFrameParser).noquote()
        << QStringLiteral("CRC mismatch: seq=%1 cmd=%2 len=%3 expected=0x%4 received=0x%5 payload=%6")
               .arg(formatByte(m_seq))
               .arg(formatByte(m_cmd))
               .arg(m_len)
               .arg(expectedCrc, 4, 16, QLatin1Char('0'))
               .arg(receivedCrc, 4, 16, QLatin1Char('0'))
               .arg(formatPayloadHex(m_data))
               .toUpper();
    reset();
    return FrameType::None;
}

FrameParser::FrameType FrameParser::finalizeStreamFrame(uint8_t xorValue) {
    const uint8_t expectedXor = calculateXor(m_channelMask, m_streamLen, m_streamData);
    if (expectedXor == xorValue) {
        m_streamFrame.channelMask = m_channelMask;
        m_streamFrame.samples.clear();
        m_streamFrame.samples.reserve(m_streamData.size() / 2);

        for (int i = 0; i + 1 < m_streamData.size(); i += 2) {
            const uint16_t high = static_cast<uint8_t>(m_streamData.at(i));
            const uint16_t low = static_cast<uint8_t>(m_streamData.at(i + 1));
            const uint16_t raw = static_cast<uint16_t>((high << 8) | low);
            m_streamFrame.samples.append(static_cast<int16_t>(raw));
        }

        reset();
        return FrameType::Stream;
    }

    qCWarning(lcFrameParser).noquote()
        << QStringLiteral("XOR mismatch: mask=%1 len=%2 expected=%3 received=%4 payload=%5")
               .arg(formatByte(m_channelMask))
               .arg(m_streamLen)
               .arg(formatByte(expectedXor))
               .arg(formatByte(xorValue))
               .arg(formatPayloadHex(m_streamData));
    reset();
    return FrameType::None;
}
