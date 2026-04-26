// =============================================================================
// @file    scopemarqueelabel.cpp
// @brief   跑马灯标签实现 — 文字宽度检测、滚动动画、绘制
// =============================================================================
#include "widgets/scopemarqueelabel.h"

#include "ui/style_constants.h"

#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

using namespace MotorDev;

// =============================================================================
// 构造 / 析构
// =============================================================================

/// @brief 初始化跑马灯标签，配置字体与滚动定时器。
///
/// - 字体大小由 Style::Size::MarqueeFontSize 决定
/// - 滚动帧率 ≈ 25 fps（40 ms 间隔）
/// - 仅当文字宽度超过控件宽度时才启动滚动
ScopeMarqueeLabel::ScopeMarqueeLabel(QWidget *parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("ScopeMarqueeLabel"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumWidth(Style::Size::MarqueeMinWidth);
    setMinimumHeight(Style::Size::MarqueeMinHeight);

    // 设置跑马灯专用字体大小
    QFont labelFont = font();
    labelFont.setPixelSize(Style::Size::MarqueeFontSize);
    setFont(labelFont);

    // ---------- 滚动定时器 ----------
    // 每 40 ms 递增偏移量，实现从右向左的平滑滚动
    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(40);
    connect(m_scrollTimer, &QTimer::timeout, this, [this]() {
        if (!m_shouldScroll || m_textWidth <= 0) {
            return;
        }
        ++m_offset;
        // 当第一段文字完全滚出 + 间隔区也滚过后，重置偏移形成无缝循环
        if (m_offset >= m_textWidth + m_gap) {
            m_offset = 0;
        }
        update();
    });
}

ScopeMarqueeLabel::~ScopeMarqueeLabel() = default;

// =============================================================================
// 公共接口
// =============================================================================

/// @brief 设置显示文本，触发宽度重算与滚动判定。
void ScopeMarqueeLabel::setText(const QString &text) {
    if (m_text == text) {
        return;
    }
    m_text = text;
    recalculate();
}

/// @brief 返回当前显示文本。
QString ScopeMarqueeLabel::text() const {
    return m_text;
}

// =============================================================================
// 绘制
// =============================================================================

/// @brief 自定义绘制：静态居左显示或双副本滚动。
///
/// 滚动模式下绘制两份文本，第二份紧接第一份末尾 + m_gap 间距，
/// 当第一份完全滚出视口后偏移归零，形成无缝循环效果。
void ScopeMarqueeLabel::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setClipRect(rect());
    painter.setPen(Style::Color::MutedText);
    painter.setFont(font());

    const QFontMetrics metrics(font());
    // 计算基线 Y 坐标，使文字垂直居中
    const int baseline = (height() + metrics.ascent() - metrics.descent()) / 2;

    // 文字未超出控件宽度，静态居左绘制即可
    if (!m_shouldScroll) {
        painter.drawText(QRect(0, 0, width(), height()), Qt::AlignVCenter | Qt::AlignLeft, m_text);
        return;
    }

    // ---------- 滚动模式：绘制两份文本实现无缝循环 ----------
    const int firstX = -m_offset;                       // 第一份文本的 X 起点
    const int secondX = firstX + m_textWidth + m_gap;   // 第二份文本紧随其后
    painter.drawText(firstX, baseline, m_text);
    painter.drawText(secondX, baseline, m_text);
}

// =============================================================================
// 事件处理
// =============================================================================

/// @brief 控件尺寸变化时重新计算是否需要滚动。
void ScopeMarqueeLabel::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    recalculate();
}

// =============================================================================
// 内部方法
// =============================================================================

/// @brief 计算文字像素宽度，决定是否启动滚动。
///
/// - 若文字宽度 > 控件宽度 → 启动定时器滚动
/// - 否则 → 停止定时器，静态显示
void ScopeMarqueeLabel::recalculate() {
    const QFontMetrics metrics(font());
    m_textWidth = metrics.horizontalAdvance(m_text);
    m_shouldScroll = m_textWidth > width();
    m_offset = 0;                                       // 重置滚动偏移

    if (m_shouldScroll) {
        m_scrollTimer->start();
    } else {
        m_scrollTimer->stop();
    }
    update();
}
