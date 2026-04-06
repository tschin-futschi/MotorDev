#include "tabs/oscilloscoptab.h"

#include "ui/style_constants.h"
#include "widgets/scopebottompanel.h"
#include "widgets/scopeplotwidget.h"
#include "widgets/scopetoolbar.h"
#include "widgets/sidebar.h"

#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

using namespace MotorDev;

OscilloscopTab::OscilloscopTab(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
}

void OscilloscopTab::setupUi() {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_sidebar = new Sidebar(tr("示波"), createSidebarContent(), this);
    m_sidebar->setBodyWidth(224);

    auto *mainContent = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(mainContent);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_toolBar = new ScopeToolBar(this);
    m_plotWidget = new ScopePlotWidget(this);
    m_bottomPanel = new ScopeBottomPanel(mainContent, this);

    mainLayout->addWidget(m_toolBar);
    mainLayout->addWidget(m_plotWidget, 1);
    mainLayout->addWidget(m_bottomPanel);

    layout->addWidget(m_sidebar);
    layout->addWidget(mainContent, 1);

    setStyleSheet(QStringLiteral("background:%1;").arg(Style::Color::ScopeBackground.name()));

    m_sidebar->setCollapsed(true);
}

void OscilloscopTab::connectSignals() {
    connect(m_toolBar, &ScopeToolBar::startRequested, this, [this]() {
        m_running = true;
        m_toolBar->setRunning(true);
        m_plotWidget->setRunning(true);
        logPlaceholderAction(QStringLiteral("Start"));
    });
    connect(m_toolBar, &ScopeToolBar::stopRequested, this, [this]() {
        m_running = false;
        m_toolBar->setRunning(false);
        m_plotWidget->setRunning(false);
        logPlaceholderAction(QStringLiteral("Stop"));
    });
    connect(m_toolBar, &ScopeToolBar::viewModeChanged, this, [this](ScopeToolBar::ViewMode mode) {
        m_viewMode = mode;
        m_plotWidget->setViewMode(mode);
        logPlaceholderAction(mode == ScopeToolBar::ViewMode::Overlay
                                 ? QStringLiteral("Switch to Overlay")
                                 : QStringLiteral("Switch to Stacked"));
    });
    connect(m_toolBar, &ScopeToolBar::zoomInRequested, this, [this]() { logPlaceholderAction(QStringLiteral("Zoom In")); });
    connect(m_toolBar, &ScopeToolBar::zoomOutRequested, this, [this]() { logPlaceholderAction(QStringLiteral("Zoom Out")); });
    connect(m_toolBar, &ScopeToolBar::panLeftRequested, this, [this]() { logPlaceholderAction(QStringLiteral("Pan Left")); });
    connect(m_toolBar, &ScopeToolBar::panRightRequested, this, [this]() { logPlaceholderAction(QStringLiteral("Pan Right")); });
    connect(m_toolBar, &ScopeToolBar::fitRequested, this, [this]() { logPlaceholderAction(QStringLiteral("Fit")); });
    connect(m_toolBar, &ScopeToolBar::exportCsvRequested, this, [this]() { logPlaceholderAction(QStringLiteral("Export CSV")); });
    connect(m_toolBar, &ScopeToolBar::screenshotRequested, this, [this]() { logPlaceholderAction(QStringLiteral("Screenshot")); });
    connect(m_toolBar, &ScopeToolBar::fullScaleRequested, this, [this]() { logPlaceholderAction(QStringLiteral("Fs")); });
    connect(m_toolBar, &ScopeToolBar::settingsRequested, this, [this]() { logPlaceholderAction(QStringLiteral("Settings")); });

    connect(m_bottomPanel, &ScopeBottomPanel::registerReadRequested, this, [this](int row) {
        logPlaceholderAction(QStringLiteral("Register R row %1").arg(row + 1));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::registerWriteRequested, this, [this](int row) {
        logPlaceholderAction(QStringLiteral("Register W row %1").arg(row + 1));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::clearPanelRequested, this, [this]() {
        logPlaceholderAction(QStringLiteral("Clear register panel"));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::loadParamsRequested, this, [this]() {
        logPlaceholderAction(QStringLiteral("Load parameters"));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::channelToggled, this, [this](int index, bool enabled) {
        logPlaceholderAction(QStringLiteral("Channel %1 %2")
                                 .arg(index + 1)
                                 .arg(enabled ? QStringLiteral("enabled") : QStringLiteral("disabled")));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::channelDescriptionChanged, this, [this](int index, const QString &text) {
        logPlaceholderAction(QStringLiteral("Channel %1 desc=%2").arg(index + 1).arg(text));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::channelAddressChanged, this, [this](int index, const QString &text) {
        logPlaceholderAction(QStringLiteral("Channel %1 addr=%2").arg(index + 1).arg(text));
    });
}

void OscilloscopTab::logPlaceholderAction(const QString &action) {
    qDebug().noquote() << QStringLiteral("[Scope GUI] %1").arg(action);
}

QWidget *OscilloscopTab::createSidebarContent() {
    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto makeLabel = [content](const QString &text) {
        auto *label = new QLabel(text, content);
        label->setStyleSheet(QStringLiteral("color:%1; font-size:11px; font-weight:600;")
                                 .arg(Style::Color::ScopeTextLabel.name()));
        return label;
    };

    auto makeField = [content](const QString &placeholder) {
        auto *edit = new QLineEdit(content);
        edit->setPlaceholderText(placeholder);
        edit->setMinimumHeight(24);
        edit->setStyleSheet(QStringLiteral(
            "QLineEdit { background:%1; border:1px solid %2; border-radius:4px; "
            "padding:2px 5px; color:%3; font-size:11px; }")
                                .arg(Style::Color::White.name(),
                                     Style::Color::ScopeInputBorderAlt.name(),
                                     Style::Color::ScopeTextAlt.name()));
        return edit;
    };

    auto *intervalCombo = new QComboBox(content);
    intervalCombo->addItems({QStringLiteral("100 us"), QStringLiteral("500 us"), QStringLiteral("1000 us"),
                             QStringLiteral("2000 us")});
    intervalCombo->setCurrentIndex(2);
    intervalCombo->setMinimumHeight(24);
    intervalCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background:%1; border:1px solid %2; border-radius:4px; "
        "padding:2px 5px; color:%3; font-size:11px; }")
                                    .arg(Style::Color::White.name(),
                                         Style::Color::ScopeInputBorderAlt.name(),
                                         Style::Color::ScopeTextAlt.name()));

    auto *triggerCombo = new QComboBox(content);
    triggerCombo->addItems({tr("Auto"), tr("Normal"), tr("Single")});
    triggerCombo->setMinimumHeight(24);
    triggerCombo->setStyleSheet(intervalCombo->styleSheet());

    auto *noteLabel = new QLabel(tr("Sidebar reserved for future sampling channel properties."), content);
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                 .arg(Style::Color::ScopeTextDim.name()));

    layout->addWidget(makeLabel(tr("Sample Interval")));
    layout->addWidget(intervalCombo);
    layout->addWidget(makeLabel(tr("Trigger Mode")));
    layout->addWidget(triggerCombo);
    layout->addWidget(makeLabel(tr("Capture Note")));
    layout->addWidget(makeField(tr("Placeholder")));
    layout->addWidget(noteLabel);
    layout->addStretch();

    return content;
}
