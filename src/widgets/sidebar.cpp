#include "widgets/sidebar.h"

#include "ui/style_constants.h"
#include "ui_sidebar.h"

#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>
#include <utility>

using namespace MotorDev;

Sidebar::Sidebar(const QString &title, QWidget *contentWidget, QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::Sidebar>()) {
    ui->setupUi(this);
    ui->headerLabel->setText(title);
    ui->headerLabel->setContentsMargins(Style::Size::SidebarContentHorizontalPadding, 0,
                                        Style::Size::SidebarContentHorizontalPadding, 0);

    m_bodyWidth = Style::Size::SidebarWidth;
    setMinimumWidth(Style::Size::SidebarTotalWidth);
    setMaximumWidth(Style::Size::SidebarTotalWidth);

    m_animation = new QPropertyAnimation(this, "maximumWidth", this);
    m_animation->setDuration(Style::Size::SidebarAnimationMs);

    auto *bodyLayout = qobject_cast<QVBoxLayout *>(ui->bodyWidget->layout());
    m_contentWidget = contentWidget;
    if (bodyLayout != nullptr && m_contentWidget != nullptr) {
        bodyLayout->replaceWidget(ui->contentPlaceholder, m_contentWidget);
        ui->contentPlaceholder->deleteLater();
        m_contentWidget->setParent(ui->bodyWidget);
    }

    connect(ui->toggleButton, &QPushButton::clicked, this, &Sidebar::toggleCollapsed);
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
    ui->bodyWidget->setFixedWidth(m_bodyWidth);
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
            ui->bodyWidget->setVisible(false);
        }
    });

    if (!collapsed) {
        ui->bodyWidget->setVisible(true);
    }

    m_animation->setStartValue(startWidth);
    m_animation->setEndValue(targetWidth);
    m_animation->start();

    m_collapsed = collapsed;
    ui->toggleButton->setText(m_collapsed ? QStringLiteral(">") : QStringLiteral("<"));
    emit collapseStateChanged(m_collapsed);
}

void Sidebar::applyExpandedWidth() {
    const int totalWidth = m_bodyWidth + Style::Size::SidebarHandleWidth;
    setMinimumWidth(totalWidth);
    setMaximumWidth(totalWidth);
}
