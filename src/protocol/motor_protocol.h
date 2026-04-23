#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QVector>

#include <cstdint>

namespace MotorProtocol {

inline constexpr uint8_t CmdHeartbeat = 0x00;
inline constexpr uint8_t CmdErrorResponse = 0x01;
inline constexpr uint8_t CmdSetMotorIcAddr = 0x02;
inline constexpr uint8_t CmdSetBaudrate = 0x03;
inline constexpr uint8_t CmdReset = 0x04;
inline constexpr uint8_t CmdMotorTest = 0x05;
inline constexpr uint8_t CmdDebugInfo = 0x06;
inline constexpr uint8_t CmdI2cBusScan = 0x07;
inline constexpr uint8_t CmdPmicEnable = 0x08;
inline constexpr uint8_t CmdSetPmicVoltage = 0x09;
inline constexpr uint8_t CmdPmicDisable = 0x0A;
inline constexpr uint8_t CmdReadRegister = 0x20;
inline constexpr uint8_t CmdWriteRegister = 0x21;
inline constexpr uint8_t CmdBulkRead = 0x22;
inline constexpr uint8_t CmdStartSampling = 0x50;
inline constexpr uint8_t CmdStopSampling = 0x51;
inline constexpr uint8_t CmdSetSampleInterval = 0x52;
inline constexpr uint8_t CmdSetSampleChannels = 0x53;
inline constexpr uint8_t CmdSetChannelRegisterMap = 0x54;
inline constexpr uint8_t CmdStartLinearGen = 0x55;
inline constexpr uint8_t CmdStartCosineGen = 0x56;
inline constexpr uint8_t CmdStopGenerator = 0x57;

QByteArray encodeReadRegister(quint16 addr);
QByteArray encodeWriteRegister(quint16 addr, qint16 value);
QByteArray encodeI2cBusScan(uint8_t busIndex = 0x02);
QByteArray encodeSetMotorIcAddr(uint8_t addr);
QByteArray encodePmicVoltage(quint16 drvvdd, quint16 iovdd, quint16 vcmvdd);
QByteArray encodePmicEnable();
QByteArray encodePmicDisable();
QByteArray encodeStartSampling();
QByteArray encodeStopSampling();
QByteArray encodeSetSampleInterval(uint8_t intervalIndex);
QByteArray encodeSetSampleChannels(uint8_t channelMask);
QByteArray encodeSetChannelRegisterMap(const QVector<quint16> &registers);
QByteArray encodeStartLinearGen(quint16 addr, qint16 min, qint16 max, qint16 step, quint16 intervalMs);
QByteArray encodeStartCosineGen(qint16 amplitude, qint16 offset, quint16 freqX100, uint8_t channelCount,
                                const QVector<QPair<quint16, qint16>> &channels);
QByteArray encodeStopGenerator();

bool decodeReadRegisterResponse(const QByteArray &data, qint16 *valueOut);
QList<uint8_t> decodeI2cScanResponse(const QByteArray &data);
uint8_t decodeErrorCode(const QByteArray &data);
bool isValidSampleIntervalIndex(uint8_t intervalIndex);
bool isValidSampleChannelMask(uint8_t channelMask);

} // namespace MotorProtocol
