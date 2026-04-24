#include "protocol/sampling_config.h"

namespace SamplingConfig {

QStringList intervalLabels() {
    return {
        QStringLiteral("200 us"),  QStringLiteral("300 us"),
        QStringLiteral("500 us"),  QStringLiteral("750 us"),
        QStringLiteral("1000 us"), QStringLiteral("1500 us"),
        QStringLiteral("2000 us")
    };
}

uint8_t intervalIndexForText(const QString &text) {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("200 us")) return 0x00;
    if (normalized == QStringLiteral("300 us")) return 0x01;
    if (normalized == QStringLiteral("500 us")) return 0x02;
    if (normalized == QStringLiteral("750 us")) return 0x03;
    if (normalized == QStringLiteral("1000 us")) return 0x04;
    if (normalized == QStringLiteral("1500 us")) return 0x05;
    if (normalized == QStringLiteral("2000 us")) return 0x06;
    return 0x04;
}

int intervalUsForIndex(uint8_t index) {
    switch (index) {
    case 0x00: return 200;
    case 0x01: return 300;
    case 0x02: return 500;
    case 0x03: return 750;
    case 0x04: return 1000;
    case 0x05: return 1500;
    case 0x06: return 2000;
    default: return 1000;
    }
}

int displayWindowMsForText(const QString &text) {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("50 ms")) return 50;
    if (normalized == QStringLiteral("200 ms")) return 200;
    if (normalized == QStringLiteral("500 ms")) return 500;
    if (normalized == QStringLiteral("1000 ms")) return 1000;
    if (normalized == QStringLiteral("2000 ms")) return 2000;
    if (normalized == QStringLiteral("4000 ms")) return 4000;
    return 50;
}

}  // namespace SamplingConfig
