#include "widgets/activitybar.h"

#include "ui/repolish.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

ActivityBar::ActivityBar(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
    setActivePage(ConfigPage);
}

ActivityBar::~ActivityBar() = default;

void ActivityBar::connectSignals() {
    connect(m_configButton, &QPushButton::clicked, this, [this] {
        setActivePage(ConfigPage);
        emit pageSelected(ConfigPage);
    });
    connect(m_registerButton, &QPushButton::clicked, this, [this] {
        setActivePage(RegisterPage);
        emit pageSelected(RegisterPage);
    });
    connect(m_flashButton, &QPushButton::clicked, this, [this] {
        setActivePage(FlashPage);
        emit pageSelected(FlashPage);
    });
    connect(m_scopeButton, &QPushButton::clicked, this, [this] {
        setActivePage(ScopePage);
        emit pageSelected(ScopePage);
    });
    connect(m_debugButton, &QPushButton::clicked, this, [this] {
        setActivePage(DebugPage);
        emit pageSelected(DebugPage);
    });
}

void ActivityBar::setPageEnabled(int page, bool enabled) {
    switch (page) {
    case RegisterPage:
        m_registerButton->setEnabled(enabled);
        break;
    case FlashPage:
        m_flashButton->setEnabled(enabled);
        break;
    case ScopePage:
        m_scopeButton->setEnabled(enabled);
        break;
    case DebugPage:
        break;
    default:
        break;
    }
}

void ActivityBar::setActivePage(int index) {
    const QList<QPushButton *> buttons = {
        m_configButton,
        m_registerButton,
        m_flashButton,
        m_scopeButton,
        m_debugButton,
        m_settingsButton,
    };

    for (auto *button : buttons) {
        const bool active = (button == m_configButton && index == ConfigPage)
            || (button == m_registerButton && index == RegisterPage)
            || (button == m_flashButton && index == FlashPage)
            || (button == m_scopeButton && index == ScopePage)
            || (button == m_debugButton && index == DebugPage);
        button->setProperty("active", active);
        MotorDev::UiUtil::repolish(button);
    }
}

void ActivityBar::setupUi() {
    setObjectName(QStringLiteral("ActivityBar"));
    resize(44, 640);
    setMinimumSize(QSize(44, 0));
    setMaximumSize(QSize(44, QWIDGETSIZE_MAX));

    auto *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
    verticalLayout->setSpacing(6);
    verticalLayout->setContentsMargins(4, 8, 4, 8);

    const struct {
        QPushButton **button;
        const char *objectName;
        const char *text;
    } specs[] = {
        {&m_configButton, "configButton", "配置"},
        {&m_registerButton, "registerButton", "读写"},
        {&m_flashButton, "flashButton", "烧录"},
        {&m_scopeButton, "scopeButton", "示波"},
        {&m_debugButton, "debugButton", "调试"},
    };

    for (const auto &spec : specs) {
        auto *wrapper = new QHBoxLayout();
        wrapper->setContentsMargins(0, 0, 0, 0);
        wrapper->addStretch();
        *spec.button = new QPushButton(this);
        (*spec.button)->setObjectName(QString::fromLatin1(spec.objectName));
        (*spec.button)->setMinimumSize(QSize(34, 34));
        (*spec.button)->setMaximumSize(QSize(34, 34));
        (*spec.button)->setText(tr(spec.text));
        (*spec.button)->setProperty("active", false);
        wrapper->addWidget(*spec.button);
        wrapper->addStretch();
        verticalLayout->addLayout(wrapper);
    }

    verticalLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    auto *settingsWrapper = new QHBoxLayout();
    settingsWrapper->setContentsMargins(0, 0, 0, 0);
    settingsWrapper->addStretch();
    m_settingsButton = new QPushButton(this);
    m_settingsButton->setObjectName(QStringLiteral("settingsButton"));
    m_settingsButton->setMinimumSize(QSize(34, 34));
    m_settingsButton->setMaximumSize(QSize(34, 34));
    m_settingsButton->setText(tr("设置"));
    m_settingsButton->setProperty("active", false);
    settingsWrapper->addWidget(m_settingsButton);
    settingsWrapper->addStretch();
    verticalLayout->addLayout(settingsWrapper);
}
