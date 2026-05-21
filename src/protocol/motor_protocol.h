// =============================================================================
// @file    motor_protocol.h
// @brief   电机驱动协议编解码（协议层）
//
// 定义所有通信命令字节常量，以及请求载荷的编码函数和响应载荷的解码函数。
// 本文件是 protocol.md 协议规范在代码中的映射实现。
//
// 命令字节分段：
// - 0x00~0x0F：系统/设备管理命令（心跳、错误、IC 地址、波特率、PMIC 等）
// - 0x20~0x2F：寄存器读写命令
// - 0x50~0x5F：示波器采样与信号发生器命令
// =============================================================================

#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QVector>

#include <cstdint>

namespace MotorProtocol {

// ---------------------------------------------------------------------------
// 命令字节常量
// ---------------------------------------------------------------------------

// --- 系统/设备管理 ---
inline constexpr uint8_t CmdHeartbeat = 0x00;           ///< 心跳（无载荷）
inline constexpr uint8_t CmdErrorResponse = 0x01;       ///< 错误响应（载荷: 1 字节错误码）
inline constexpr uint8_t CmdSetMotorIcAddr = 0x02;      ///< 设置 IC I2C 从机地址
inline constexpr uint8_t CmdSetBaudrate = 0x03;         ///< 设置串口波特率
inline constexpr uint8_t CmdReset = 0x04;               ///< 复位下位机
inline constexpr uint8_t CmdMotorTest = 0x05;           ///< 电机测试
inline constexpr uint8_t CmdDebugInfo = 0x06;           ///< 设备主动上报的调试信息
inline constexpr uint8_t CmdI2cBusScan = 0x07;          ///< I2C 总线扫描
inline constexpr uint8_t CmdPmicEnable = 0x08;          ///< PMIC 使能
inline constexpr uint8_t CmdSetPmicVoltage = 0x09;      ///< 设置 PMIC 电压
inline constexpr uint8_t CmdPmicDisable = 0x0A;         ///< PMIC 禁用

// --- 寄存器读写 ---
inline constexpr uint8_t CmdReadRegister = 0x20;        ///< 读寄存器（载荷: 2 字节地址）
inline constexpr uint8_t CmdWriteRegister = 0x21;       ///< 写寄存器（载荷: 2 字节地址 + 2 字节值）
inline constexpr uint8_t CmdBulkRead = 0x22;            ///< 批量读寄存器
inline constexpr uint8_t CmdI2cTransferWrite = 0x30;    ///< I2C 透传写（任意 7-bit 从机 + 任意寄存器地址长度 + 任意数据长度）
inline constexpr uint8_t CmdI2cTransferRead = 0x31;     ///< I2C 透传读

// --- AW 本地 ISP 烧录（0x32~0x37，协议 v2.7）---
inline constexpr uint8_t CmdFlashBegin = 0x32;      ///< 启动烧录 session（载荷: [addr LE 4B][totalBytes LE 4B]）
inline constexpr uint8_t CmdFlashData = 0x33;       ///< 分包下发固件（载荷: [pktSeq LE 2B][chunk]；响应: [nextSeq LE 2B]）
inline constexpr uint8_t CmdFlashExec = 0x34;       ///< 触发实际烧录（阻塞 5-10s；响应: [ispStatus 1B]）
inline constexpr uint8_t CmdFlashStatus = 0x35;     ///< 查询 session 状态（响应: [state 1B][rxOffset LE 4B][totalBytes LE 4B]）
inline constexpr uint8_t CmdFlashCancel = 0x36;     ///< 重置 session 到 IDLE
inline constexpr uint8_t CmdFlashResetChip = 0x37;  ///< 单独调 aw_reset_chip()（响应: [ispStatus 1B]）

// --- 示波器采样 ---
inline constexpr uint8_t CmdStartSampling = 0x50;       ///< 启动采样
inline constexpr uint8_t CmdStopSampling = 0x51;        ///< 停止采样
inline constexpr uint8_t CmdSetSampleInterval = 0x52;   ///< 设置采样间隔
inline constexpr uint8_t CmdSetSampleChannels = 0x53;   ///< 设置采样通道掩码
inline constexpr uint8_t CmdSetChannelRegisterMap = 0x54; ///< 设置通道-寄存器映射表

// --- 信号发生器 ---
inline constexpr uint8_t CmdStartLinearGen = 0x55;      ///< 启动线性发生器
inline constexpr uint8_t CmdStartCosineGen = 0x56;      ///< 启动余弦发生器
inline constexpr uint8_t CmdStopGenerator = 0x57;       ///< 停止发生器
inline constexpr uint8_t CmdStartSawtoothGen = 0x58;    ///< 启动锯齿波测试发生器

// ---------------------------------------------------------------------------
// 错误码（protocol.md §4.5）
// ---------------------------------------------------------------------------

namespace ErrorCode {
inline constexpr uint8_t CrcFailed = 0x01;        ///< 收到的控制帧 CRC 校验不通过
inline constexpr uint8_t UnknownCmd = 0x02;       ///< 命令码未定义或不支持
inline constexpr uint8_t ExecutionFailed = 0x03;  ///< 命令执行过程中发生错误（如 I2C 通讯失败）
} // namespace ErrorCode

// ---------------------------------------------------------------------------
// 命令名称查询
// ---------------------------------------------------------------------------

/// @brief 将命令字节转为可读名称（用于日志输出）
/// @param cmd  命令字节
/// @return 命令名称 C 字符串
const char *commandName(uint8_t cmd);

// ---------------------------------------------------------------------------
// 请求载荷编码
// ---------------------------------------------------------------------------

/// @brief 编码读寄存器请求载荷
/// @param addr  16 位寄存器地址
/// @return 2 字节载荷（大端）
QByteArray encodeReadRegister(quint16 addr);

/// @brief 编码写寄存器请求载荷
/// @param addr   16 位寄存器地址
/// @param value  16 位寄存器值（有符号）
/// @return 4 字节载荷（地址 + 值，大端）
QByteArray encodeWriteRegister(quint16 addr, qint16 value);

/// @brief 编码 I2C 透传写请求载荷（CmdI2cTransferWrite, 0x30）
///
/// 载荷格式：[DevId(7-bit)] [AddrSize] [AddrBytes(AddrSize 字节)] [DataLen] [Data(DataLen 字节)]
/// AddrSize == 0 时跳过寄存器地址段（直接对从机写一段字节流）。
/// @param devId      7-bit I2C 从机地址（0x00–0x7F）
/// @param addr       寄存器地址字节流；可为 nullptr 当 addrSize == 0
/// @param addrSize   寄存器地址字节数（0 表示无寄存器地址）
/// @param data       数据字节流；可为 nullptr 当 dataLen == 0
/// @param dataLen    数据字节数
/// @return 完整载荷字节流
QByteArray encodeI2cTransferWrite(uint8_t devId,
                                  const uint8_t *addr, uint8_t addrSize,
                                  const uint8_t *data, uint8_t dataLen);

/// @brief 编码 I2C 透传读请求载荷（CmdI2cTransferRead, 0x31）
///
/// 载荷格式：[DevId(7-bit)] [AddrSize] [AddrBytes(AddrSize 字节)] [ReadLen]
/// AddrSize == 0 时跳过寄存器地址段（直接从从机读一段字节流）。
/// 成功响应数据段即为读到的 readLen 字节内容。
/// @param devId      7-bit I2C 从机地址（0x00–0x7F）
/// @param addr       寄存器地址字节流；可为 nullptr 当 addrSize == 0
/// @param addrSize   寄存器地址字节数（0 表示无寄存器地址）
/// @param readLen    要读的字节数
/// @return 完整载荷字节流
QByteArray encodeI2cTransferRead(uint8_t devId,
                                 const uint8_t *addr, uint8_t addrSize,
                                 uint8_t readLen);

/// @brief 编码 I2C 总线扫描请求载荷
/// @param busIndex  总线索引（默认 0x02）
/// @return 1 字节载荷
QByteArray encodeI2cBusScan(uint8_t busIndex = 0x02);

/// @brief 编码设置 IC 从机地址请求载荷
/// @param addr  I2C 从机地址（7 位）
/// @return 1 字节载荷
QByteArray encodeSetMotorIcAddr(uint8_t addr);

/// @brief 编码 PMIC 电压设置请求载荷
/// @param drvvdd  DRVVDD 电压值（mV）
/// @param iovdd   IOVDD 电压值（mV）
/// @param vcmvdd  VCMVDD 电压值（mV）
/// @return 6 字节载荷（3 × 2 字节大端）
QByteArray encodePmicVoltage(quint16 drvvdd, quint16 iovdd, quint16 vcmvdd);

/// @brief 编码 PMIC 使能请求（无载荷）
QByteArray encodePmicEnable();

/// @brief 编码 PMIC 禁用请求（无载荷）
QByteArray encodePmicDisable();

/// @brief 编码复位请求（无载荷）
QByteArray encodeReset();

/// @brief 编码电机测试请求（无载荷）
QByteArray encodeMotorTest();

/// @brief 编码启动采样请求（无载荷）
QByteArray encodeStartSampling();

/// @brief 编码停止采样请求（无载荷）
QByteArray encodeStopSampling();

/// @brief 编码设置采样间隔请求
/// @param intervalIndex  采样间隔索引（0~6）
/// @return 1 字节载荷；索引无效时返回空
QByteArray encodeSetSampleInterval(uint8_t intervalIndex);

/// @brief 编码设置采样通道请求
/// @param channelMask  通道掩码（bit0~bit7 对应 CH1~CH8）
/// @return 1 字节载荷；掩码为 0 时返回空
QByteArray encodeSetSampleChannels(uint8_t channelMask);

/// @brief 编码设置通道-寄存器映射表请求
/// @param registers  8 个寄存器地址（必须恰好 8 个）
/// @return 16 字节载荷（8 × 2 字节大端）；长度不对时返回空
QByteArray encodeSetChannelRegisterMap(const QVector<quint16> &registers);

/// @brief 编码启动线性发生器请求
/// @param addr        目标寄存器地址
/// @param min         最小值
/// @param max         最大值
/// @param step        步进值
/// @param intervalMs  步进间隔（毫秒）
/// @return 10 字节载荷
QByteArray encodeStartLinearGen(quint16 addr, qint16 min, qint16 max, qint16 step, quint16 intervalMs);

/// @brief 编码启动余弦发生器请求
/// @param amplitude    振幅
/// @param offset       偏移量
/// @param freqX100     频率 × 100（如 100 表示 1.00 Hz）
/// @param channelCount 通道数量
/// @param channels     通道列表：每项为 (寄存器地址, 相位偏移)
/// @return 可变长度载荷
QByteArray encodeStartCosineGen(qint16 amplitude, qint16 offset, quint16 freqX100, uint8_t channelCount,
                                const QVector<QPair<quint16, qint16>> &channels);

/// @brief 编码停止发生器请求（无载荷）
QByteArray encodeStopGenerator();

/// @brief 编码启动锯齿波测试发生器请求
/// @param addr  目标寄存器地址
/// @param min   最小值
/// @param max   最大值
/// @param step  步进值
/// @return 8 字节载荷
QByteArray encodeStartSawtoothGen(quint16 addr, qint16 min, qint16 max, qint16 step);

// ---------------------------------------------------------------------------
// AW 本地 ISP 烧录（0x32~0x37）
// ---------------------------------------------------------------------------

/// @brief ISP 状态码（与 STM32 `enum isp_status` 一一对应）
enum class AwIspStatus : uint8_t {
    Ok = 0,            ///< 全成功
    AddrError = 1,     ///< 烧录起始地址不在 [0x01000000, 0x01020000)
    PbufError = 2,     ///< 缓冲指针非法
    HankError = 3,     ///< handshake 失败（IC 未供电 / I2C 无 ACK）
    JumpError = 4,     ///< 跳转回主程序失败
    FlashError = 5,    ///< Flash 写入 / 校验失败
    SpaceError = 6,    ///< 目标地址 + len 超出 FLASH_TOP
    NotInited = 7,     ///< ISP 驱动未注册回调
};

/// @brief 把 AwIspStatus 转为可读字符串（含数值,便于日志定位）
const char *awIspStatusName(uint8_t status);

/// @brief STATUS 命令响应中的 session 状态码
enum class FlashSessionState : uint8_t {
    Idle = 0,
    Receiving = 1,
    Ready = 2,
    Executing = 3,  ///< EXEC 阻塞期间,实际几乎观察不到
    Done = 4,
    Error = 5,
};

/// @brief 编码 BEGIN 命令载荷（8 字节,小端,与 STM32 `pFrame->data[0..7]` 对齐）
/// @param addr        目标 Flash 起始地址（必须 ∈ [0x01000000, 0x01020000)）
/// @param totalBytes  固件总字节数（必须 4 字节对齐,≤ 64 KB）
QByteArray encodeFlashBegin(quint32 addr, quint32 totalBytes);

/// @brief 编码 DATA 命令载荷
/// @param pktSeq  当前包序号（从 0 严格递增,小端 2 字节）
/// @param chunk   本包数据（≤ 253 字节）
QByteArray encodeFlashData(quint16 pktSeq, const QByteArray &chunk);

/// @brief 解码 DATA 响应（2 字节小端,STM32 期望的下一个 pktSeq）
/// @return true=解析成功;失败时 nextSeqOut 不变
bool decodeFlashDataResponse(const QByteArray &data, quint16 *nextSeqOut);

/// @brief 解码 EXEC / RESET_CHIP 响应（1 字节,值 = AwIspStatus）
/// @return true=解析成功
bool decodeFlashExecResponse(const QByteArray &data, uint8_t *ispStatusOut);

/// @brief 解码 STATUS 响应（9 字节: state + rxOffset + totalBytes,均小端）
/// @return true=解析成功;失败时 out 参数不变
bool decodeFlashStatusResponse(const QByteArray &data,
                               uint8_t *stateOut,
                               quint32 *rxOffsetOut,
                               quint32 *totalBytesOut);

// ---------------------------------------------------------------------------
// 响应载荷解码
// ---------------------------------------------------------------------------

/// @brief 解码读寄存器响应
/// @param data      响应载荷
/// @param valueOut  输出：寄存器值
/// @return 解码是否成功
bool decodeReadRegisterResponse(const QByteArray &data, qint16 *valueOut);

/// @brief 解码 I2C 总线扫描响应
/// @param data  响应载荷（第 1 字节为地址数量，后续为地址列表）
/// @return 扫描到的 I2C 从机地址列表
QList<uint8_t> decodeI2cScanResponse(const QByteArray &data);

/// @brief 解码错误响应中的错误码
/// @param data  错误响应载荷
/// @return 错误码（载荷为空时返回 0）
uint8_t decodeErrorCode(const QByteArray &data);

/// @brief 验证采样间隔索引是否合法（0~6）
bool isValidSampleIntervalIndex(uint8_t intervalIndex);

/// @brief 验证采样通道掩码是否合法（非零）
bool isValidSampleChannelMask(uint8_t channelMask);

} // namespace MotorProtocol
