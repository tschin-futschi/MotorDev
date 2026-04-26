// =============================================================================
// @file    scopemarqueelabel.h
// @brief   跑马灯标签 — 文字超出宽度时自动滚动显示
//
// ScopeMarqueeLabel 用于示波器底部面板，展示当前活跃状态文本。
// 当文字宽度超过控件宽度时，自动启动水平滚动动画。
// =============================================================================
#pragma once

#include <QString>
#include <QWidget>

class QTimer;

/// @brief 跑马灯标签控件 — 文字溢出时自动水平滚动
///
/// 文字未溢出时静态居左显示；溢出时以循环滚动方式展示完整文本，
/// 两次循环之间插入 gap 像素的空白间距。
class ScopeMarqueeLabel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeMarqueeLabel(QWidget *parent = nullptr);
    ~ScopeMarqueeLabel() override;

    /// @brief 设置显示文本
    /// @param text 新文本（空文本停止滚动）
    void setText(const QString &text);

    /// @brief 获取当前文本
    QString text() const;

protected:
    /// @brief 绘制事件 — 静态显示或滚动绘制
    void paintEvent(QPaintEvent *event) override;

    /// @brief 尺寸变化时重新计算是否需要滚动
    void resizeEvent(QResizeEvent *event) override;

private:
    /// @brief 重新计算文字宽度和滚动状态
    void recalculate();

    QString m_text;                     ///< 当前显示文本
    QTimer *m_scrollTimer = nullptr;    ///< 滚动定时器
    int m_textWidth = 0;                ///< 文字像素宽度
    int m_offset = 0;                   ///< 当前滚动偏移量
    int m_gap = 40;                     ///< 循环间距（像素）
    bool m_shouldScroll = false;        ///< 是否需要滚动
};
