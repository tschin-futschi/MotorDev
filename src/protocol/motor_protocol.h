// =============================================================================
// @file    motor_protocol.h
// @brief   电机驱动协议编解码（协议层）
//
// 定义所有通信命令字节常量，以及请求载荷的编码函数和响应载荷的解码函数。
// 本文件是 protocol.md 协议规范在代码中的映射实现。
//
// 命令字节分段：
// - 0x00~0x0F：系统/设备管理命令（心跳、错误、IC 地址、波特率、PMIC、BootStatus 等）
// - 0x20~0x2F：寄存器读写命令
// - 0x30~0x31：通用 I2C 透传（写/读，DW vendor 桥接复用）
// - 0x32~0x38：AW 本地 ISP 烧录命令 + 0x38 EXEC 进度事件
// - 0x39~0x3F：STM32 FLASH 文件存储（上传/下载/容量/WIPE）
// - 0x50~0x5F：示波器采样与信号发生器命令
// =============================================================================

#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
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
inline constexpr uint8_t CmdBootStatus = 0x0B;          ///< 启动状态报告（STM32→PC 主动发送，SEQ=0xFF，LEN=1）

// --- 寄存器读写 ---
inline constexpr uint8_t CmdReadRegister = 0x20;        ///< 读寄存器（载荷: 2 字节地址）
inline constexpr uint8_t CmdWriteRegister = 0x21;       ///< 写寄存器（载荷: 2 字节地址 + 2 字节值）
inline constexpr uint8_t CmdBulkRead = 0x22;            ///< 批量读寄存器
inline constexpr uint8_t CmdI2cTransferWrite = 0x30;    ///< I2C 透传写（任意 7-bit 从机 + 任意寄存器地址长度 + 任意数据长度）
inline constexpr uint8_t CmdI2cTransferRead = 0x31;     ///< I2C 透传读

// --- AW 本地 ISP 烧录（0x32~0x38，协议 v2.9）---
inline constexpr uint8_t CmdFlashBegin = 0x32;          ///< 启动烧录 session（载荷: [addr LE 4B][totalBytes LE 4B]）
inline constexpr uint8_t CmdFlashData = 0x33;           ///< 分包下发固件（载荷: [pktSeq LE 2B][chunk]；响应: [nextSeq LE 2B]）
inline constexpr uint8_t CmdFlashExec = 0x34;           ///< 触发实际烧录（阻塞 5-10s；响应: [ispStatus 1B]）
inline constexpr uint8_t CmdFlashStatus = 0x35;         ///< 查询 session 状态（响应: [state 1B][rxOffset LE 4B][totalBytes LE 4B]）
inline constexpr uint8_t CmdFlashCancel = 0x36;         ///< 重置 session 到 IDLE
inline constexpr uint8_t CmdFlashResetChip = 0x37;      ///< 单独调 aw_reset_chip()（响应: [ispStatus 1B]）
inline constexpr uint8_t CmdFlashExecProgress = 0x38;   ///< STM32 主动上报 EXEC 进度（SEQ=0xFF，载荷: [phase 1B][done LE 4B][total LE 4B]）

// --- STM32 本地 FLASH 文件存储（0x39~0x3E，协议 v2.10）---
// 写 STM32 自身 Flash Sector 5-11（[0x08020000, 0x08100000) = 896KB）暂存任意文件。
// 单插槽覆盖：每次 WRITE_BEGIN 整区擦除。下载字节与原始上传文件 1:1 无损（改扩展名即可复原）。
inline constexpr uint8_t CmdFlashStoreWriteBegin = 0x39; ///< 开始写入 session（载荷: [totalBytes LE 4B]；响应: [status 1B]；阻塞 3-7s 整区擦）
inline constexpr uint8_t CmdFlashStoreWriteData  = 0x3A; ///< 写一包（载荷: [pktSeq LE 2B][chunk]；响应: [nextSeq LE 2B] 或错误响应）
inline constexpr uint8_t CmdFlashStoreWriteEnd   = 0x3B; ///< 提交并校验 CRC（载荷: [expectedCrc32 LE 4B]；响应: [status 1B]）
inline constexpr uint8_t CmdFlashStoreReadBegin  = 0x3C; ///< 读元数据（无载荷；响应: [status 1B][size LE 4B][crc32 LE 4B]）
inline constexpr uint8_t CmdFlashStoreReadData   = 0x3D; ///< 读一包（载荷: [pktSeq LE 2B]；响应: [chunk N≤252B] 或错误响应）
inline constexpr uint8_t CmdFlashStoreInfo       = 0x3E; ///< 查询容量（无载荷；响应: [totalCapacity LE 4B][usedSize LE 4B]）
inline constexpr uint8_t CmdFlashStoreWipe       = 0x3F; ///< 整区擦除（无载荷；响应: [status LE 1B]；阻塞 3-7s，消除存储痕迹）

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
// 启动状态码（protocol.md §4.2 / 0x0B BOOT_STATUS）
// ---------------------------------------------------------------------------

