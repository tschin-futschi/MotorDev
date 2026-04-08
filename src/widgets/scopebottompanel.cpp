#include "widgets/scopebottompanel.h"

#include "ui/style_constants.h"
#include "widgets/scopechannelstrip.h"
#include "widgets/scopegeneratorpanel.h"
#include "widgets/scoperegisterpanel.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QToolButton>
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

void ScopeBottomPanel::setRunning(bool running) {
    if (m_running == running) {
        return;
    }

    m_running = running;
    refreshSamplingButton();
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

void ScopeBottomPanel::refreshSamplingButton() {
    if (m_samplingButton == nullptr) {
        return;
    }

    m_samplingButton->setText(m_running ? tr("停止采样") : tr("开始采样"));
}

void ScopeBottomPanel::refreshYAxisButton() {
    if (m_yAxisButton == nullptr) {
        return;
    }

    if (m_yAxisAuto) {
        m_yAxisButton->setText(tr("Y Axis: Auto"));
        return;
    }

    m_yAxisButton->setText(tr("Y Axis: %1 ~ %2")
                               .arg(QString::number(m_manualYMin, 'f', 1))
                               .arg(QString::number(m_manualYMax, 'f', 1)));
}

bool ScopeBottomPanel::promptManualYAxisRange(double &minValue, double &maxValue) {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("设置 Y Axis 范围"));
    dialog.setModal(true);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *formLayout = new QFormLayout;
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(8);

    auto *minEdit = new QLineEdit(QString::number(m_manualYMin, 'f', 1), &dialog);
    auto *maxEdit = new QLineEdit(QString::number(m_manualYMax, 'f', 1), &dialog);
    minEdit->setPlaceholderText(tr("例如 -1000"));
    maxEdit->setPlaceholderText(tr("例如 1000"));
    formLayout->addRow(tr("最小值"), minEdit);
    formLayout->addRow(tr("最大值"), maxEdit);
    layout->addLayout(formLayout);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    bool minOk = false;
    bool maxOk = false;
    const double parsedMin = minEdit->text().trimmed().toDouble(&minOk);
    const double parsedMax = maxEdit->text().trimmed().toDouble(&maxOk);
    if (!minOk || !maxOk || parsedMin >= parsedMax) {
        return false;
    }

    minValue = parsedMin;
    maxValue = parsedMax;
    return true;
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
    channelLayout->setSpacing(6);

    auto *channelHeaderLayout = new QHBoxLayout;
    channelHeaderLayout->setContentsMargins(0, 0, 0, 0);
    channelHeaderLayout->setSpacing(6);

    auto *intervalLabel = new QLabel(tr("Sample Interval"), m_channelFrame);
    intervalLabel->setStyleSheet(makeSectionTitleStyle());
    auto *noteLabel = new QLabel(tr("Capture Note"), m_channelFrame);
    noteLabel->setStyleSheet(makeSectionTitleStyle());

    const QString fieldStyle = QStringLiteral(
        "QComboBox, QLineEdit, QToolButton, QPushButton { background:%1; border:1px solid %2; border-radius:4px; "
        "padding:2px 6px; color:%3; font-size:11px; min-height:28px; }"
        "QToolButton::menu-indicator { subcontrol-position: right center; subcontrol-origin: padding; }")
                                   .arg(Style::Color::White.name(),
                                        Style::Color::ScopeInputBorderAlt.name(),
                                        Style::Color::ScopeTextAlt.name());

    m_intervalCombo = new QComboBox(m_channelFrame);
    m_intervalCombo->addItems({QStringLiteral("100 us"), QStringLiteral("500 us"), QStringLiteral("1000 us"),
                               QStringLiteral("2000 us")});
    m_intervalCombo->setCurrentIndex(2);
    m_intervalCombo->setMinimumWidth(110);
    m_intervalCombo->setStyleSheet(fieldStyle);

    m_samplingButton = new QPushButton(m_channelFrame);
    m_samplingButton->setMinimumWidth(96);
    m_samplingButton->setStyleSheet(fieldStyle);

    m_yAxisButton = new QToolButton(m_channelFrame);
    m_yAxisButton->setPopupMode(QToolButton::InstantPopup);
    m_yAxisButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_yAxisButton->setMinimumWidth(132);
    m_yAxisButton->setStyleSheet(fieldStyle);

    m_yAxisMenu = new QMenu(m_yAxisButton);
    m_yAxisMenu->setStyleSheet(QStringLiteral("QMenu { background:%1; color:%2; border:1px solid %3; }"
                                              "QMenu::item:selected { background:%4; }")
                                   .arg(Style::Color::White.name(),
                                        Style::Color::ScopeTextAlt.name(),
                                        Style::Color::ScopeInputBorderAlt.name(),
                                        Style::Color::ScopePanelBackground.name()));
    m_yAxisMenu->addAction(tr("Auto"));
    m_yAxisMenu->addAction(tr("Manual..."));
    m_yAxisButton->setMenu(m_yAxisMenu);

    m_noteEdit = new QLineEdit(m_channelFrame);
    m_noteEdit->setPlaceholderText(tr("Add capture note"));
    m_noteEdit->setMinimumWidth(220);
    m_noteEdit->setStyleSheet(fieldStyle);

    channelHeaderLayout->addWidget(intervalLabel);
    channelHeaderLayout->addWidget(m_intervalCombo);
    channelHeaderLayout->addWidget(m_yAxisButton);
    channelHeaderLayout->addWidget(m_samplingButton);
    channelHeaderLayout->addWidget(noteLabel);
    channelHeaderLayout->addWidget(m_noteEdit, 1);
    channelLayout->addLayout(channelHeaderLayout);

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

    refreshSamplingButton();
    refreshYAxisButton();

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
    connect(m_intervalCombo, &QComboBox::currentTextChanged, this, &ScopeBottomPanel::sampleIntervalChanged);
    connect(m_noteEdit, &QLineEdit::textChanged, this, &ScopeBottomPanel::captureNoteChanged);
    connect(m_samplingButton, &QPushButton::clicked, this, [this]() {
        m_running = !m_running;
        refreshSamplingButton();
        emit runningToggled(m_running);
    });
    connect(m_yAxisMenu, &QMenu::triggered, this, [this](QAction *action) {
        if (action == nullptr) {
            return;
        }

        if (action->text() == tr("Auto")) {
            m_yAxisAuto = true;
            refreshYAxisButton();
            emit yAxisAutoRequested();
            return;
        }

        double minValue = m_manualYMin;
        double maxValue = m_manualYMax;
        if (!promptManualYAxisRange(minValue, maxValue)) {
            return;
        }

        m_yAxisAuto = false;
        m_manualYMin = minValue;
        m_manualYMax = maxValue;
        refreshYAxisButton();
        emit yAxisManualRequested(minValue, maxValue);
    });

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
