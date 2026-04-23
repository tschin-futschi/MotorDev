#include "widgets/scopebottompanel.h"

#include "ui/style_constants.h"
#include "widgets/scopechannelstrip.h"
#include "widgets/scopegeneratorpanel.h"
#include "widgets/scopemarqueelabel.h"
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
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

using namespace MotorDev;

ScopeBottomPanel::ScopeBottomPanel(QWidget *overlayHost, QWidget *parent)
    : QWidget(parent)
    , m_overlayHost(overlayHost) {
    setupUi();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_channelFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    if (auto *channelHeaderLayout = qobject_cast<QHBoxLayout *>(m_channelFrame->layout()->itemAt(0)->layout())) {
        channelHeaderLayout->setStretch(channelHeaderLayout->indexOf(m_noteEdit), 1);
    }

    auto *stripRowLayout = qobject_cast<QHBoxLayout *>(m_channelStripRow->layout());
    for (int i = 0; i < 8; ++i) {
        m_channels[i] = new ScopeChannelStrip(i, m_channelStripRow);
        stripRowLayout->addWidget(m_channels[i], 1);
    }

    // Panels are created without parent — ownership is transferred to
    // the overlay QWidget via layout->addWidget() in createOverlayWindow().
    // The overlay windows are top-level Qt::Tool widgets, manually
    // deleted in ~ScopeBottomPanel().
    m_registerPanel = new ScopeRegisterPanel(nullptr);
    m_generatorPanel = new ScopeGeneratorPanel(nullptr);
    m_registerWindow = createOverlayWindow(tr("Register R/W"), m_registerPanel, QSize(500, 400));
    m_generatorWindow = createOverlayWindow(tr("Wave Generator"), m_generatorPanel, QSize(420, 340));

    m_yAxisMenu = new QMenu(m_yAxisButton);
    m_yAxisMenu->addAction(tr("Auto"));
    m_yAxisMenu->addAction(tr("Manual..."));
    m_yAxisButton->setMenu(m_yAxisMenu);

    m_intervalCombo->setCurrentIndex(4);
    m_windowCombo->setCurrentIndex(0);

    connectSignals();
    refreshPanels();
    refreshYAxisButton();
}

ScopeBottomPanel::~ScopeBottomPanel() {
    delete m_registerWindow;
    m_registerWindow = nullptr;
    delete m_generatorWindow;
    m_generatorWindow = nullptr;
}

ScopeRegisterPanel *ScopeBottomPanel::registerPanel() const {
    return m_registerPanel;
}

ScopeGeneratorPanel *ScopeBottomPanel::generatorPanel() const {
    return m_generatorPanel;
}

ScopeMarqueeLabel *ScopeBottomPanel::marqueeLabel() const {
    return m_marqueeLabel;
}

void ScopeBottomPanel::setRunning(bool running) {
    if (m_running == running) {
        return;
    }

    m_running = running;
    const bool configEditable = !running;
    m_intervalCombo->setEnabled(configEditable);
    m_windowCombo->setEnabled(configEditable);
    for (ScopeChannelStrip *channel : m_channels) {
        if (channel != nullptr) {
            channel->setEnabled(configEditable);
        }
    }
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
    window->setProperty("scopeOverlayWindow", true);
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
    setMinimumHeight(m_channelsVisible ? Style::Size::ScopeBottomPanelMinExpanded
                                       : Style::Size::ScopeBottomPanelMinCollapsed);
    setMaximumHeight(m_channelsVisible ? Style::Size::ScopeBottomPanelMaxExpanded
                                       : Style::Size::ScopeBottomPanelMinCollapsed);

    m_channelsToggleButton->setText(m_channelsVisible ? tr("Hide Channels") : tr("Show Channels"));
    m_registerToggleButton->setText(m_registerWindow->isVisible() ? tr("Hide Register") : tr("Show Register"));
    m_generatorToggleButton->setText(m_generatorWindow->isVisible() ? tr("Hide Generator") : tr("Show Generator"));

    updateGeometry();
}

