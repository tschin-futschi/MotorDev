#include "widgets/scopebottompanel.h"

#include "ui/style_constants.h"
#include "widgets/scopechannelstrip.h"
#include "widgets/scopegeneratorpanel.h"
#include "widgets/scoperegisterpanel.h"

#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>

using namespace MotorDev;

namespace {
QString makeSectionTitleStyle() {
    return QStringLiteral("QLabel { color:%1; font-size:12px; font-weight:600; }")
        .arg(Style::Color::ScopeTextSecondary.name());
}

QString makeSectionFrameStyle() {
    return QStringLiteral("background:%1; border:1px solid %2; border-radius:8px;")
        .arg(Style::Color::ScopeSectionBackground.name(), Style::Color::ScopeSectionBorder.name());
}

QString makeToggleButtonStyle() {
    return QStringLiteral(
               "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:4px; "
               "padding:2px 8px; font-size:11px; }"
               "QPushButton:hover { background:%4; border-color:%5; }")
        .arg(Style::Color::ScopeToggleBackground.name(),
             Style::Color::ScopeToggleText.name(),
             Style::Color::ScopeToggleBorder.name(),
             Style::Color::ScopeToggleHover.name(),
             Style::Color::ScopeToggleHoverBorder.name());
}

QString makeOverlayStyle() {
    return QStringLiteral("background:%1; border:1px solid %2; border-radius:8px;")
        .arg(Style::Color::ScopeSectionBackground.name(), Style::Color::ScopeOverlayBorder.name());
}
} // namespace

ScopeBottomPanel::ScopeBottomPanel(QWidget *overlayHost, QWidget *parent)
    : QWidget(parent)
    , m_overlayHost(overlayHost) {
    setupUi();
    connectSignals();
    refreshPanels();
}

ScopeBottomPanel::~ScopeBottomPanel() {
    delete m_registerWindow;
    m_registerWindow = nullptr;
    delete m_generatorWindow;
    m_generatorWindow = nullptr;
}

bool ScopeBottomPanel::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_registerWindow || watched == m_generatorWindow) {
        auto clampToScreen = [this](QWidget *window) {
            if (window == nullptr) {
                return;
            }

            QScreen *screen = window->screen();
            if (screen == nullptr && m_overlayHost != nullptr && m_overlayHost->windowHandle() != nullptr) {
                screen = m_overlayHost->windowHandle()->screen();
            }
            if (screen == nullptr) {
                screen = QGuiApplication::primaryScreen();
            }
            if (screen == nullptr) {
                return;
            }

            const QRect available = screen->availableGeometry();
            const int maxX = qMax(available.left(), available.right() - window->width() + 1);
            const int maxY = qMax(available.top(), available.bottom() - window->height() + 1);
            const int x = qBound(available.left(), window->x(), maxX);
            const int y = qBound(available.top(), window->y(), maxY);
            if (x != window->x() || y != window->y()) {
                window->move(x, y);
            }
        };

        if (event->type() == QEvent::Move || event->type() == QEvent::Show) {
            clampToScreen(static_cast<QWidget *>(watched));
        }

        if (event->type() == QEvent::Show || event->type() == QEvent::Hide || event->type() == QEvent::Close) {
            refreshPanels();
        }
    }

    return QWidget::eventFilter(watched, event);
}

QWidget *ScopeBottomPanel::createOverlayWindow(const QString &title, QWidget *content, const QSize &size) {
    auto *window = new QWidget(nullptr, Qt::Tool);
    window->setWindowTitle(title);
    window->setStyleSheet(makeOverlayStyle());
    window->setAttribute(Qt::WA_DeleteOnClose, false);
    window->hide();
    window->installEventFilter(this);

    auto *layout = new QVBoxLayout(window);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    layout->addWidget(content);

    window->resize(size);
    if (m_overlayHost != nullptr) {
        const QPoint center = m_overlayHost->mapToGlobal(m_overlayHost->rect().center());
        window->move(center.x() - window->width() / 2, center.y() - window->height() / 2);
    }
    return window;
}

