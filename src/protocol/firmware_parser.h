// =============================================================================
// @file    firmware_parser.h
// @brief   固件文件解析 — 支持 .bin 与 Intel .hex 两种格式
//
// 解析后的 FirmwareInfo 同时承载：合并后的连续二进制（.hex 段间用 0xFF 填充）、
// CRC32、文件元信息、Intel HEX 段列表与地址范围。FwFlashService 与
// FwFileInfoPanel 共用本结构，分别取 data 与显示字段。
//
// 上限 1024 KB（含 .hex 合并后大小），超过即视为非法。
// =============================================================================
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include <cstdint>

/// @brief 固件文件格式
enum class FirmwareFormat {
    Unknown,
    Bin,
    Hex,        ///< Intel HEX（每行 `:` 开头 + 校验和）
    Hl9788Hex,  ///< Dongwoon HL9788N 自定义 hex：每行 1 个 32-bit 十六进制字（无 `:` 前缀），
                ///< 共 16384 行 = 32768 words = 64KB。解析后 data 为 65536 字节小端二进制，
                ///< 可直接 reinterpret_cast<const uint16_t*> 传给 vendor SDK。
};

/// @brief Intel HEX 中一段连续地址区间
struct FirmwareSegment {
    quint32 startAddress = 0;   ///< 段起始物理地址（包含 04 记录的高 16 位）
    quint32 length = 0;         ///< 段长度（字节）
};

/// @brief 解析结果
///
/// `valid == true` 时其余字段方有意义；否则只看 `errorMessage`。
struct FirmwareInfo {
    bool valid = false;
    QString errorMessage;       ///< 解析失败原因（valid=false 时填充）

    QString fileName;           ///< 不含路径的文件名
    qint64 fileSizeBytes = 0;   ///< 原始文件大小
    FirmwareFormat format = FirmwareFormat::Unknown;
    QByteArray data;            ///< 用于烧录的连续二进制（.bin = 原文件；.hex = 合并填充后）
    quint32 crc32 = 0;          ///< 基于 data 计算的标准 CRC32
    qint64 effectiveBytes = 0;  ///< 有效字节数（.bin 等于 fileSizeBytes；.hex 为所有 data record 字节总和）

    QVector<FirmwareSegment> segments;  ///< 仅 .hex 有意义
    quint32 minAddress = 0;             ///< 仅 .hex 有意义
    quint32 maxAddress = 0;             ///< 仅 .hex 有意义
};

/// @brief 固件文件解析器（静态工具类）
class FirmwareParser {
public:
    /// @brief 文件大小上限：1024 KB
    static constexpr qint64 MaxFileBytes = 1024LL * 1024LL;

    /// @brief 解析指定路径的固件文件
    /// @param path 完整文件路径
    /// @return 解析结果；失败时 valid=false 且 errorMessage 描述原因
    static FirmwareInfo parseFile(const QString &path);

    /// @brief 计算标准 CRC32（多项式 0xEDB88320，IEEE 802.3）
    static quint32 computeCrc32(const QByteArray &data);
};
