// =============================================================================
// @file    firmware_parser.cpp
// @brief   固件文件解析实现 — Intel HEX 行级解析、段合并、CRC32
// =============================================================================
#include "protocol/firmware_parser.h"

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QTextStream>

namespace {

// 标准 CRC32 (poly 0xEDB88320, IEEE 802.3) 表，用首次调用时懒初始化
quint32 crc32Compute(const QByteArray &data) {
    static quint32 table[256];
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < 256; ++i) {
            quint32 c = static_cast<quint32>(i);
            for (int j = 0; j < 8; ++j) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        initialized = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    const auto *bytes = reinterpret_cast<const uchar *>(data.constData());
    const int n = data.size();
    for (int i = 0; i < n; ++i) {
        crc = table[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8);
    }
    return ~crc;
}

bool nibble(QChar c, int &out) {
    const ushort u = c.unicode();
    if (u >= '0' && u <= '9') { out = u - '0'; return true; }
    if (u >= 'A' && u <= 'F') { out = u - 'A' + 10; return true; }
    if (u >= 'a' && u <= 'f') { out = u - 'a' + 10; return true; }
    return false;
}

bool parseByte(const QString &line, int pos, int &outByte) {
    if (pos + 1 >= line.length()) return false;
    int hi = 0, lo = 0;
    if (!nibble(line[pos], hi) || !nibble(line[pos + 1], lo)) return false;
    outByte = (hi << 4) | lo;
    return true;
}

FirmwareInfo parseBin(const QFileInfo &fi, const QByteArray &content) {
    FirmwareInfo info;
    info.fileName = fi.fileName();
    info.fileSizeBytes = content.size();
    info.format = FirmwareFormat::Bin;
    info.data = content;
    info.effectiveBytes = content.size();
    info.crc32 = FirmwareParser::computeCrc32(content);
    info.valid = true;
    return info;
}

/// @brief Intel HEX 解析
///
/// 仅支持记录类型 00（data）/ 01（EOF）/ 04（Extended Linear Address）/ 05（Start Linear Address，忽略）。
/// 02/03 与未知类型直接报错。
/// 校验所有行的校验和、长度字段、字符合法性。
/// 合并产物：从 minAddress 到 maxAddress 的连续二进制，未覆盖处填 0xFF。
FirmwareInfo parseHex(const QFileInfo &fi, const QByteArray &content) {
    FirmwareInfo info;
    info.fileName = fi.fileName();
    info.fileSizeBytes = content.size();
    info.format = FirmwareFormat::Hex;

    QMap<quint32, quint8> bytes;  // 用 QMap 自动按地址排序
    quint32 upperAddr = 0;
    bool eofSeen = false;
    int lineNo = 0;

    QTextStream stream(content);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        ++lineNo;
        if (line.isEmpty()) continue;
        if (!line.startsWith(QLatin1Char(':'))) {
            info.errorMessage = QStringLiteral("第 %1 行：缺少 ':' 起始符").arg(lineNo);
            return info;
        }
        line = line.mid(1);
        if (line.length() < 10 || (line.length() % 2) != 0) {
            info.errorMessage = QStringLiteral("第 %1 行：长度异常").arg(lineNo);
            return info;
        }

        int byteCount = 0, hiAddr = 0, loAddr = 0, type = 0, b = 0;
        if (!parseByte(line, 0, byteCount)
            || !parseByte(line, 2, hiAddr)
            || !parseByte(line, 4, loAddr)
            || !parseByte(line, 6, type)) {
            info.errorMessage = QStringLiteral("第 %1 行：头部解析失败").arg(lineNo);
            return info;
        }
        const int expectedLen = 10 + byteCount * 2;
        if (line.length() != expectedLen) {
            info.errorMessage = QStringLiteral("第 %1 行：长度与字节数不符").arg(lineNo);
            return info;
        }

        QByteArray dataBytes;
        dataBytes.reserve(byteCount);
        for (int i = 0; i < byteCount; ++i) {
            if (!parseByte(line, 8 + i * 2, b)) {
                info.errorMessage = QStringLiteral("第 %1 行：数据字节解析失败").arg(lineNo);
                return info;
            }
            dataBytes.append(static_cast<char>(b));
        }

        if (!parseByte(line, 8 + byteCount * 2, b)) {
            info.errorMessage = QStringLiteral("第 %1 行：校验和解析失败").arg(lineNo);
            return info;
        }
        const int checksumExpected = b;
        int sum = byteCount + hiAddr + loAddr + type;
        for (uchar by : dataBytes) sum += by;
        const int checksumActual = ((-sum) & 0xFF);
        if (checksumExpected != checksumActual) {
            info.errorMessage = QStringLiteral("第 %1 行：校验和不符（期望 0x%2，实际 0x%3）")
                                    .arg(lineNo)
                                    .arg(checksumActual, 2, 16, QLatin1Char('0'))
                                    .arg(checksumExpected, 2, 16, QLatin1Char('0'));
            return info;
        }

        const quint16 addr16 = static_cast<quint16>((hiAddr << 8) | loAddr);
        switch (type) {
        case 0x00: {  // Data
            const quint32 fullAddr = upperAddr | addr16;
            for (int i = 0; i < byteCount; ++i) {
                bytes.insert(fullAddr + i, static_cast<quint8>(dataBytes[i]));
            }
            break;
        }
        case 0x01:  // EOF
            eofSeen = true;
            break;
        case 0x04: {  // Extended Linear Address
            if (byteCount != 2) {
                info.errorMessage = QStringLiteral("第 %1 行：04 记录长度应为 2").arg(lineNo);
                return info;
            }
            const auto hb = static_cast<quint8>(dataBytes[0]);
            const auto lb = static_cast<quint8>(dataBytes[1]);
            upperAddr = (static_cast<quint32>(hb) << 24) | (static_cast<quint32>(lb) << 16);
            break;
        }
        case 0x05:  // Start Linear Address — 仅启动地址，不影响内容，忽略
            break;
        case 0x02:
        case 0x03:
            info.errorMessage = QStringLiteral("第 %1 行：暂不支持记录类型 0x%2")
                                    .arg(lineNo)
                                    .arg(type, 2, 16, QLatin1Char('0'));
            return info;
        default:
            info.errorMessage = QStringLiteral("第 %1 行：未知记录类型 0x%2")
                                    .arg(lineNo)
                                    .arg(type, 2, 16, QLatin1Char('0'));
            return info;
        }
    }

