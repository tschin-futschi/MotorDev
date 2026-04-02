#include "protocol/motor_protocol.h"

namespace MotorProtocol {

QByteArray encodeReadRegister(quint16 addr) {
    QByteArray payload;
    payload.append(static_cast<char>(addr >> 8));
    payload.append(static_cast<char>(addr & 0xFF));
    return payload;
}

QByteArray encodeWriteRegister(quint16 addr, qint16 value) {
    QByteArray payload = encodeReadRegister(addr);
    const quint16 rawValue = static_cast<quint16>(value);
    payload.append(static_cast<char>(rawValue >> 8));
    payload.append(static_cast<char>(rawValue & 0xFF));
    return payload;
}

QByteArray encodeI2cBusScan() {
    QByteArray payload;
    payload.append(static_cast<char>(0x02));
    return payload;
}

QByteArray encodeSetMotorIcAddr(uint8_t addr) {
    QByteArray payload;
    payload.append(static_cast<char>(addr));
    return payload;
}

bool decodeReadRegisterResponse(const QByteArray &data, qint16 *valueOut) {
    if (data.size() < 2 || valueOut == nullptr) {
        return false;
    }

    const quint16 rawValue = (static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1));
    *valueOut = static_cast<qint16>(rawValue);
    return true;
}

QList<uint8_t> decodeI2cScanResponse(const QByteArray &data) {
    QList<uint8_t> addresses;
    if (data.isEmpty()) {
        return addresses;
    }

    const int count = static_cast<quint8>(data.at(0));
    for (int index = 1; index <= count && index < data.size(); ++index) {
        addresses.append(static_cast<uint8_t>(data.at(index)));
    }

    return addresses;
}

uint8_t decodeErrorCode(const QByteArray &data) {
    if (data.isEmpty()) {
        return 0;
    }

    return static_cast<uint8_t>(data.at(0));
}

} // namespace MotorProtocol
