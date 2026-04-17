#include "widgets/scopebottompanel.h"

#include "ui_scopebottompanel.h"
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
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWindow>
#include <utility>

ScopeBottomPanel::ScopeBottomPanel(QWidget *overlayHost, QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ScopeBottomPanel>())
    , m_overlayHost(overlayHost) {
    ui->setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ui->channelFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ui->channelHeaderLayout->setStretch(ui->channelHeaderLayout->indexOf(ui->noteEdit), 1);

    auto *stripRowLayout = qobject_cast<QHBoxLayout *>(ui->channelStripRow->layout());
    for (int i = 0; i < 8; ++i) {
        m_channels[i] = new ScopeChannelStrip(i, ui->channelStripRow);
        stripRowLayout->addWidget(m_channels[i], 1);
    }

    m_registerPanel = new ScopeRegisterPanel;
    m_generatorPanel = new ScopeGeneratorPanel;
    m_registerWindow = createOverlayWindow(tr("Register R/W"), m_registerPanel, QSize(500, 400));
    m_generatorWindow = createOverlayWindow(tr("Wave Generator"), m_generatorPanel, QSize(420, 240));

    m_yAxisMenu = new QMenu(ui->yAxisButton);
    m_yAxisMenu->addAction(tr("Auto"));
    m_yAxisMenu->addAction(tr("Manual..."));
    ui->yAxisButton->setMenu(m_yAxisMenu);

    ui->intervalCombo->setCurrentIndex(5);
    ui->windowCombo->setCurrentIndex(0);

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

void ScopeBottomPanel::setRunning(bool running) {
    if (m_running == running) {
        return;
    }

    m_running = running;
    const bool configEditable = !running;
    ui->intervalCombo->setEnabled(configEditable);
    ui->windowCombo->setEnabled(configEditable);
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
    ui->channelFrame->setVisible(m_channelsVisible);
    setMinimumHeight(m_channelsVisible ? 132 : 40);
    setMaximumHeight(m_channelsVisible ? 240 : 40);

    ui->channelsToggleButton->setText(m_channelsVisible ? tr("Hide Channels") : tr("Show Channels"));
    ui->registerToggleButton->setText(m_registerWindow->isVisible() ? tr("Hide Register") : tr("Show Register"));
    ui->generatorToggleButton->setText(m_generatorWindow->isVisible() ? tr("Hide Generator") : tr("Show Generator"));

    updateGeometry();
}

void ScopeBottomPanel::refreshYAxisButton() {
    if (m_yAxisAuto) {
        ui->yAxisButton->setText(tr("Y Axis: Auto"));
        return;
    }

    ui->yAxisButton->setText(tr("Y Axis: %1 ~ %2")
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
    connect(m_registerPanel, &ScopeRegisterPanel::clearPanelRequested, this, &ScopeBottomPanel::clearPanelRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::loadParamsRequested, this, &ScopeBottomPanel::loadParamsRequested);
    connect(ui->intervalCombo, &QComboBox::currentTextChanged, this, &ScopeBottomPanel::sampleIntervalChanged);
    connect(ui->windowCombo, &QComboBox::currentTextChanged, this, &ScopeBottomPanel::displayWindowChanged);
    connect(ui->noteEdit, &QLineEdit::textChanged, this, &ScopeBottomPanel::captureNoteChanged);
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

    connect(ui->channelsToggleButton, &QPushButton::clicked, this, [this]() {
        m_channelsVisible = !m_channelsVisible;
        refreshPanels();
    });
    connect(ui->registerToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !m_registerWindow->isVisible();
        m_registerWindow->setVisible(visible);
        if (visible) {
            m_registerWindow->raise();
            m_registerWindow->activateWindow();
        }
        refreshPanels();
    });
    connect(ui->generatorToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !m_generatorWindow->isVisible();
        m_generatorWindow->setVisible(visible);
        if (visible) {
            m_generatorWindow->raise();
            m_generatorWindow->activateWindow();
        }
        refreshPanels();
    });
}