/// @brief BSP/App 初始化阶段结果，由 STM32 通过 0x0B 帧主动上报
enum class BootStatusCode : uint8_t {
    Ok = 0x00,             ///< BOOT_OK：全部模块初始化完成，主循环即将启动
    InitFailI2c1 = 0x01,   ///< I2C1（INA 电流测量）初始化失败
    InitFailI2c2 = 0x02,   ///< I2C2（电机 IC 软件 bit-bang）初始化失败
    InitFailI2c3 = 0x03,   ///< I2C3（PMIC）初始化失败
    InitFailPmic = 0x04,   ///< PMIC RT5112WSC 电压预设失败（I2C 通信失败）
    InitFailAwIsp = 0x05,  ///< AW ISP 回调表注册失败（ops 表 NULL 成员 = 固件 bug）
};

/// @brief 启动状态码的可读名称（含 0x06~0xFF 保留段）
/// @param status  启动状态码
/// @return 名称 C 字符串（"BOOT_OK" / "INIT_FAIL_*" / "RESERVED"）
const char *bootStatusName(uint8_t status);

/// @brief 启动状态码的中文描述，供 UI/日志展示
/// @param status  启动状态码
/// @return 用户可读的中文描述字符串
const char *bootStatusDescription(uint8_t status);

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
/// @param drvvdd  DRVVDD 电压值（实际电压 × 100，如 1.80V→180）
/// @param iovdd   IOVDD 电压值（实际电压 × 100）
/// @param vcmvdd  VCMVDD 电压值（实际电压 × 100）
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

/// @brief EXEC 进度事件帧的阶段标识（与 STM32 端 AW_ISP_PROGRESS_PHASE_* 一致）
enum class FlashExecPhase : uint8_t {
    Erase = 0,  ///< 擦除阶段（aw_flash_block_erase_check 前/后各一次）
    Write = 1,  ///< 写入阶段（write 块循环每次成功后一次）
};

/// @brief 解码 0x38 FLASH_EXEC_PROGRESS 事件帧（9 字节: phase + done + total,均小端）
/// @return true=解析成功;失败时 out 参数不变
bool decodeFlashExecProgress(const QByteArray &data,
                             uint8_t *phaseOut,
                             quint32 *doneOut,
                             quint32 *totalOut);

// ---------------------------------------------------------------------------
// STM32 本地 FLASH 文件存储（0x39~0x3E，协议 v2.10）
//
// 状态码 FlashStoreStatus 与 STM32 端 App/Inc/app_flashstore.h 的 FsStatus
// 枚举严格对齐——任一侧改动必须同步另一侧（值、名称、语义）。
// ---------------------------------------------------------------------------

/// @brief Flash 文件存储操作状态（用于 0x39 / 0x3B / 0x3C 响应的 status 字段）
enum class FlashStoreStatus : uint8_t {
    Ok           = 0x00,  ///< 成功
    Empty        = 0x01,  ///< 元数据 magic = 0xFFFFFFFF（slot 为空，正常状态）
    Corrupt      = 0x02,  ///< 元数据 magic 非法或 size 越界
    WriteFailed  = 0x03,  ///< Flash erase / program 硬件失败
    CrcMismatch  = 0x04,  ///< WRITE_END：PC 给的 CRC 与 STM32 累计 CRC 不一致 → 元数据未写
    Busy         = 0x05,  ///< 保留
    OutOfRange   = 0x06,  ///< totalBytes / pktSeq / len 越界
    SeqError     = 0x07,  ///< pktSeq 不连续 / 无 active session / WriteEnd 时 offset != totalBytes
};

