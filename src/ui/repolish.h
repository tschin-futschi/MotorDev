// =============================================================================
// @file    repolish.h
// @brief   QSS 样式重新应用工具函数
//
// 提供 repolish() 工具函数，用于在动态修改 QSS 属性后强制刷新控件样式。
// 典型场景：通过 setProperty() 修改控件的动态属性后，需要调用 repolish()
// 使新的 QSS 规则立即生效。
// =============================================================================

#pragma once

#include <QStyle>
#include <QWidget>

namespace MotorDev::UiUtil {

/// @brief 强制重新应用控件的 QSS 样式
///
/// 依次执行 unpolish → polish → update，确保控件在动态属性变更后
/// 立即呈现正确的样式。对 nullptr 输入做安全保护。
///
/// @param widget  需要刷新样式的目标控件，允许为 nullptr（此时直接返回）
inline void repolish(QWidget *widget) {
    if (widget == nullptr) {
        return;
    }

    widget->style()->unpolish(widget);   // 移除当前样式
    widget->style()->polish(widget);     // 重新应用样式规则
    widget->update();                    // 触发重绘
}

}
