#include "protocol/motor_protocol.h"

namespace MotorProtocol {

const char *commandName(uint8_t cmd) {
    switch (cmd) {
    case CmdHeartbeat: return "Heartbeat";
    case CmdErrorResponse: return "ErrorResponse";
    case CmdSetMotorIcAddr: return "SetMotorIcAddr";
    case CmdSetBaudrate: return "SetBaudrate";
    case CmdReset: return "Reset";
    case CmdMotorTest: return "MotorTest";
    case CmdDebugInfo: return "DebugInfo";
    case CmdI2cBusScan: return "I2cBusScan";
    case CmdPmicEnable: return "PmicEnable";
    case CmdSetPmicVoltage: return "SetPmicVoltage";
    case CmdPmicDisable: return "PmicDisable";
    case CmdReadRegister: return "ReadRegister";
    case CmdWriteRegister: return "WriteRegister";
    case CmdBulkRead: return "BulkRead";
    case CmdStartSampling: return "StartSampling";
    case CmdStopSampling: return "StopSampling";
    case CmdSetSampleInterval: return "SetSampleInterval";
    case CmdSetSampleChannels: return "SetSampleChannels";
    case CmdSetChannelRegisterMap: return "SetChannelRegisterMap";
    case CmdStartLinearGen: return "StartLinearGen";
    case CmdStartCosineGen: return "StartCosineGen";
    case CmdStopGenerator: return "StopGenerator";
    default: return "UnknownCommand";
    }
}

bool isValidSampleIntervalIndex(uint8_t intervalIndex) {
    return intervalIndex <= 0x06;
}

bool isValidSampleChannelMask(uint8_t channelMask) {
    return channelMask != 0x00;
}

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

QByteArray encodeI2cBusScan(uint8_t busIndex) {
    QByteArray payload;
    payload.append(static_cast<char>(busIndex));
    return payload;
}

QByteArray encodeSetMotorIcAddr(uint8_t addr) {
    QByteArray payload;
    payload.append(static_cast<char>(addr));
    return payload;
}

QByteArray encodePmicVoltage(quint16 drvvdd, quint16 iovdd, quint16 vcmvdd) {
    QByteArray payload;
    payload.reserve(6);
    payload.append(static_cast<char>((drvvdd >> 8) & 0xFF));
    payload.append(static_cast<char>(drvvdd & 0xFF));
    payload.append(static_cast<char>((iovdd >> 8) & 0xFF));
    payload.append(static_cast<char>(iovdd & 0xFF));
    payload.append(static_cast<char>((vcmvdd >> 8) & 0xFF));
    payload.append(static_cast<char>(vcmvdd & 0xFF));
    return payload;
}

QByteArray encodePmicEnable() {
    return {};
}

QByteArray encodePmicDisable() {
    return {};
}

QByteArray encodeStartSampling() {
    return {};
}

QByteArray encodeStopSampling() {
    return {};
}

QByteArray encodeSetSampleInterval(uint8_t intervalIndex) {
    if (!isValidSampleIntervalIndex(intervalIndex)) {
        return {};
    }

    QByteArray payload;
    payload.append(static_cast<char>(intervalIndex));
    return payload;
}

QByteArray encodeSetSampleChannels(uint8_t channelMask) {
    if (!isValidSampleChannelMask(channelMask)) {
        return {};
    }

    QByteArray payload;
    payload.append(static_cast<char>(channelMask));
    return payload;
}

QByteArray encodeSetChannelRegisterMap(const QVector<quint16> &registers) {
    if (registers.size() != 8) {
        return {};
    }

    QByteArray payload;
    payload.reserve(16);
    for (quint16 reg : registers) {
        payload.append(static_cast<char>((reg >> 8) & 0xFF));
        payload.append(static_cast<char>(reg & 0xFF));
    }
    return payload;
}

QByteArray encodeStartLinearGen(quint16 addr, qint16 min, qint16 max, qint16 step, quint16 intervalMs) {
    QByteArray payload;
    payload.reserve(10);
    auto appendU16 = [&](quint16 value) {
        payload.append(static_cast<char>((value >> 8) & 0xFF));
        payload.append(static_cast<char>(value & 0xFF));
    };
    auto appendS16 = [&](qint16 value) {
        appendU16(static_cast<quint16>(value));
    };

    appendU16(addr);
    appendS16(min);
    appendS16(max);
    appendS16(step);
    appendU16(intervalMs);
    return payload;
}

QByteArray encodeStartCosineGen(qint16 amplitude, qint16 offset, quint16 freqX100, uint8_t channelCount,
                                const QVector<QPair<quint16, qint16>> &channels) {
    QByteArray payload;
    payload.reserve(7 + static_cast<int>(channelCount) * 4);
    auto appendU16 = [&](quint16 value) {
        payload.append(static_cast<char>((value >> 8) & 0xFF));
        payload.append(static_cast<char>(value & 0xFF));
    };
    auto appendS16 = [&](qint16 value) {
        appendU16(static_cast<quint16>(value));
    };

    appendS16(amplitude);
    appendS16(offset);
    appendU16(freqX100);
    payload.append(static_cast<char>(channelCount));
    for (const auto &channel : channels) {
        appendU16(channel.first);
        appendS16(channel.second);
    }
    return payload;
}

QByteArray encodeStopGenerator() {
    return {};
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
