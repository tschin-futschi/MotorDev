// =============================================================================
// @file    motor_protocol.cpp
// @brief   电机驱动协议编解码实现
//
// 实现所有命令的载荷编码和响应解码逻辑。
// 所有多字节字段采用大端字节序（MSB first）。
// =============================================================================

#include "protocol/motor_protocol.h"

namespace MotorProtocol {

// -----------------------------------------------------------------------------
// 命令名称查询
// -----------------------------------------------------------------------------

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
    case CmdBootStatus: return "BootStatus";
    case CmdReadRegister: return "ReadRegister";
    case CmdWriteRegister: return "WriteRegister";
    case CmdBulkRead: return "BulkRead";
    case CmdI2cTransferWrite: return "I2cTransferWrite";
    case CmdI2cTransferRead: return "I2cTransferRead";
    case CmdFlashBegin: return "FlashBegin";
    case CmdFlashData: return "FlashData";
    case CmdFlashExec: return "FlashExec";
    case CmdFlashStatus: return "FlashStatus";
    case CmdFlashCancel: return "FlashCancel";
    case CmdFlashResetChip: return "FlashResetChip";
    case CmdFlashExecProgress: return "FlashExecProgress";
    case CmdFlashStoreWriteBegin: return "FlashStoreWriteBegin";
    case CmdFlashStoreWriteData:  return "FlashStoreWriteData";
    case CmdFlashStoreWriteEnd:   return "FlashStoreWriteEnd";
    case CmdFlashStoreReadBegin:  return "FlashStoreReadBegin";
    case CmdFlashStoreReadData:   return "FlashStoreReadData";
    case CmdFlashStoreInfo:       return "FlashStoreInfo";
    case CmdStartSampling: return "StartSampling";
    case CmdStopSampling: return "StopSampling";
    case CmdSetSampleInterval: return "SetSampleInterval";
    case CmdSetSampleChannels: return "SetSampleChannels";
    case CmdSetChannelRegisterMap: return "SetChannelRegisterMap";
    case CmdStartLinearGen: return "StartLinearGen";
    case CmdStartCosineGen: return "StartCosineGen";
    case CmdStopGenerator: return "StopGenerator";
    case CmdStartSawtoothGen: return "StartSawtoothGen";
    default: return "UnknownCommand";
    }
}

// -----------------------------------------------------------------------------
// 校验工具
// -----------------------------------------------------------------------------

bool isValidSampleIntervalIndex(uint8_t intervalIndex) {
    return intervalIndex <= 0x06;  // 有效范围 0~6
}

bool isValidSampleChannelMask(uint8_t channelMask) {
    return channelMask != 0x00;  // 至少要启用一个通道
}

// -----------------------------------------------------------------------------
// 请求载荷编码
// -----------------------------------------------------------------------------

QByteArray encodeReadRegister(quint16 addr) {
    QByteArray payload;
    payload.append(static_cast<char>(addr >> 8));     // 地址高字节
    payload.append(static_cast<char>(addr & 0xFF));   // 地址低字节
    return payload;
}

QByteArray encodeWriteRegister(quint16 addr, qint16 value) {
    QByteArray payload = encodeReadRegister(addr);
    const quint16 rawValue = static_cast<quint16>(value);
    payload.append(static_cast<char>(rawValue >> 8));     // 值高字节
    payload.append(static_cast<char>(rawValue & 0xFF));   // 值低字节
    return payload;
}

/// @brief 编码 I2C 透传写
QByteArray encodeI2cTransferWrite(uint8_t devId,
                                  const uint8_t *addr, uint8_t addrSize,
                                  const uint8_t *data, uint8_t dataLen) {
    QByteArray payload;
    payload.reserve(3 + addrSize + dataLen);
    payload.append(static_cast<char>(devId & 0x7F));   // 7-bit 从机地址（高位强制清零，防呆）
    payload.append(static_cast<char>(addrSize));
    if (addrSize > 0 && addr != nullptr) {
        payload.append(reinterpret_cast<const char *>(addr), addrSize);
    }
    payload.append(static_cast<char>(dataLen));
    if (dataLen > 0 && data != nullptr) {
        payload.append(reinterpret_cast<const char *>(data), dataLen);
    }
    return payload;
}