void ScopeBottomPanel::refreshYAxisButton() {
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

void ScopeBottomPanel::connectSignals() {
    for (ScopeChannelStrip *channel : m_channels) {
        connect(channel, &ScopeChannelStrip::channelToggled, this, &ScopeBottomPanel::channelToggled);
        connect(channel, &ScopeChannelStrip::descriptionChanged, this, &ScopeBottomPanel::channelDescriptionChanged);
        connect(channel, &ScopeChannelStrip::addressChanged, this, &ScopeBottomPanel::channelAddressChanged);
    }

    connect(m_registerPanel, &ScopeRegisterPanel::readRequested, this, &ScopeBottomPanel::registerReadRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::writeRequested, this, &ScopeBottomPanel::registerWriteRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::startRequested, this, &ScopeBottomPanel::registerStartRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::stopRequested, this, &ScopeBottomPanel::registerStopRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::clearPanelRequested, this, &ScopeBottomPanel::clearPanelRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::loadParamsRequested, this, &ScopeBottomPanel::loadParamsRequested);
    connect(m_generatorPanel, &ScopeGeneratorPanel::linearStartRequested, this, &ScopeBottomPanel::generatorLinearStartRequested);
    connect(m_generatorPanel, &ScopeGeneratorPanel::cosineStartRequested, this, &ScopeBottomPanel::generatorCosineStartRequested);
    connect(m_generatorPanel, &ScopeGeneratorPanel::stopRequested, this, &ScopeBottomPanel::generatorStopRequested);
    connect(m_intervalCombo, &QComboBox::currentTextChanged, this, &ScopeBottomPanel::sampleIntervalChanged);
    connect(m_windowCombo, &QComboBox::currentTextChanged, this, &ScopeBottomPanel::displayWindowChanged);
    connect(m_noteEdit, &QLineEdit::textChanged, this, &ScopeBottomPanel::captureNoteChanged);
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

void ScopeBottomPanel::setupUi() {
    setObjectName(QStringLiteral("ScopeBottomPanel"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(6);
    rootLayout->setContentsMargins(8, 6, 8, 6);

    m_channelFrame = new QWidget(this);
    m_channelFrame->setObjectName(QStringLiteral("channelFrame"));
    auto *channelLayout = new QVBoxLayout(m_channelFrame);
    channelLayout->setObjectName(QStringLiteral("channelLayout"));
    channelLayout->setSpacing(6);
    channelLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->addWidget(m_channelFrame);

    auto *channelHeaderLayout = new QHBoxLayout();
    channelHeaderLayout->setObjectName(QStringLiteral("channelHeaderLayout"));
    channelHeaderLayout->setSpacing(6);
    channelLayout->addLayout(channelHeaderLayout);

    auto *intervalLabel = new QLabel(m_channelFrame);
    intervalLabel->setObjectName(QStringLiteral("intervalLabel"));
    intervalLabel->setText(QStringLiteral("Sample Interval"));
    channelHeaderLayout->addWidget(intervalLabel);

    m_intervalCombo = new QComboBox(m_channelFrame);
    m_intervalCombo->setObjectName(QStringLiteral("intervalCombo"));
    m_intervalCombo->setMinimumSize(QSize(110, 0));
    m_intervalCombo->addItems({QStringLiteral("200 us"), QStringLiteral("300 us"), QStringLiteral("500 us"), QStringLiteral("750 us"),
                               QStringLiteral("1000 us"), QStringLiteral("1500 us"), QStringLiteral("2000 us")});
    channelHeaderLayout->addWidget(m_intervalCombo);

    m_yAxisButton = new QToolButton(m_channelFrame);
    m_yAxisButton->setObjectName(QStringLiteral("yAxisButton"));
    m_yAxisButton->setMinimumSize(QSize(132, 0));
    m_yAxisButton->setPopupMode(QToolButton::InstantPopup);
    m_yAxisButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_yAxisButton->setText(QStringLiteral("Y Axis: Auto"));
    channelHeaderLayout->addWidget(m_yAxisButton);

    auto *windowLabel = new QLabel(m_channelFrame);
    windowLabel->setObjectName(QStringLiteral("windowLabel"));
    windowLabel->setText(QStringLiteral("Display Window"));
    channelHeaderLayout->addWidget(windowLabel);

    m_windowCombo = new QComboBox(m_channelFrame);
    m_windowCombo->setObjectName(QStringLiteral("windowCombo"));
    m_windowCombo->setMinimumSize(QSize(110, 0));
    m_windowCombo->addItems({QStringLiteral("50 ms"), QStringLiteral("200 ms"), QStringLiteral("500 ms"),
                             QStringLiteral("1000 ms"), QStringLiteral("2000 ms"), QStringLiteral("4000 ms")});
    channelHeaderLayout->addWidget(m_windowCombo);

    auto *noteLabel = new QLabel(m_channelFrame);
    noteLabel->setObjectName(QStringLiteral("noteLabel"));
    noteLabel->setText(QStringLiteral("Capture Note"));
    channelHeaderLayout->addWidget(noteLabel);

    m_noteEdit = new QLineEdit(m_channelFrame);
    m_noteEdit->setObjectName(QStringLiteral("noteEdit"));
    m_noteEdit->setMinimumSize(QSize(220, 0));
    m_noteEdit->setPlaceholderText(QStringLiteral("Add capture note"));
    channelHeaderLayout->addWidget(m_noteEdit);

    m_channelStripRow = new QWidget(m_channelFrame);
    m_channelStripRow->setObjectName(QStringLiteral("channelStripRow"));
    auto *channelStripRowLayout = new QHBoxLayout(m_channelStripRow);
    channelStripRowLayout->setObjectName(QStringLiteral("channelStripRowLayout"));
    channelStripRowLayout->setSpacing(4);
    channelStripRowLayout->setContentsMargins(0, 0, 0, 0);
    channelLayout->addWidget(m_channelStripRow);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setObjectName(QStringLiteral("buttonLayout"));
    buttonLayout->setSpacing(6);
    rootLayout->addLayout(buttonLayout);
    buttonLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_channelsToggleButton = new QPushButton(this);
    m_channelsToggleButton->setObjectName(QStringLiteral("channelsToggleButton"));
    m_channelsToggleButton->setProperty("buttonRole", QStringLiteral("toggle"));
    m_channelsToggleButton->setText(QStringLiteral("Hide Channels"));
    buttonLayout->addWidget(m_channelsToggleButton);

    m_registerToggleButton = new QPushButton(this);
    m_registerToggleButton->setObjectName(QStringLiteral("registerToggleButton"));
    m_registerToggleButton->setProperty("buttonRole", QStringLiteral("toggle"));
    m_registerToggleButton->setText(QStringLiteral("Show Register"));
    buttonLayout->addWidget(m_registerToggleButton);

    m_generatorToggleButton = new QPushButton(this);
    m_generatorToggleButton->setObjectName(QStringLiteral("generatorToggleButton"));
    m_generatorToggleButton->setProperty("buttonRole", QStringLiteral("toggle"));
    m_generatorToggleButton->setText(QStringLiteral("Show Generator"));
    buttonLayout->addWidget(m_generatorToggleButton);

    m_marqueeLabel = new ScopeMarqueeLabel(this);
    m_marqueeLabel->setObjectName(QStringLiteral("marqueeLabel"));
    m_marqueeLabel->setText(QStringLiteral("Idle"));
    buttonLayout->addWidget(m_marqueeLabel, 1);
}
