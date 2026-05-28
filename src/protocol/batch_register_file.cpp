// =============================================================================
// @file    batch_register_file.cpp
// @brief   批量读写配置文件解析与回写实现
// =============================================================================
#include "protocol/batch_register_file.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>

namespace BatchRegisterFile {

namespace {

/// @brief 解析 0x 十六进制字符串到 quint32
/// @param token      去过空白的纯 token（如 "0xB500"）
/// @param outValue   输出值
/// @return 是否合法 0x 十六进制（必须以 0x / 0X 前缀；非空十六进制位）
bool parseHexToken(const QString &token, quint32 *outValue) {
    if (outValue == nullptr) return false;
    if (token.size() < 3) return false;
    if (!(token.startsWith(QStringLiteral("0x")) || token.startsWith(QStringLiteral("0X")))) {
        return false;
    }
    const QString digits = token.mid(2);
    if (digits.isEmpty()) return false;
    for (QChar c : digits) {
        if (!c.isLetterOrNumber()) return false;
        // 仅允许 0-9 / a-f / A-F
        const QChar low = c.toLower();
        if (!(low.isDigit() || (low >= QChar('a') && low <= QChar('f')))) return false;
    }
    bool ok = false;
    const quint32 v = digits.toUInt(&ok, 16);
    if (!ok) return false;
    *outValue = v;
    return true;
}

/// @brief 剥离行末 // 注释，返回数据部分（trim 前后空白）；不影响整行注释判断
QString stripInlineComment(const QString &line, bool *wasComment) {
    if (wasComment != nullptr) *wasComment = false;
    const int idx = line.indexOf(QStringLiteral("//"));
    QString head = (idx >= 0) ? line.left(idx) : line;
    head = head.trimmed();
    if (wasComment != nullptr && idx == 0) {
        // 整行注释
        *wasComment = true;
    }
    return head;
}

}  // namespace

ParseResult parseFile(const QString &filePath) {
    ParseResult result;

    // 1. 打开文件
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        result.status = ParseStatus::FileOpenFailed;
        result.errorMessage = QStringLiteral("文件读取失败：%1").arg(f.errorString());
        return result;
    }
    result.rawBytes = f.readAll();
    f.close();

    // 2. 按 UTF-8 解码（推荐用户配置文件用 UTF-8 保存；ASCII / 0xXX 数据行不依赖编码）
    QString content = QString::fromUtf8(result.rawBytes);

    // 3. 按行切分（保留原始行内容）
    // 先把 \r\n 和 \r 归一化为 \n，再 split；KeepEmptyParts 保留空行用于 lineNumber 计数
    QString normalized = content;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QChar('\r'), QChar('\n'));
    result.rawLines = normalized.split(QChar('\n'), Qt::KeepEmptyParts);

    // 4. 逐行解析
    QSet<quint16> seenAddrs;
    for (int i = 0; i < result.rawLines.size(); ++i) {
        const int lineNo = i + 1;  // 1-based
        const QString &rawLine = result.rawLines.at(i);

        bool isFullComment = false;
        const QString head = stripInlineComment(rawLine, &isFullComment);

        // 空行 / 整行注释 / 注释剥离后为空 → 跳过
        if (head.isEmpty()) {
            continue;
        }
        Q_UNUSED(isFullComment);

        // 按逗号分割
        const QStringList parts = head.split(QChar(','), Qt::KeepEmptyParts);
        if (parts.size() != 2) {
            result.status = ParseStatus::LineFormatError;
            result.errorLineNumber = lineNo;
            result.errorMessage = QStringLiteral("第 %1 行格式错误（应为 `<addr>, <value>`）：%2")
                                      .arg(lineNo)
                                      .arg(rawLine.trimmed());
            return result;
        }

        const QString addrToken = parts.at(0).trimmed();
        const QString valueToken = parts.at(1).trimmed();
        if (addrToken.isEmpty() || valueToken.isEmpty()) {
            result.status = ParseStatus::LineFormatError;
            result.errorLineNumber = lineNo;
            result.errorMessage = QStringLiteral("第 %1 行格式错误（字段为空）：%2")
                                      .arg(lineNo)
                                      .arg(rawLine.trimmed());
            return result;
        }

        // 解析地址
        quint32 addrRaw = 0;
        if (!parseHexToken(addrToken, &addrRaw)) {
            result.status = ParseStatus::LineFormatError;
            result.errorLineNumber = lineNo;
            result.errorMessage = QStringLiteral("第 %1 行地址不是合法 0x 十六进制：%2")
                                      .arg(lineNo)
                                      .arg(addrToken);
            return result;
        }
        if (addrRaw > 0xFFFFu) {
            result.status = ParseStatus::AddressOutOfRange;
            result.errorLineNumber = lineNo;
            result.errorMessage = QStringLiteral("第 %1 行地址越界（应在 0x0000~0xFFFF）：%2")
                                      .arg(lineNo)
                                      .arg(addrToken);
            return result;
        }

        // 解析值
        quint32 valueRaw = 0;
        if (!parseHexToken(valueToken, &valueRaw)) {
            result.status = ParseStatus::LineFormatError;
            result.errorLineNumber = lineNo;
            result.errorMessage = QStringLiteral("第 %1 行值不是合法 0x 十六进制：%2")
                                      .arg(lineNo)
                                      .arg(valueToken);
            return result;
        }
        if (valueRaw > 0xFFFFu) {
            result.status = ParseStatus::ValueOutOfRange;
            result.errorLineNumber = lineNo;
            result.errorMessage = QStringLiteral("第 %1 行值越界（应在 0x0000~0xFFFF）：%2")
                                      .arg(lineNo)
                                      .arg(valueToken);
            return result;
        }

        const quint16 addr = static_cast<quint16>(addrRaw);
        const quint16 value = static_cast<quint16>(valueRaw);

        // 重复地址检查
        if (seenAddrs.contains(addr)) {
            // 找出第一次出现的行号（线性扫描已有 entries）
            int firstLine = 0;
            for (const Entry &e : std::as_const(result.entries)) {
                if (e.addr == addr) { firstLine = e.lineNumber; break; }
            }
            result.status = ParseStatus::DuplicateAddress;
            result.errorLineNumber = lineNo;
            result.errorMessage = QStringLiteral("第 %1 行与第 %2 行地址重复：0x%3")
                                      .arg(lineNo)
                                      .arg(firstLine)
                                      .arg(addr, 4, 16, QLatin1Char('0')).toUpper()
                                      .replace(QStringLiteral("0X"), QStringLiteral("0x"));
            return result;
        }
        seenAddrs.insert(addr);

        Entry entry;
        entry.lineNumber = lineNo;
        entry.addr = addr;
        entry.value = value;
        result.entries.append(entry);
    }

    // 5. 空文件检查
    if (result.entries.isEmpty()) {
        result.status = ParseStatus::EmptyFile;
        result.errorMessage = QStringLiteral("文件无有效数据行（仅含注释或空行）");
        return result;
    }

    result.status = ParseStatus::Ok;
    return result;
}