/// @brief 编码 I2C 透传读
QByteArray encodeI2cTransferRead(uint8_t devId,
                                 const uint8_t *addr, uint8_t addrSize,
                                 uint8_t readLen) {
    QByteArray payload;
    payload.reserve(3 + addrSize);
    payload.append(static_cast<char>(devId & 0x7F));
    payload.append(static_cast<char>(addrSize));
    if (addrSize > 0 && addr != nullptr) {
        payload.append(reinterpret_cast<const char *>(addr), addrSize);
    }
    payload.append(static_cast<char>(readLen));
    return payload;
}

QByteArray encodeI2cBusScan(uint8_t busIndex) {
    QByteArray payload;
    payload.append(static_cast<char>(busIndex));  // I2C 总线索引
    return payload;
}

QByteArray encodeSetMotorIcAddr(uint8_t addr) {
    QByteArray payload;
    payload.append(static_cast<char>(addr));  // 7 位 I2C 从机地址
    return payload;
}

/// @brief 编码 PMIC 电压设置
///
/// 载荷格式（6 字节）：[DRVVDD_H][DRVVDD_L][IOVDD_H][IOVDD_L][VCMVDD_H][VCMVDD_L]
/// 电压值单位为 mV。
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
    return {};  // 无载荷
}

QByteArray encodePmicDisable() {
    return {};  // 无载荷
}

QByteArray encodeReset() {
    return {};  // 无载荷
}

QByteArray encodeMotorTest() {
    return {};  // 无载荷
}

QByteArray encodeStartSampling() {
    return {};  // 无载荷
}

QByteArray encodeStopSampling() {
    return {};  // 无载荷
}

QByteArray encodeSetSampleInterval(uint8_t intervalIndex) {
    if (!isValidSampleIntervalIndex(intervalIndex)) {
        return {};  // 索引无效，返回空
    }

    QByteArray payload;
    payload.append(static_cast<char>(intervalIndex));
    return payload;
}

QByteArray encodeSetSampleChannels(uint8_t channelMask) {
    if (!isValidSampleChannelMask(channelMask)) {
        return {};  // 掩码无效，返回空
    }

    QByteArray payload;
    payload.append(static_cast<char>(channelMask));
    return payload;
}

/// @brief 编码通道-寄存器映射表
///
/// 载荷格式（16 字节）：8 个寄存器地址 × 2 字节大端。
/// 数组索引对应通道号（0=CH1, 7=CH8）。
QByteArray encodeSetChannelRegisterMap(const QVector<quint16> &registers) {
    if (registers.size() != 8) {
        return {};  // 必须恰好 8 个通道
    }

    QByteArray payload;
    payload.reserve(16);
    for (quint16 reg : registers) {
        payload.append(static_cast<char>((reg >> 8) & 0xFF));
        payload.append(static_cast<char>(reg & 0xFF));
    }
    return payload;
}

/// @brief 编码线性发生器启动参数
///
/// 载荷格式（10 字节）：
/// [ADDR_H][ADDR_L][MIN_H][MIN_L][MAX_H][MAX_L][STEP_H][STEP_L][INTERVAL_H][INTERVAL_L]
QByteArray encodeStartLinearGen(quint16 addr, qint16 min, qint16 max, qint16 step, quint16 intervalMs) {
    QByteArray payload;
    payload.reserve(10);

    // 辅助 lambda：追加 16 位大端值
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

/// @brief 编码余弦发生器启动参数
///
/// 载荷格式（可变长度）：
/// [AMP_H][AMP_L][OFF_H][OFF_L][FREQ_H][FREQ_L][CH_COUNT]
/// + 每通道 4 字节 [ADDR_H][ADDR_L][PHASE_H][PHASE_L]
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

    // 追加每个通道的 (寄存器地址, 相位偏移)
    for (const auto &channel : channels) {
        appendU16(channel.first);    // 寄存器地址
        appendS16(channel.second);   // 相位偏移
    }
    return payload;
}