/// @brief Flash 文件存储状态的可读名称
/// @param status  状态码原值
/// @return 名称 C 字符串（"OK" / "EMPTY" / ... / "UNKNOWN"）
const char *flashStoreStatusName(uint8_t status);

/// @brief 编码 0x39 WRITE_BEGIN 命令载荷（4 字节小端）
/// @param totalBytes  即将写入的总字节数；调用方保证 1 ≤ totalBytes ≤ 917488
QByteArray encodeFlashStoreWriteBegin(quint32 totalBytes);

/// @brief 编码 0x3A WRITE_DATA 命令载荷
/// @param pktSeq  当前包序号（从 0 严格递增，2 字节小端）
/// @param chunk   本包数据（≤ 252 字节，与 0x33 一致）
QByteArray encodeFlashStoreWriteData(quint16 pktSeq, const QByteArray &chunk);

/// @brief 编码 0x3B WRITE_END 命令载荷（4 字节小端）
/// @param expectedCrc32  PC 端预先对原始文件算出的 CRC32（IEEE 802.3）
QByteArray encodeFlashStoreWriteEnd(quint32 expectedCrc32);

/// @brief 编码 0x3D READ_DATA 命令载荷（2 字节小端）
/// @param pktSeq  请求第 pktSeq 包（0-based，offset = pktSeq * 252）
QByteArray encodeFlashStoreReadData(quint16 pktSeq);

/// @brief 解码 0x3A WRITE_DATA 响应（2 字节小端，STM32 期望的下一个 pktSeq）
/// @return true=解析成功；失败时 nextSeqOut 不变
bool decodeFlashStoreWriteDataResponse(const QByteArray &data, quint16 *nextSeqOut);

/// @brief 解码 0x39 WRITE_BEGIN / 0x3B WRITE_END 响应（1 字节 status）
/// @return true=解析成功；失败时 statusOut 不变
bool decodeFlashStoreSimpleStatus(const QByteArray &data, uint8_t *statusOut);

/// @brief 解码 0x3C READ_BEGIN 响应（9 字节: status + size + crc32，均小端）
/// @return true=解析成功；失败时 out 参数不变
bool decodeFlashStoreReadBeginResponse(const QByteArray &data,
                                       uint8_t *statusOut,
                                       quint32 *sizeOut,
                                       quint32 *crc32Out);

/// @brief 解码 0x3E INFO 响应（8 字节: totalCapacity + usedSize，均小端）
/// @return true=解析成功；失败时 out 参数不变
bool decodeFlashStoreInfoResponse(const QByteArray &data,
                                  quint32 *totalCapacityOut,
                                  quint32 *usedSizeOut);

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

/// @brief 判断 0x06 调试信息是否为"采样时间不够"（tick overrun）
///
/// STM32 在采样/发生器启动耗时超限（预期单 tick I2C 总耗时 > tick 周期 × 80%）时，
/// 主动发 0x06 文本 "Tick overrun: estimated Xus > limit Yus"
/// （见 protocol.md §0x06 合法场景 / §0x50 耗时校验）。本函数集中持有该标记，
/// 避免标记串散落到业务/UI 层。
/// @param debugText    0x06 调试帧文本（UTF-8 解码后的字符串）
/// @param estimatedUs  输出：预估耗时(us)，未解析到时置 -1（可为 nullptr）
/// @param limitUs      输出：限值(us)，未解析到时置 -1（可为 nullptr）
/// @return 文本含 tick overrun 标记返回 true，否则 false
bool parseTickOverrunDebug(const QString &debugText, int *estimatedUs, int *limitUs);

/// @brief 解码 0x0B BOOT_STATUS 帧载荷
/// @param data       载荷字节流（应为 1 字节状态码）
/// @param statusOut  输出：启动状态码
/// @return 解码成功返回 true；data 长度不足或 statusOut 为 nullptr 返回 false
bool decodeBootStatusResponse(const QByteArray &data, uint8_t *statusOut);

/// @brief 验证采样间隔索引是否合法（0~6）
bool isValidSampleIntervalIndex(uint8_t intervalIndex);

/// @brief 验证采样通道掩码是否合法（非零）
bool isValidSampleChannelMask(uint8_t channelMask);

} // namespace MotorProtocol
