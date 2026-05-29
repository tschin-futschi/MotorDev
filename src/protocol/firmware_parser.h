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
                ///< 拆字规则：out[2i]=val&0xFFFF; out[2i+1]=(val>>16)&0xFFFF。
    Dw9786Hex,  ///< Dongwoon DW9786 自定义 hex：每行 1 个 32-bit 十六进制字（无 `:` 前缀），
                ///< 共 10240 行 = 20480 words = 40KB。解析后 data 为 40960 字节小端二进制。
                ///< 拆字规则与 HL9788N **相反**：out[2i]=(val>>16)&0xFFFF; out[2i+1]=val&0xFFFF。
};

/// @brief IC 提示：FwFlashTab 已知 IC 时把 hint 传给 parser，便于区分
/// 行内格式相同（每行 8 hex）但拆字规则不同的两种 DW vendor hex 格式。
enum class IcHint {
    Auto,    ///< 无 hint，parser 用文件大小启发兜底
    Hl9788,  ///< 强制走 Hl9788Hex 解析
    Dw9786,  ///< 强制走 Dw9786Hex 解析
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

    QVector<FirmwareSegment> segments;  ///< 仅 Intel .hex 有意义
    quint32 minAddress = 0;             ///< 仅 Intel .hex 有意义
    quint32 maxAddress = 0;             ///< 仅 Intel .hex 有意义

    // --- 仅 Hl9788Hex 路径有意义；其他格式恒为默认值 ---
    bool    paddingApplied = false;     ///< 原始 hex 行数 < 16384 时触发了自动补齐
    int     originalLines = 0;          ///< 原始 hex 有效（非空）数据行数 N
    quint32 footerCrc32 = 0;            ///< 写入 footer 倒数第二行的 CRC32 值
                                         ///< = CRC32(原始 hex 文件全部字节，含 \r\n / 空行)
};

/// @brief 固件文件解析器（静态工具类）
class FirmwareParser {
public:
    /// @brief 文件大小上限：1024 KB
    static constexpr qint64 MaxFileBytes = 1024LL * 1024LL;

    /// @brief 解析指定路径的固件文件
    /// @param path 完整文件路径
    /// @param hint IC 提示；默认 Auto（按文件大小启发兜底）
    /// @return 解析结果；失败时 valid=false 且 errorMessage 描述原因
    static FirmwareInfo parseFile(const QString &path, IcHint hint = IcHint::Auto);

    /// @brief 计算标准 CRC32（多项式 0xEDB88320，IEEE 802.3）
    static quint32 computeCrc32(const QByteArray &data);
};