QByteArray encodeStopGenerator() {
    return {};  // 无载荷
}

/// @brief 编码锯齿波测试发生器启动参数
///
/// 载荷格式（8 字节）：
/// [ADDR_H][ADDR_L][MIN_H][MIN_L][MAX_H][MAX_L][STEP_H][STEP_L]
QByteArray encodeStartSawtoothGen(quint16 addr, qint16 min, qint16 max, qint16 step) {
    QByteArray payload;
    payload.reserve(8);

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
    return payload;
}

// -----------------------------------------------------------------------------
// AW 本地 ISP 烧录（0x32~0x37）— 注意所有多字节字段使用小端,与 STM32
// `pFrame->data[0] | data[1]<<8 | ...` 解码方式严格对齐。
// -----------------------------------------------------------------------------

const char *awIspStatusName(uint8_t status) {
    switch (status) {
    case 0: return "ISP_OK";
    case 1: return "ISP_ADDR_ERROR";
    case 2: return "ISP_PBUF_ERROR";
    case 3: return "ISP_HANK_ERROR";
    case 4: return "ISP_JUMP_ERROR";
    case 5: return "ISP_FLASH_ERROR";
    case 6: return "ISP_SPACE_ERROR";
    case 7: return "ISP_NOT_INITED";
    default: return "ISP_UNKNOWN";
    }
}

static void appendU32LE(QByteArray &out, quint32 value) {
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
}

static void appendU16LE(QByteArray &out, quint16 value) {
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

QByteArray encodeFlashBegin(quint32 addr, quint32 totalBytes) {
    QByteArray payload;
    payload.reserve(8);
    appendU32LE(payload, addr);
    appendU32LE(payload, totalBytes);
    return payload;
}

QByteArray encodeFlashData(quint16 pktSeq, const QByteArray &chunk) {
    QByteArray payload;
    payload.reserve(2 + chunk.size());
    appendU16LE(payload, pktSeq);
    payload.append(chunk);
    return payload;
}

bool decodeFlashDataResponse(const QByteArray &data, quint16 *nextSeqOut) {
    if (data.size() < 2 || nextSeqOut == nullptr) {
        return false;
    }
    const quint16 lo = static_cast<quint8>(data.at(0));
    const quint16 hi = static_cast<quint8>(data.at(1));
    *nextSeqOut = static_cast<quint16>(lo | (hi << 8));
    return true;
}

bool decodeFlashExecResponse(const QByteArray &data, uint8_t *ispStatusOut) {
    if (data.size() < 1 || ispStatusOut == nullptr) {
        return false;
    }
    *ispStatusOut = static_cast<uint8_t>(data.at(0));
    return true;
}

bool decodeFlashStatusResponse(const QByteArray &data,
                               uint8_t *stateOut,
                               quint32 *rxOffsetOut,
                               quint32 *totalBytesOut) {
    if (data.size() < 9 || stateOut == nullptr ||
        rxOffsetOut == nullptr || totalBytesOut == nullptr) {
        return false;
    }
    *stateOut = static_cast<uint8_t>(data.at(0));
    auto u32LE = [&](int offset) -> quint32 {
        return  static_cast<quint8>(data.at(offset))
             | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 1))) << 8)
             | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 2))) << 16)
             | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 3))) << 24);
    };
    *rxOffsetOut = u32LE(1);
    *totalBytesOut = u32LE(5);
    return true;
}

