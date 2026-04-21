#pragma once

#include <QString>
#include <QtGlobal>

namespace SamplingConfig {

uint8_t intervalIndexForText(const QString &text);
int intervalUsForIndex(uint8_t index);
int displayWindowMsForText(const QString &text);

}  // namespace SamplingConfig
