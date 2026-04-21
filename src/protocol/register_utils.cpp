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
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        value = trimmed.mid(2).toUInt(&ok, 16);
    } else {
        value = trimmed.toUInt(&ok, 10);
    }

    if (!ok || value > 0xFFFFu) {
        return false;
    }

    *out = static_cast<quint16>(value);
    return true;
}

}  // namespace RegisterUtils
