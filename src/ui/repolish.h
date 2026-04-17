#pragma once

#include <QStyle>
#include <QWidget>

namespace MotorDev::UiUtil {

inline void repolish(QWidget *widget) {
    if (widget == nullptr) {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

}
