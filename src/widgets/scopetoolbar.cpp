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
    button->setFixedSize(30, 24);
    button->setStyleSheet(makeToolButtonStyle());
    return button;
}

void ScopeToolBar::setupUi() {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(3);

    m_startButton = createToolButton(QStringLiteral("|>"), tr("Start acquisition"));
    m_stopButton = createToolButton(QStringLiteral("[]"), tr("Stop acquisition"));
    m_zoomInButton = createToolButton(QStringLiteral("+"), tr("Zoom in"));
    m_zoomOutButton = createToolButton(QStringLiteral("-"), tr("Zoom out"));
    m_panLeftButton = createToolButton(QStringLiteral("<<"), tr("Pan left"));
    m_panRightButton = createToolButton(QStringLiteral(">>"), tr("Pan right"));
    m_fitButton = createToolButton(QStringLiteral("F"), tr("Fit waveform"));
    m_overlayButton = createToolButton(QStringLiteral("Ov"), tr("Overlay mode"));
    m_stackedButton = createToolButton(QStringLiteral("St"), tr("Stacked mode"));
    m_exportButton = createToolButton(QStringLiteral("Csv"), tr("Export CSV"));
    m_screenshotButton = createToolButton(QStringLiteral("Cam"), tr("Save screenshot"));
    m_fullScaleButton = createToolButton(QStringLiteral("Fs"), tr("Full scale"));
    m_settingsButton = createToolButton(QStringLiteral("*"), tr("Settings"));

    m_overlayButton->setCheckable(true);
    m_stackedButton->setCheckable(true);
    m_exportButton->setFixedWidth(34);
    m_screenshotButton->setFixedWidth(34);

    m_statusLabel = new QLabel(this);

    auto addDivider = [this, layout]() {
        auto *divider = new QWidget(this);
        divider->setFixedSize(1, 18);
        divider->setStyleSheet(QStringLiteral("background:%1;").arg(Style::Color::ScopeDivider.name()));
        layout->addWidget(divider, 0, Qt::AlignVCenter);
    };

    layout->addWidget(m_startButton);
    layout->addWidget(m_stopButton);
    addDivider();
    layout->addWidget(m_zoomInButton);
    layout->addWidget(m_zoomOutButton);
    layout->addWidget(m_panLeftButton);
    layout->addWidget(m_panRightButton);
    layout->addWidget(m_fitButton);
    layout->addWidget(m_fullScaleButton);
    addDivider();
    layout->addWidget(m_overlayButton);
    layout->addWidget(m_stackedButton);
    addDivider();
    layout->addWidget(m_exportButton);
    layout->addWidget(m_screenshotButton);
    addDivider();
    layout->addWidget(m_settingsButton);
    layout->addStretch();
    layout->addWidget(m_statusLabel, 0, Qt::AlignVCenter);

    setStyleSheet(QStringLiteral("background:%1; border-bottom:1px solid %2;")
                      .arg(Style::Color::ScopeToolBarBackground.name())
                      .arg(Style::Color::ScopeDivider.name()));
}

void ScopeToolBar::connectSignals() {
    connect(m_startButton, &QToolButton::clicked, this, &ScopeToolBar::startRequested);
    connect(m_stopButton, &QToolButton::clicked, this, &ScopeToolBar::stopRequested);
    connect(m_zoomInButton, &QToolButton::clicked, this, &ScopeToolBar::zoomInRequested);
    connect(m_zoomOutButton, &QToolButton::clicked, this, &ScopeToolBar::zoomOutRequested);
    connect(m_panLeftButton, &QToolButton::clicked, this, &ScopeToolBar::panLeftRequested);
    connect(m_panRightButton, &QToolButton::clicked, this, &ScopeToolBar::panRightRequested);
    connect(m_fitButton, &QToolButton::clicked, this, &ScopeToolBar::fitRequested);
    connect(m_exportButton, &QToolButton::clicked, this, &ScopeToolBar::exportCsvRequested);
    connect(m_screenshotButton, &QToolButton::clicked, this, &ScopeToolBar::screenshotRequested);
    connect(m_fullScaleButton, &QToolButton::clicked, this, &ScopeToolBar::fullScaleRequested);
    connect(m_settingsButton, &QToolButton::clicked, this, &ScopeToolBar::settingsRequested);
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
    m_startButton->setEnabled(!m_running);
    m_stopButton->setEnabled(m_running);
    m_overlayButton->setChecked(m_viewMode == ViewMode::Overlay);
    m_stackedButton->setChecked(m_viewMode == ViewMode::Stacked);
    m_statusLabel->setText(m_running ? QStringLiteral("RUNNING") : QStringLiteral("STOPPED"));
    m_statusLabel->setStyleSheet(makeStatusStyle(m_running));
}
