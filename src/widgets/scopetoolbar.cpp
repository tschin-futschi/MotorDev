#include "widgets/scopetoolbar.h"

#include "ui/style_constants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

using namespace MotorDev;

namespace {
QString makeToolButtonStyle() {
    return QStringLiteral(
               "QToolButton {"
               "background:%1; border:1px solid %2; border-radius:4px; color:%3;"
               "padding:1px 4px; font-size:10px; min-height:22px;"
               "}"
               "QToolButton:hover { background:%4; border-color:%5; }"
               "QToolButton:pressed { background:%6; padding-top:3px; padding-bottom:1px; }"
               "QToolButton:checked { background:%7; border-color:%8; color:%9; }"
               "QToolButton:disabled { background:%10; border-color:%11; color:%12; }")
        .arg(Style::Color::ScopeToolButtonBackground.name(),
             Style::Color::ScopeToolButtonBorder.name(),
             Style::Color::ScopeText.name(),
             Style::Color::ScopeToolButtonHover.name(),
             Style::Color::ScopeToolButtonHoverBorder.name(),
             Style::Color::ScopeToolButtonPressed.name(),
             Style::Color::ScopeToolButtonChecked.name(),
             Style::Color::ScopeToolButtonCheckedBorder.name(),
             Style::Color::ScopeToolButtonCheckedText.name(),
             Style::Color::ScopeToolButtonDisabled.name(),
             Style::Color::ScopeToolButtonDisabledBorder.name(),
             Style::Color::ScopeToolButtonDisabledText.name());
}

QString makeStatusStyle(bool running) {
    return running
               ? QStringLiteral("QLabel { background:%1; color:%2; border:1px solid %3; "
                                "border-radius:10px; padding:2px 10px; font-size:11px; font-weight:600; }")
                     .arg(Style::Color::ScopeStatusRunningBackground.name(),
                          Style::Color::ScopeStatusRunningText.name(),
                          Style::Color::ScopeStatusRunningBorder.name())
               : QStringLiteral("QLabel { background:%1; color:%2; border:1px solid %3; "
                                "border-radius:10px; padding:2px 10px; font-size:11px; font-weight:600; }")
                     .arg(Style::Color::ScopeStatusStoppedBackground.name(),
                          Style::Color::ScopeStatusStoppedText.name(),
                          Style::Color::ScopeStatusStoppedBorder.name());
}
} // namespace

ScopeToolBar::ScopeToolBar(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
    refreshState();
}

ScopeToolBar::ViewMode ScopeToolBar::viewMode() const {
    return m_viewMode;
}

void ScopeToolBar::setRunning(bool running) {
    if (m_running == running) {
        return;
    }
    m_running = running;
    refreshState();
}

void ScopeToolBar::setViewMode(ScopeToolBar::ViewMode mode) {
    if (m_viewMode == mode) {
        refreshState();
        return;
    }
    m_viewMode = mode;
    refreshState();
}

QToolButton *ScopeToolBar::createToolButton(const QString &text, const QString &toolTip) {
    auto *button = new QToolButton(this);
    button->setText(text);
    button->setToolTip(toolTip);
    button->setAutoRaise(false);
    button->setFixedHeight(26);
    button->setStyleSheet(makeToolButtonStyle());
    return button;
}

void ScopeToolBar::setupUi() {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);

    m_overlayButton = createToolButton(QStringLiteral("Overlay"), tr("Overlay mode"));
    m_stackedButton = createToolButton(QStringLiteral("Stack"), tr("Stacked mode"));

    m_overlayButton->setCheckable(true);
    m_stackedButton->setCheckable(true);
    m_overlayButton->setMinimumWidth(62);
    m_stackedButton->setMinimumWidth(58);

    m_statusLabel = new QLabel(this);
    layout->addWidget(m_overlayButton);
    layout->addWidget(m_stackedButton);
    layout->addStretch();
    layout->addWidget(m_statusLabel, 0, Qt::AlignVCenter);

    setStyleSheet(QStringLiteral("background:%1; border-bottom:1px solid %2;")
                      .arg(Style::Color::ScopeToolBarBackground.name())
                      .arg(Style::Color::ScopeDivider.name()));
}

void ScopeToolBar::connectSignals() {
    connect(m_overlayButton, &QToolButton::clicked, this, [this]() {
        setViewMode(ViewMode::Overlay);
        emit viewModeChanged(m_viewMode);
    });
    connect(m_stackedButton, &QToolButton::clicked, this, [this]() {
        setViewMode(ViewMode::Stacked);
        emit viewModeChanged(m_viewMode);
    });
}

void ScopeToolBar::refreshState() {
    m_overlayButton->setChecked(m_viewMode == ViewMode::Overlay);
    m_stackedButton->setChecked(m_viewMode == ViewMode::Stacked);
    m_statusLabel->setText(m_running ? QStringLiteral("RUNNING") : QStringLiteral("STOPPED"));
    m_statusLabel->setStyleSheet(makeStatusStyle(m_running));
}
