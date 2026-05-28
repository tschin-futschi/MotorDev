// =============================================================================
// @file    batch_register_file.h
// @brief   批量读写配置文件解析与回写（协议层）
//
// 配置文件格式（参考 register_read.txt 同款）：
//   每行 `<addr>, <value>`，地址和值均为 16-bit 0x 十六进制
//   `//` 起到行末为注释（行首整行注释、行末同行尾注释都算）
//   空行在解析时跳过；在回写时被删除（决策 #4 + #5）
//
// 解析策略：严格全或无（决策 #13）——任一异常立刻返回，不进 entries
// 回写策略：保留原文件结构（注释 / 整行注释 / 行序），仅替换数据行「值」列；
//           输出时删除所有空行（决策 #5）
//
// 被 RegisterRwTab 使用，不依赖 Qt 模型层。
// =============================================================================
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>

namespace BatchRegisterFile {

/// @brief 单条数据行解析结果
struct Entry {
    int lineNumber = 0;     ///< 原文件 1-based 行号（用于回写定位）
    quint16 addr = 0;       ///< 16-bit 寄存器地址
    quint16 value = 0;      ///< 16-bit 寄存器值（unsigned 存储，写入时 reinterpret 为 qint16）
};

/// @brief 解析状态码
enum class ParseStatus {
    Ok,
    FileOpenFailed,         ///< 文件打开失败（路径无效 / 无权限）
    EmptyFile,              ///< 文件无有效数据行（全是注释 / 空行）
    LineFormatError,        ///< 单行格式错误（缺逗号 / 非合法十六进制 / 字段缺失）
    DuplicateAddress,       ///< 同一地址在文件中出现多次
    AddressOutOfRange,      ///< 地址不是 16-bit
    ValueOutOfRange,        ///< 值不是 16-bit
};

/// @brief 解析结果
struct ParseResult {
    ParseStatus status = ParseStatus::Ok;
    QString errorMessage;       ///< 中文错误描述，可直接显示给用户
    int errorLineNumber = 0;    ///< 出错行号（适用时；FileOpenFailed/EmptyFile 时为 0）
    QVector<Entry> entries;     ///< 解析到的合法数据行；status=Ok 时有效
    QByteArray rawBytes;        ///< 原文件原始字节流（用于回写时保留编码 / 行尾符）
    QStringList rawLines;       ///< 原文件按行切分（保留原始内容），用于 writeBackValues 回写
};

/// @brief 解析配置文件
/// @param filePath 文件路径
/// @return 解析结果；status=Ok 时 entries 有效；否则查 errorMessage
ParseResult parseFile(const QString &filePath);

/// @brief 把读到的新值回写到原文件，保留原结构（决策 #5）
///
/// 行为规则：
/// - 注释行（含整行注释和被注释的数据行 `//0xB567, 0x4444`）原样保留
/// - 数据行：保留地址原文、保留逗号位置、保留行末注释；仅"值"段被替换为 `0x%04X`（大写）
/// - 空行（rawLines 中 trim 后为空）全部删除
///
/// @param filePath        原文件路径（覆盖写）
/// @param rawLines        原始文件按行切分（来自 ParseResult.rawLines）
/// @param updatedEntries  顺序与 ParseResult.entries 一致；value 字段为新读到的值
/// @param errorMessage    失败时输出错误描述（nullptr-safe）
/// @return 是否成功写入
bool writeBackValues(const QString &filePath,
                     const QStringList &rawLines,
                     const QVector<Entry> &updatedEntries,
                     QString *errorMessage = nullptr);

}  // namespace BatchRegisterFile
