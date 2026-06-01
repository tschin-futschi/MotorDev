// =============================================================================
// @file    frameparser.cpp
// @brief   二进制帧解析器实现
//
// 实现控制帧/流帧的状态机解析、CRC-16 校验和控制帧编码。
// =============================================================================

#include "frameparser.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcFrameParser, "motordev.frame")

namespace {

constexpr uint8_t ControlHeader1 = 0xAA;  ///< 控制帧帧头第一字节
constexpr uint8_t ControlHeader2 = 0x55;  ///< 控制帧帧头第二字节
constexpr uint8_t StreamHeader = 0xBB;    ///< 流帧帧头

/// @brief 统计通道掩码中启用的通道数量
/// @param channelMask  8 位通道掩码，每 bit 对应一个通道
/// @return 置位的 bit 数量
int countEnabledChannels(uint8_t channelMask) {
    int count = 0;
    for (int bit = 0; bit < 8; ++bit) {
        if ((channelMask & (1u << bit)) != 0) {
            ++count;
        }
    }
    return count;
}

/// @brief 计算流帧的 XOR 校验值
/// @param channelMask  通道掩码
/// @param streamLen    数据长度
/// @param data         流帧载荷数据
/// @return XOR 校验值 = channelMask ^ streamLen ^ data[0] ^ data[1] ^ ...
uint8_t calculateXor(uint8_t channelMask, uint8_t streamLen, const QByteArray &data) {
    uint8_t value = channelMask ^ streamLen;
    for (char byte : data) {
        value ^= static_cast<uint8_t>(byte);
    }
    return value;
}

/// @brief 将字节格式化为 "0x??" 的十六进制字符串（用于日志输出）
QString formatByte(uint8_t value) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

/// @brief 将字节数组格式化为空格分隔的十六进制字符串（用于日志输出）
QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}
} // namespace

// -----------------------------------------------------------------------------
// 控制帧编码
// -----------------------------------------------------------------------------

QByteArray FrameParser::encodeControlFrame(uint8_t seq, uint8_t cmd, const QByteArray &data) {
    // LEN 字段仅 1 字节（最大 255）。>255 载荷若继续编码会被 `& 0xFF` 静默截断，
    // 导致帧长 / CRC 范围错位、接收端必然解析失败且无告警。此处直接拒绝并返回空帧
    // （本协议所有载荷均 < 255，触发即为调用方错误）；调用方写空帧不会发出有效命令，
    // 由其超时/写校验兜底，而非静默发出损坏帧。
    if (data.size() > 255) {
        qCWarning(lcFrameParser) << "encodeControlFrame: payload too large, rejected"
                                 << "size=" << data.size() << "cmd=" << cmd;
        return QByteArray();
    }

    QByteArray frame;
    frame.reserve(data.size() + 7);  // 帧头(2) + SEQ(1) + CMD(1) + LEN(1) + CRC(2)

    // 组装帧：[AA][55][SEQ][CMD][LEN][DATA...][CRC_H][CRC_L]
    frame.append(static_cast<char>(ControlHeader1));
    frame.append(static_cast<char>(ControlHeader2));
    frame.append(static_cast<char>(seq));
    frame.append(static_cast<char>(cmd));
    frame.append(static_cast<char>(data.size() & 0xFF));
    frame.append(data);

    // CRC 计算范围：SEQ + CMD + LEN + DATA
    QByteArray crcInput;
    crcInput.reserve(data.size() + 3);
    crcInput.append(static_cast<char>(seq));
    crcInput.append(static_cast<char>(cmd));
    crcInput.append(static_cast<char>(data.size() & 0xFF));
    crcInput.append(data);

    const uint16_t crc = calculateCrc16(crcInput);
    frame.append(static_cast<char>((crc >> 8) & 0xFF));  // CRC 高字节
    frame.append(static_cast<char>(crc & 0xFF));          // CRC 低字节
    return frame;
}

// -----------------------------------------------------------------------------
// CRC-16 校验
// -----------------------------------------------------------------------------

/// @brief CRC-16 校验算法
///
/// 多项式：0x8005（反转），初始值：0xFFFF，LSB 优先处理。
/// 注意：本算法用 0x8005 + 右移，与标准 Modbus CRC-16（反射 0xA001）不同，不可混用。
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

// -----------------------------------------------------------------------------
// 状态机解析
// -----------------------------------------------------------------------------

/// @brief 逐字节驱动解析状态机
///
/// 状态机转换示意：
/// 控制帧：WaitHeader → WaitHeader2 → WaitSeq → WaitCmd → WaitLen
///         → WaitData → WaitCrcHigh → WaitCrcLow → (完成)
/// 流帧：  WaitHeader → WaitChannelMask → WaitStreamLen
///         → WaitStreamData → WaitXor → (完成)
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
            // 连续收到 0xAA，重新开始等待第二字节
            resetControlState();
            m_state = State::WaitHeader2;
        } else if (byte == StreamHeader) {
            // 控制帧头不匹配，但发现流帧头，切换到流帧解析
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
            m_state = State::WaitCrcHigh;  // 无载荷，直接等待 CRC
        } else {
            m_state = State::WaitData;
        }
        return FrameType::None;

    case State::WaitData:
        m_data.append(static_cast<char>(byte));
        if (m_data.size() >= m_len) {
            m_state = State::WaitCrcHigh;  // 载荷接收完毕
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
        // 校验数据长度：必须等于 启用通道数 × 2（每通道 2 字节 int16）
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
            m_state = State::WaitXor;  // 流帧数据接收完毕
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

/// @brief 完成控制帧解析
///
/// 重新计算 CRC 并与接收到的 CRC 对比。校验通过则填充 m_controlFrame 并返回 Control；
/// 校验失败则输出警告日志并丢弃该帧。
FrameParser::FrameType FrameParser::finalizeControlFrame(uint8_t crcLow) {
    // 重建 CRC 输入：SEQ + CMD + LEN + DATA
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

/// @brief 完成流帧解析
///
/// 校验 XOR 后，将大端字节序的采样数据解码为 int16 数组。
/// 每两个字节组成一个 int16 采样值，按通道掩码顺序排列。
FrameParser::FrameType FrameParser::finalizeStreamFrame(uint8_t xorValue) {
    const uint8_t expectedXor = calculateXor(m_channelMask, m_streamLen, m_streamData);
    if (expectedXor == xorValue) {
        m_streamFrame.channelMask = m_channelMask;
        m_streamFrame.samples.clear();
        m_streamFrame.samples.reserve(m_streamData.size() / 2);

        // 大端字节序解码：每 2 字节组成一个 int16 采样值
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
