#pragma once

#include <QByteArray>
#include <QList>
#include <QVector>

#include <cstdint>

namespace MotorProtocol {

inline constexpr uint8_t CmdErrorResponse = 0x01;
inline constexpr uint8_t CmdSetMotorIcAddr = 0x02;
inline constexpr uint8_t CmdI2cBusScan = 0x07;
inline constexpr uint8_t CmdReadRegister = 0x20;
inline constexpr uint8_t CmdWriteRegister = 0x21;
inline constexpr uint8_t CmdStartSampling = 0x50;
inline constexpr uint8_t CmdStopSampling = 0x51;
inline constexpr uint8_t CmdSetSampleInterval = 0x52;
inline constexpr uint8_t CmdSetSampleChannels = 0x53;
inline constexpr uint8_t CmdSetChannelRegisterMap = 0x54;

QByteArray encodeReadRegister(quint16 addr);
QByteArray encodeWriteRegister(quint16 addr, qint16 value);
QByteArray encodeI2cBusScan();
QByteArray encodeSetMotorIcAddr(uint8_t addr);
QByteArray encodeStartSampling();
QByteArray encodeStopSampling();
QByteArray encodeSetSampleInterval(uint8_t intervalIndex);
QByteArray encodeSetSampleChannels(uint8_t channelMask);
QByteArray encodeSetChannelRegisterMap(const QVector<quint16> &registers);

bool decodeReadRegisterResponse(const QByteArray &data, qint16 *valueOut);
QList<uint8_t> decodeI2cScanResponse(const QByteArray &data);
uint8_t decodeErrorCode(const QByteArray &data);
bool isValidSampleIntervalIndex(uint8_t intervalIndex);
bool isValidSampleChannelMask(uint8_t channelMask);

} // namespace MotorProtocol
