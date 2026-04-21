#pragma once

#include <QString>
#include <QtGlobal>

namespace RegisterUtils {

bool parseNumber(const QString &text, quint16 *out);

}  // namespace RegisterUtils
