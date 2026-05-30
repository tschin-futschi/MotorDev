// =============================================================================
// @file    register_utils.cpp
// @brief   寄存器地址/值输入解析工具实现
// =============================================================================

#include "protocol/register_utils.h"

namespace RegisterUtils {

bool parseNumber(const QString &text, quint16 *out) {
    if (out == nullptr) {
        return false;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    bool ok = false;
    uint value = 0;

    // 根据前缀判断进制
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        value = trimmed.mid(2).toUInt(&ok, 16);  // 十六进制
    } else {
        value = trimmed.toUInt(&ok, 10);           // 十进制
    }

    // 检查转换结果和 16 位范围
    if (!ok || value > 0xFFFFu) {
        return false;
    }

    *out = static_cast<quint16>(value);
    return true;
}

bool parseSignedValue(const QString &text, qint16 *value) {
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    // 十六进制：按 16 位裸值解析后转有符号（0xFFFF → -1）
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        trimmed.remove(0, 2);
        bool ok = false;
        const quint16 raw = trimmed.toUShort(&ok, 16);
        if (!ok) {
            return false;
        }
        if (value != nullptr) {
            *value = static_cast<qint16>(raw);
        }
        return true;
    }

    // 十进制：有符号 -32768 ~ 32767
    bool ok = false;
    const qint16 parsed = trimmed.toShort(&ok, 10);
    if (!ok) {
        return false;
    }
    if (value != nullptr) {
        *value = parsed;
    }
    return true;
}

QString formatValue(qint16 value, bool hex) {
    if (hex) {
        const quint16 raw = static_cast<quint16>(value);
        return QStringLiteral("0x") + QString::number(raw, 16).toUpper().rightJustified(4, QLatin1Char('0'));
    }
    return QString::number(value);
}

}  // namespace RegisterUtils
