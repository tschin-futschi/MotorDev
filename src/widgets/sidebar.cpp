#include "widgets/sidebar.h"

#include "ui/style_constants.h"

#include <QPropertyAnimation>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace MotorDev;

Sidebar::Sidebar(const QString &title, QWidget *contentWidget, QWidget *parent)
    : QWidget(parent) {
    setupUi(title);
    m_headerLabel->setContentsMargins(Style::Size::SidebarContentHorizontalPadding, 0,
                                        Style::Size::SidebarContentHorizontalPadding, 0);

    m_bodyWidth = Style::Size::SidebarWidth;
    setMinimumWidth(Style::Size::SidebarTotalWidth);
    setMaximumWidth(Style::Size::SidebarTotalWidth);

    m_animation = new QPropertyAnimation(this, "maximumWidth", this);
    m_animation->setDuration(Style::Size::SidebarAnimationMs);

    auto *bodyLayout = qobject_cast<QVBoxLayout *>(m_bodyWidget->layout());
    m_contentWidget = contentWidget;
    if (bodyLayout != nullptr && m_contentWidget != nullptr) {
        bodyLayout->replaceWidget(m_contentPlaceholder, m_contentWidget);
        m_contentPlaceholder->deleteLater();
        m_contentWidget->setParent(m_bodyWidget);
    }

    connect(m_toggleButton, &QPushButton::clicked, this, &Sidebar::toggleCollapsed);
    connect(m_animation, &QPropertyAnimation::valueChanged, this, [this](const QVariant &value) {
        setMinimumWidth(value.toInt());
    });
}

Sidebar::~Sidebar() = default;

bool Sidebar::isCollapsed() const {
    return m_collapsed;
}

QWidget *Sidebar::contentWidget() const {
    return m_contentWidget;
}

void Sidebar::setBodyWidth(int width) {
    if (width <= 0 || m_bodyWidth == width) {
        return;
    }

    m_bodyWidth = width;
    m_bodyWidget->setFixedWidth(m_bodyWidth);
    if (!m_collapsed) {
        applyExpandedWidth();
    }
}

void Sidebar::toggleCollapsed() {
    setCollapsed(!m_collapsed);
}

void Sidebar::setCollapsed(bool collapsed) {
    if (m_collapsed == collapsed) {
        return;
    }

    const int startWidth = maximumWidth();
    const int targetWidth = collapsed ? Style::Size::SidebarHandleWidth
                                      : m_bodyWidth + Style::Size::SidebarHandleWidth;

    m_animation->stop();
    m_animation->disconnect(this);
    connect(m_animation, &QPropertyAnimation::valueChanged, this, [this](const QVariant &value) {
        setMinimumWidth(value.toInt());
    });
    connect(m_animation, &QPropertyAnimation::finished, this, [this, collapsed] {
        if (collapsed) {
            m_bodyWidget->setVisible(false);
        }
    });

    if (!collapsed) {
        m_bodyWidget->setVisible(true);
    }

    m_animation->setStartValue(startWidth);
    m_animation->setEndValue(targetWidth);
    m_animation->start();

    m_collapsed = collapsed;
    m_toggleButton->setText(m_collapsed ? QStringLiteral(">") : QStringLiteral("<"));
    emit collapseStateChanged(m_collapsed);
}

void Sidebar::applyExpandedWidth() {
    const int totalWidth = m_bodyWidth + Style::Size::SidebarHandleWidth;
    setMinimumWidth(totalWidth);
    setMaximumWidth(totalWidth);
}

void Sidebar::setupUi(const QString &title) {
    setObjectName(QStringLiteral("Sidebar"));

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_bodyWidget = new QWidget(this);
    m_bodyWidget->setObjectName(QStringLiteral("bodyWidget"));
    m_bodyWidget->setMinimumSize(QSize(150, 0));
    m_bodyWidget->setMaximumSize(QSize(150, QWIDGETSIZE_MAX));
    auto *bodyLayout = new QVBoxLayout(m_bodyWidget);
    bodyLayout->setObjectName(QStringLiteral("bodyLayout"));
    bodyLayout->setSpacing(0);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(m_bodyWidget);

    m_headerLabel = new QLabel(m_bodyWidget);
    m_headerLabel->setObjectName(QStringLiteral("headerLabel"));
    m_headerLabel->setMinimumSize(QSize(0, 30));
    m_headerLabel->setMaximumSize(QSize(QWIDGETSIZE_MAX, 30));
    m_headerLabel->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);
    m_headerLabel->setText(title);
    bodyLayout->addWidget(m_headerLabel);

    m_contentPlaceholder = new QWidget(m_bodyWidget);
    m_contentPlaceholder->setObjectName(QStringLiteral("contentPlaceholder"));
    bodyLayout->addWidget(m_contentPlaceholder);

    m_toggleButton = new QPushButton(this);
    m_toggleButton->setObjectName(QStringLiteral("toggleButton"));
    m_toggleButton->setMinimumSize(QSize(18, 0));
    m_toggleButton->setMaximumSize(QSize(18, QWIDGETSIZE_MAX));
    m_toggleButton->setText(QStringLiteral("<"));
    rootLayout->addWidget(m_toggleButton);
}
