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