bool decodeFlashExecProgress(const QByteArray &data,
                             uint8_t *phaseOut,
                             quint32 *doneOut,
                             quint32 *totalOut) {
    if (data.size() < 9 || phaseOut == nullptr ||
        doneOut == nullptr || totalOut == nullptr) {
        return false;
    }
    *phaseOut = static_cast<uint8_t>(data.at(0));
    auto u32LE = [&](int offset) -> quint32 {
        return  static_cast<quint8>(data.at(offset))
             | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 1))) << 8)
             | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 2))) << 16)
             | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 3))) << 24);
    };
    *doneOut = u32LE(1);
    *totalOut = u32LE(5);
    return true;
}

// -----------------------------------------------------------------------------
// STM32 本地 FLASH 文件存储 — 0x39 ~ 0x3E（协议 v2.10）
// 小端布局；状态码与 STM32 端 FsStatus 严格对齐
// -----------------------------------------------------------------------------

const char *flashStoreStatusName(uint8_t status) {
    switch (static_cast<FlashStoreStatus>(status)) {
    case FlashStoreStatus::Ok:          return "OK";
    case FlashStoreStatus::Empty:       return "EMPTY";
    case FlashStoreStatus::Corrupt:     return "CORRUPT";
    case FlashStoreStatus::WriteFailed: return "WRITE_FAILED";
    case FlashStoreStatus::CrcMismatch: return "CRC_MISMATCH";
    case FlashStoreStatus::Busy:        return "BUSY";
    case FlashStoreStatus::OutOfRange:  return "OUT_OF_RANGE";
    case FlashStoreStatus::SeqError:    return "SEQ_ERROR";
    }
    return "UNKNOWN";
}

// appendU32LE / appendU16LE 已在本文件 line 318/325 定义（file-static）, 直接复用。
// 这里只补两个 read helper, 命名加 fs 前缀避免与未来扩展冲突。
namespace {
inline quint32 fsReadU32LE(const QByteArray &data, int offset) {
    return  static_cast<quint8>(data.at(offset))
         | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 1))) << 8)
         | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 2))) << 16)
         | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 3))) << 24);
}
inline quint16 fsReadU16LE(const QByteArray &data, int offset) {
    return static_cast<quint16>(static_cast<quint8>(data.at(offset)))
         | (static_cast<quint16>(static_cast<quint8>(data.at(offset + 1))) << 8);
}
}  // namespace

QByteArray encodeFlashStoreWriteBegin(quint32 totalBytes) {
    QByteArray payload;
    payload.reserve(4);
    appendU32LE(payload, totalBytes);
    return payload;
}

QByteArray encodeFlashStoreWriteData(quint16 pktSeq, const QByteArray &chunk) {
    QByteArray payload;
    payload.reserve(2 + chunk.size());
    appendU16LE(payload, pktSeq);
    payload.append(chunk);
    return payload;
}

QByteArray encodeFlashStoreWriteEnd(quint32 expectedCrc32) {
    QByteArray payload;
    payload.reserve(4);
    appendU32LE(payload, expectedCrc32);
    return payload;
}

QByteArray encodeFlashStoreReadData(quint16 pktSeq) {
    QByteArray payload;
    payload.reserve(2);
    appendU16LE(payload, pktSeq);
    return payload;
}

bool decodeFlashStoreWriteDataResponse(const QByteArray &data, quint16 *nextSeqOut) {
    if (data.size() < 2 || nextSeqOut == nullptr) {
        return false;
    }
    *nextSeqOut = fsReadU16LE(data, 0);
    return true;
}

bool decodeFlashStoreSimpleStatus(const QByteArray &data, uint8_t *statusOut) {
    if (data.size() < 1 || statusOut == nullptr) {
        return false;
    }
    *statusOut = static_cast<uint8_t>(data.at(0));
    return true;
}