void ScopeBottomPanel::refreshPanels() {
    m_channelFrame->setVisible(m_channelsVisible);
    setMinimumHeight(m_channelsVisible ? 132 : 40);
    setMaximumHeight(m_channelsVisible ? QWIDGETSIZE_MAX : 40);

    m_channelsToggleButton->setText(m_channelsVisible ? tr("Hide Channels") : tr("Show Channels"));
    m_registerToggleButton->setText(m_registerWindow->isVisible() ? tr("Hide Register") : tr("Show Register"));
    m_generatorToggleButton->setText(m_generatorWindow->isVisible() ? tr("Hide Generator") : tr("Show Generator"));

    updateGeometry();
}

void ScopeBottomPanel::setupUi() {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 6, 8, 6);
    rootLayout->setSpacing(6);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6);
    m_channelsToggleButton = new QPushButton(this);
    m_registerToggleButton = new QPushButton(this);
    m_generatorToggleButton = new QPushButton(this);

    const auto buttons = {m_channelsToggleButton, m_registerToggleButton, m_generatorToggleButton};
    for (QPushButton *button : buttons) {
        button->setStyleSheet(makeToggleButtonStyle());
    }

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_channelsToggleButton);
    buttonLayout->addWidget(m_registerToggleButton);
    buttonLayout->addWidget(m_generatorToggleButton);

    m_channelFrame = new QWidget(this);
    m_channelFrame->setStyleSheet(makeSectionFrameStyle());
    m_channelFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *channelLayout = new QVBoxLayout(m_channelFrame);
    channelLayout->setContentsMargins(8, 8, 8, 8);
    channelLayout->setSpacing(4);

    auto *channelTitle = new QLabel(tr("Channels"), m_channelFrame);
    channelTitle->setStyleSheet(makeSectionTitleStyle());
    channelLayout->addWidget(channelTitle);

    auto *stripRow = new QHBoxLayout;
    stripRow->setContentsMargins(0, 0, 0, 0);
    stripRow->setSpacing(4);
    for (int i = 0; i < 8; ++i) {
        m_channels[i] = new ScopeChannelStrip(i, m_channelFrame);
        stripRow->addWidget(m_channels[i], 1);
    }
    channelLayout->addLayout(stripRow);

    m_registerPanel = new ScopeRegisterPanel;
    m_generatorPanel = new ScopeGeneratorPanel;
    m_registerWindow = createOverlayWindow(tr("Register R/W"), m_registerPanel, QSize(500, 400));
    m_generatorWindow = createOverlayWindow(tr("Wave Generator"), m_generatorPanel, QSize(420, 240));

    rootLayout->addWidget(m_channelFrame);
    rootLayout->addLayout(buttonLayout);

    setStyleSheet(QStringLiteral("background:%1; border-top:1px solid %2;")
                      .arg(Style::Color::ScopeBottomBackground.name())
                      .arg(Style::Color::ScopeDivider.name()));
}

void ScopeBottomPanel::connectSignals() {
    for (ScopeChannelStrip *channel : m_channels) {
        connect(channel, &ScopeChannelStrip::channelToggled, this, &ScopeBottomPanel::channelToggled);
        connect(channel, &ScopeChannelStrip::descriptionChanged, this, &ScopeBottomPanel::channelDescriptionChanged);
        connect(channel, &ScopeChannelStrip::addressChanged, this, &ScopeBottomPanel::channelAddressChanged);
    }

    connect(m_registerPanel, &ScopeRegisterPanel::readRequested, this, &ScopeBottomPanel::registerReadRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::writeRequested, this, &ScopeBottomPanel::registerWriteRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::clearPanelRequested, this, &ScopeBottomPanel::clearPanelRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::loadParamsRequested, this, &ScopeBottomPanel::loadParamsRequested);

    connect(m_channelsToggleButton, &QPushButton::clicked, this, [this]() {
        m_channelsVisible = !m_channelsVisible;
        refreshPanels();
    });
    connect(m_registerToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !m_registerWindow->isVisible();
        m_registerWindow->setVisible(visible);
        if (visible) {
            m_registerWindow->raise();
            m_registerWindow->activateWindow();
        }
        refreshPanels();
    });
    connect(m_generatorToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !m_generatorWindow->isVisible();
        m_generatorWindow->setVisible(visible);
        if (visible) {
            m_generatorWindow->raise();
            m_generatorWindow->activateWindow();
        }
        refreshPanels();
    });
}