    if (!eofSeen) {
        info.errorMessage = QStringLiteral("缺少 EOF 记录（:00000001FF）");
        return info;
    }
    if (bytes.isEmpty()) {
        info.errorMessage = QStringLiteral("文件不包含任何数据记录");
        return info;
    }

    // QMap 按 key 自动有序遍历
    auto it = bytes.constBegin();
    info.minAddress = it.key();
    quint32 segStart = it.key();
    quint32 segPrev = it.key();
    ++it;
    for (; it != bytes.constEnd(); ++it) {
        const quint32 a = it.key();
        if (a != segPrev + 1) {
            info.segments.append({segStart, segPrev - segStart + 1});
            segStart = a;
        }
        segPrev = a;
    }
    info.segments.append({segStart, segPrev - segStart + 1});
    info.maxAddress = segPrev;
    info.effectiveBytes = bytes.size();

    const quint64 totalLen = static_cast<quint64>(info.maxAddress)
                             - static_cast<quint64>(info.minAddress) + 1ull;
    if (totalLen > static_cast<quint64>(FirmwareParser::MaxFileBytes)) {
        info.errorMessage = QStringLiteral("合并后大小 %1 字节超过上限 1024 KB").arg(totalLen);
        return info;
    }
    info.data = QByteArray(static_cast<int>(totalLen), '\xFF');
    for (auto kt = bytes.constBegin(); kt != bytes.constEnd(); ++kt) {
        info.data[static_cast<int>(kt.key() - info.minAddress)] = static_cast<char>(kt.value());
    }
    info.crc32 = FirmwareParser::computeCrc32(info.data);
    info.valid = true;
    return info;
}

}  // namespace

quint32 FirmwareParser::computeCrc32(const QByteArray &data) {
    return crc32Compute(data);
}