bool decodeFlashStoreReadBeginResponse(const QByteArray &data,
                                       uint8_t *statusOut,
                                       quint32 *sizeOut,
                                       quint32 *crc32Out) {
    if (data.size() < 9 || statusOut == nullptr ||
        sizeOut == nullptr || crc32Out == nullptr) {
        return false;
    }
    *statusOut = static_cast<uint8_t>(data.at(0));
    *sizeOut   = fsReadU32LE(data, 1);
    *crc32Out  = fsReadU32LE(data, 5);
    return true;
}

bool decodeFlashStoreInfoResponse(const QByteArray &data,
                                  quint32 *totalCapacityOut,
                                  quint32 *usedSizeOut) {
    if (data.size() < 8 || totalCapacityOut == nullptr || usedSizeOut == nullptr) {
        return false;
    }
    *totalCapacityOut = fsReadU32LE(data, 0);
    *usedSizeOut      = fsReadU32LE(data, 4);
    return true;
}

// -----------------------------------------------------------------------------
// 响应载荷解码
// -----------------------------------------------------------------------------

/// @brief 解码读寄存器响应
///
/// 响应载荷格式（2 字节）：[VALUE_H][VALUE_L]，大端有符号 16 位。
bool decodeReadRegisterResponse(const QByteArray &data, qint16 *valueOut) {
    if (data.size() < 2 || valueOut == nullptr) {
        return false;
    }

    const quint16 rawValue = (static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1));
    *valueOut = static_cast<qint16>(rawValue);
    return true;
}

/// @brief 解码 I2C 扫描响应
///
/// 响应载荷格式：[COUNT][ADDR_1][ADDR_2]...[ADDR_N]
/// 第 1 字节为扫描到的设备数量，后续为 7 位 I2C 地址列表。
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

// -----------------------------------------------------------------------------
// 0x0B BOOT_STATUS
// -----------------------------------------------------------------------------

const char *bootStatusName(uint8_t status) {
    switch (status) {
    case static_cast<uint8_t>(BootStatusCode::Ok):            return "BOOT_OK";
    case static_cast<uint8_t>(BootStatusCode::InitFailI2c1):  return "INIT_FAIL_I2C1";
    case static_cast<uint8_t>(BootStatusCode::InitFailI2c2):  return "INIT_FAIL_I2C2";
    case static_cast<uint8_t>(BootStatusCode::InitFailI2c3):  return "INIT_FAIL_I2C3";
    case static_cast<uint8_t>(BootStatusCode::InitFailPmic):  return "INIT_FAIL_PMIC";
    case static_cast<uint8_t>(BootStatusCode::InitFailAwIsp): return "INIT_FAIL_AWISP";
    default:                                                  return "RESERVED";
    }
}

const char *bootStatusDescription(uint8_t status) {
    switch (status) {
    case static_cast<uint8_t>(BootStatusCode::Ok):
        return "STM32 就绪：全部模块初始化完成";
    case static_cast<uint8_t>(BootStatusCode::InitFailI2c1):
        return "I2C1（INA 电流测量）初始化失败：请检查 PB6/PB7 上拉";
    case static_cast<uint8_t>(BootStatusCode::InitFailI2c2):
        return "I2C2（电机 IC 总线）初始化失败：请检查 PB10/PB11";
    case static_cast<uint8_t>(BootStatusCode::InitFailI2c3):
        return "I2C3（PMIC 总线）初始化失败：请检查 PA8/PC9";
    case static_cast<uint8_t>(BootStatusCode::InitFailPmic):
        return "PMIC RT5112WSC 初始化失败：请检查电源";
    case static_cast<uint8_t>(BootStatusCode::InitFailAwIsp):
        return "AW ISP 回调注册失败：固件 bug，请联系开发";
    default:
        return "未知启动状态码（保留段）";
    }
}

bool decodeBootStatusResponse(const QByteArray &data, uint8_t *statusOut) {
    if (data.size() < 1 || statusOut == nullptr) {
        return false;
    }
    *statusOut = static_cast<uint8_t>(data.at(0));
    return true;
}

} // namespace MotorProtocol
