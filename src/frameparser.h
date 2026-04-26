// =============================================================================
// @file    frameparser.h
// @brief   二进制帧解析器（传输层）
//
// 实现字节流到结构化帧的解析，以及控制帧的编码。
// 支持两种帧类型：
// - 控制帧（ControlFrame）：AA 55 开头，用于指令请求/响应，含 CRC16 校验
// - 流帧  （StreamFrame） ：BB 开头，用于示波器实时数据上报，含 XOR 校验
//
// 解析采用逐字节喂入的状态机模式，适配串口的异步逐字节接收场景。
// SerialManager 是本类的唯一使用者。
// =============================================================================

#pragma once

#include <QByteArray>
#include <QVector>

#include <cstdint>

/// @brief 控制帧数据结构
///
/// 控制帧格式：[AA][55][SEQ][CMD][LEN][DATA...][CRC_H][CRC_L]
/// 解析完成后，seq/cmd/data 被填充到此结构中。
struct ControlFrame {
    uint8_t seq = 0;       ///< 序列号，用于请求-响应配对
    uint8_t cmd = 0;       ///< 命令字节，定义见 MotorProtocol
    QByteArray data;       ///< 载荷数据（长度由 LEN 字段决定）
};

/// @brief 流帧数据结构
///
/// 流帧格式：[BB][CHANNEL_MASK][LEN][DATA...][XOR]
/// 用于示波器实时采样数据的高频上报（每帧包含一个采样点的多通道数据）。
struct StreamFrame {
    uint8_t channelMask = 0;       ///< 通道掩码，bit0~bit7 对应 CH1~CH8
    QVector<int16_t> samples;      ///< 各启用通道的采样值（int16，大端字节序解码）
};

/// @brief 二进制帧解析器
///
/// 状态机逐字节解析串口数据流，识别控制帧和流帧。
/// 使用方式：循环调用 feedByte()，返回值非 None 时读取对应帧数据。
///
/// 线程安全：非线程安全，仅在 SerialManager 的工作线程中使用。
class FrameParser {
public:
    /// @brief 帧类型枚举
    enum class FrameType {
        None,      ///< 帧尚未完整，继续喂入字节
        Control,   ///< 解析到完整的控制帧
        Stream     ///< 解析到完整的流帧
    };

    /// @brief 编码一个控制帧
    /// @param seq   序列号
    /// @param cmd   命令字节
    /// @param data  载荷数据
    /// @return 编码后的完整帧字节数组（含帧头、CRC）
    static QByteArray encodeControlFrame(uint8_t seq, uint8_t cmd, const QByteArray &data);

    /// @brief 计算 CRC-16 校验值
    /// @param data  待校验的数据（SEQ + CMD + LEN + DATA）
    /// @return CRC-16 校验值（多项式 0x8005，初始值 0xFFFF，LSB 优先）
    static uint16_t calculateCrc16(const QByteArray &data);

    /// @brief 喂入一个字节，驱动状态机前进
    /// @param byte  从串口接收到的一个字节
    /// @return 帧类型；返回 Control/Stream 时可读取对应帧数据
    FrameType feedByte(uint8_t byte);

    /// @brief 获取最近一次解析完成的控制帧
    /// @return 控制帧引用（仅在 feedByte 返回 Control 后有效）
    const ControlFrame &controlFrame() const;

    /// @brief 获取最近一次解析完成的流帧
    /// @return 流帧引用（仅在 feedByte 返回 Stream 后有效）
    const StreamFrame &streamFrame() const;

    /// @brief 重置解析器状态，回到等待帧头
    void reset();

private:
    /// @brief 状态机的内部状态枚举
    enum class State {
        WaitHeader,       ///< 等待帧头第一字节（0xAA 或 0xBB）
        WaitHeader2,      ///< 等待控制帧头第二字节（0x55）
        WaitSeq,          ///< 等待序列号
        WaitCmd,          ///< 等待命令字节
        WaitLen,          ///< 等待载荷长度
        WaitData,         ///< 接收载荷数据
        WaitCrcHigh,      ///< 等待 CRC 高字节
        WaitCrcLow,       ///< 等待 CRC 低字节
        WaitChannelMask,  ///< 等待流帧通道掩码
        WaitStreamLen,    ///< 等待流帧数据长度
        WaitStreamData,   ///< 接收流帧数据
        WaitXor           ///< 等待流帧 XOR 校验字节
    };

    /// @brief 重置控制帧解析中间状态
    void resetControlState();

    /// @brief 重置流帧解析中间状态
    void resetStreamState();

    /// @brief 完成控制帧解析：校验 CRC 并填充 m_controlFrame
    /// @param crcLow  CRC 低字节
    /// @return 校验通过返回 Control，失败返回 None
    FrameType finalizeControlFrame(uint8_t crcLow);

    /// @brief 完成流帧解析：校验 XOR 并将字节数据解码为 int16 采样值
    /// @param xorValue  XOR 校验字节
    /// @return 校验通过返回 Stream，失败返回 None
    FrameType finalizeStreamFrame(uint8_t xorValue);

    State m_state = State::WaitHeader;  ///< 当前状态机状态

    // --- 控制帧解析中间变量 ---
    uint8_t m_seq = 0;            ///< 当前帧序列号
    uint8_t m_cmd = 0;            ///< 当前帧命令字节
    uint8_t m_len = 0;            ///< 当前帧载荷长度
    QByteArray m_data;            ///< 当前帧载荷缓冲区
    uint8_t m_crcHigh = 0;        ///< CRC 高字节暂存
    ControlFrame m_controlFrame;  ///< 最近一次解析完成的控制帧

    // --- 流帧解析中间变量 ---
    uint8_t m_channelMask = 0;    ///< 当前流帧通道掩码
    uint8_t m_streamLen = 0;      ///< 当前流帧数据长度
    QByteArray m_streamData;      ///< 当前流帧数据缓冲区
    StreamFrame m_streamFrame;    ///< 最近一次解析完成的流帧
};
