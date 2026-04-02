#pragma once

#include <QByteArray>
#include <QList>

#include <cstdint>

namespace MotorProtocol {

inline constexpr uint8_t CmdErrorResponse = 0x01;
inline constexpr uint8_t CmdSetMotorIcAddr = 0x02;
inline constexpr uint8_t CmdI2cBusScan = 0x07;
inline constexpr uint8_t CmdReadRegister = 0x20;
inline constexpr uint8_t CmdWriteRegister = 0x21;

QByteArray encodeReadRegister(quint16 addr);
QByteArray encodeWriteRegister(quint16 addr, qint16 value);
QByteArray encodeI2cBusScan();
QByteArray encodeSetMotorIcAddr(uint8_t addr);

bool decodeReadRegisterResponse(const QByteArray &data, qint16 *valueOut);
QList<uint8_t> decodeI2cScanResponse(const QByteArray &data);
uint8_t decodeErrorCode(const QByteArray &data);

} // namespace MotorProtocol
