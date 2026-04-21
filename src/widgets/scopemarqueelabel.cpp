#include "widgets/scopemarqueelabel.h"

#include "ui/style_constants.h"

#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

using namespace MotorDev;

ScopeMarqueeLabel::ScopeMarqueeLabel(QWidget *parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("ScopeMarqueeLabel"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumWidth(Style::Size::MarqueeMinWidth);
    setMinimumHeight(Style::Size::MarqueeMinHeight);

    QFont labelFont = font();
    labelFont.setPixelSize(Style::Size::MarqueeFontSize);
    setFont(labelFont);

    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(40);
    connect(m_scrollTimer, &QTimer::timeout, this, [this]() {
        if (!m_shouldScroll || m_textWidth <= 0) {
            return;
        }
        ++m_offset;
        if (m_offset >= m_textWidth + m_gap) {
            m_offset = 0;
        }
        update();
    });
}

ScopeMarqueeLabel::~ScopeMarqueeLabel() = default;

void ScopeMarqueeLabel::setText(const QString &text) {
    if (m_text == text) {
        return;
    }
    m_text = text;
    recalculate();
}

QString ScopeMarqueeLabel::text() const {
    return m_text;
}

void ScopeMarqueeLabel::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setClipRect(rect());
    painter.setPen(Style::Color::MutedText);
    painter.setFont(font());

    const QFontMetrics metrics(font());
    const int baseline = (height() + metrics.ascent() - metrics.descent()) / 2;

    if (!m_shouldScroll) {
        painter.drawText(QRect(0, 0, width(), height()), Qt::AlignVCenter | Qt::AlignLeft, m_text);
        return;
    }

    const int firstX = -m_offset;
    const int secondX = firstX + m_textWidth + m_gap;
    painter.drawText(firstX, baseline, m_text);
    painter.drawText(secondX, baseline, m_text);
}

void ScopeMarqueeLabel::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    recalculate();
}

void ScopeMarqueeLabel::recalculate() {
    const QFontMetrics metrics(font());
    m_textWidth = metrics.horizontalAdvance(m_text);
    m_shouldScroll = m_textWidth > width();
    m_offset = 0;

    if (m_shouldScroll) {
        m_scrollTimer->start();
    } else {
        m_scrollTimer->stop();
    }
    update();
}