namespace {

// -----------------------------------------------------------------------------
// Dongwoon HL9788N 自定义 hex 文本（.hex 后缀但非 Intel HEX）
// 行格式：8 个十六进制字符 + 换行；无 `:` 前缀、无校验和；总行数 16384 = 64KB 数据
// 拆分规则（与 vendor hl9788n_api_ref.cpp:752-753 一致）：
//   out[2i]   = val32 & 0xFFFF          // low 16
//   out[2i+1] = (val32 >> 16) & 0xFFFF  // high 16
// 输出 data: 65536 字节小端二进制，可直接 reinterpret_cast<const uint16_t*> 传 vendor。
// -----------------------------------------------------------------------------
FirmwareInfo parseHl9788Hex(const QFileInfo &fi, const QByteArray &content) {
    constexpr int kExpectedWords = 32768;       // 64KB / 2 = 32768 uint16
    constexpr int kExpectedLines = kExpectedWords / 2;  // 16384

    FirmwareInfo info;
    info.fileName = fi.fileName();
    info.fileSizeBytes = content.size();
    info.format = FirmwareFormat::Hl9788Hex;

    QByteArray buf(kExpectedWords * static_cast<int>(sizeof(quint16)), 0);
    auto *outWords = reinterpret_cast<quint16 *>(buf.data());

    int wordIdx = 0;
    int lineNo = 0;
    int dataLineCount = 0;
    int pos = 0;
    const int total = content.size();

    while (pos < total) {
        int nl = content.indexOf('\n', pos);
        const int end = (nl < 0) ? total : nl;
        QByteArray line = content.mid(pos, end - pos);
        pos = (nl < 0) ? total : (nl + 1);
        ++lineNo;

        if (line.endsWith('\r')) line.chop(1);
        line = line.trimmed();
        if (line.isEmpty()) continue;

        for (int i = 0; i < line.size(); ++i) {
            const char c = line.at(i);
            const bool hexOk = (c >= '0' && c <= '9') ||
                                (c >= 'A' && c <= 'F') ||
                                (c >= 'a' && c <= 'f');
            if (!hexOk) {
                const QString snippet = QString::fromLatin1(line.left(32));
                info.errorMessage = QStringLiteral("第 %1 行非法 hex 字符 '%2' (内容: \"%3\")")
                                        .arg(lineNo).arg(QChar(c)).arg(snippet);
                return info;
            }
        }

        if (wordIdx + 1 >= kExpectedWords) {
            info.errorMessage = QStringLiteral(
                "第 %1 行：固件超出预期长度 (已读 %2 words，上限 %3)")
                .arg(lineNo).arg(wordIdx).arg(kExpectedWords);
            return info;
        }

        bool convOk = false;
        const quint32 val32 = static_cast<quint32>(line.toUInt(&convOk, 16));
        if (!convOk) {
            const QString snippet = QString::fromLatin1(line.left(32));
            info.errorMessage = QStringLiteral("第 %1 行 hex 转换失败 (内容: \"%2\")")
                                    .arg(lineNo).arg(snippet);
            return info;
        }

        outWords[wordIdx++] = static_cast<quint16>(val32 & 0xFFFFu);
        outWords[wordIdx++] = static_cast<quint16>((val32 >> 16) & 0xFFFFu);
        ++dataLineCount;
    }

    if (wordIdx != kExpectedWords) {
        info.errorMessage = QStringLiteral(
            "数据行数不足：得到 %1 行 (%2 words)，预期 %3 行 (%4 words)")
            .arg(dataLineCount).arg(wordIdx)
            .arg(kExpectedLines).arg(kExpectedWords);
        return info;
    }

    info.data = buf;
    info.effectiveBytes = buf.size();
    info.crc32 = FirmwareParser::computeCrc32(buf);
    info.valid = true;
    return info;
}

// 内容嗅探：根据第一行非空白字符判断是 Intel HEX (`:` 起始) 还是 Hl9788Hex (纯 hex 字符)
bool looksLikeIntelHex(const QByteArray &content) {
    for (int i = 0; i < content.size(); ++i) {
        const char c = content.at(i);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        return c == ':';
    }
    return false;
}

}  // namespace

FirmwareInfo FirmwareParser::parseFile(const QString &path) {
    FirmwareInfo info;
    QFileInfo fi(path);
    info.fileName = fi.fileName();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        info.errorMessage = QStringLiteral("无法打开文件：%1").arg(f.errorString());
        return info;
    }
    const qint64 size = f.size();
    if (size <= 0) {
        info.errorMessage = QStringLiteral("文件为空");
        return info;
    }
    if (size > MaxFileBytes) {
        info.errorMessage = QStringLiteral("文件大小 %1 字节超过上限 1024 KB").arg(size);
        return info;
    }

    const QByteArray content = f.readAll();
    f.close();

    const QString suffix = fi.suffix().toLower();
    if (suffix == QLatin1String("bin")) {
        return parseBin(fi, content);
    }
    if (suffix == QLatin1String("hex")) {
        // .hex 后缀有两种实际格式：Intel HEX 和 DW HL9788N 自定义 hex。
        // 嗅探首字符：`:` 是 Intel HEX；其余按 Hl9788Hex 处理。
        if (looksLikeIntelHex(content)) {
            return parseHex(fi, content);
        }
        return parseHl9788Hex(fi, content);
    }
    info.errorMessage = QStringLiteral("不支持的文件格式：.%1").arg(suffix);
    return info;
}