bool writeBackValues(const QString &filePath,
                     const QStringList &rawLines,
                     const QVector<Entry> &updatedEntries,
                     QString *errorMessage) {
    auto setErr = [&](const QString &msg) {
        if (errorMessage != nullptr) *errorMessage = msg;
    };

    // 建立 lineNumber → updated value 的索引
    QHash<int, quint16> lineToValue;
    lineToValue.reserve(updatedEntries.size());
    for (const Entry &e : updatedEntries) {
        lineToValue.insert(e.lineNumber, e.value);
    }

    QStringList outputLines;
    outputLines.reserve(rawLines.size());

    for (int i = 0; i < rawLines.size(); ++i) {
        const int lineNo = i + 1;
        const QString &rawLine = rawLines.at(i);

        // 1. 空行（trim 后为空且不含 //）→ 删除（决策 #4 + #5）
        if (rawLine.trimmed().isEmpty()) {
            continue;
        }

        // 2. 不是数据行 → 原样保留（含整行注释、被注释的数据行）
        if (!lineToValue.contains(lineNo)) {
            outputLines.append(rawLine);
            continue;
        }

        // 3. 数据行 → 替换「值」部分
        // 找到 // 注释位置（如果有）
        const int commentIdx = rawLine.indexOf(QStringLiteral("//"));
        const QString head = (commentIdx >= 0) ? rawLine.left(commentIdx) : rawLine;
        const QString tail = (commentIdx >= 0) ? rawLine.mid(commentIdx) : QString();

        // head 内必有逗号（解析阶段已校验）
        const int commaIdx = head.indexOf(QChar(','));
        if (commaIdx < 0) {
            // 防御性：理论上不会到这里（lineNumber 对应的 entry 必含逗号）
            outputLines.append(rawLine);
            continue;
        }

        const QString addrPart = head.left(commaIdx);           // "0xB500 " 含逗号前的空白
        // value 区段：逗号之后到 head 末尾的部分（含前后空白）
        const QString valueArea = head.mid(commaIdx + 1);

        // 拆解 valueArea 的前后空白
        int leftWs = 0;
        while (leftWs < valueArea.size() && valueArea.at(leftWs).isSpace()) ++leftWs;
        int rightWs = 0;
        while (rightWs < valueArea.size() - leftWs &&
               valueArea.at(valueArea.size() - 1 - rightWs).isSpace()) ++rightWs;

        const QString leftSpacing = valueArea.left(leftWs);
        const QString rightSpacing = valueArea.right(rightWs);

        const quint16 newValue = lineToValue.value(lineNo);
        const QString newValueStr = QStringLiteral("0x%1")
                                        .arg(newValue, 4, 16, QLatin1Char('0'))
                                        .toUpper()
                                        .replace(QStringLiteral("0X"), QStringLiteral("0x"));

        QString newLine = addrPart + QChar(',') + leftSpacing + newValueStr + rightSpacing + tail;
        outputLines.append(newLine);
    }

    // 4. 用原平台换行符写回（默认 \r\n，与 Windows 平台一致；Qt 内部 \n 也可，但保留 CRLF 更稳）
    QString output = outputLines.join(QStringLiteral("\r\n"));
    if (!output.isEmpty()) {
        output.append(QStringLiteral("\r\n"));
    }

    // 5. 安全写回（先写临时文件再 rename，QSaveFile 自动管理）
    QSaveFile saver(filePath);
    if (!saver.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setErr(QStringLiteral("文件写回失败（打开）：%1").arg(saver.errorString()));
        return false;
    }
    const QByteArray bytes = output.toUtf8();
    const qint64 written = saver.write(bytes);
    if (written != bytes.size()) {
        setErr(QStringLiteral("文件写回失败（短写 %1/%2）").arg(written).arg(bytes.size()));
        saver.cancelWriting();
        return false;
    }
    if (!saver.commit()) {
        setErr(QStringLiteral("文件写回失败（提交）：%1").arg(saver.errorString()));
        return false;
    }

    return true;
}

}  // namespace BatchRegisterFile
