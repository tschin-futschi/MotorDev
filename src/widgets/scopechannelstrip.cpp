#include "widgets/scopechannelstrip.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QSizePolicy>
#include <QVBoxLayout>

ScopeChannelStrip::ScopeChannelStrip(int index, QWidget *parent)
    : QWidget(parent)
    , m_index(index) {
    setupUi();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_checkBox->setText(QStringLiteral("CH%1").arg(m_index + 1));
    m_checkBox->setChecked(m_index < 4);
    m_descEdit->setText(m_index == 0 ? QStringLiteral("Speed")
                     : m_index == 1 ? QStringLiteral("Torque")
                     : m_index == 2 ? QStringLiteral("Temp")
                     : m_index == 3 ? QStringLiteral("Current")
                                     : QString());
    m_addrEdit->setText(QStringLiteral("0x%1").arg(0x0010 + m_index * 2, 4, 16, QLatin1Char('0')).toUpper());
    connectSignals();
}

ScopeChannelStrip::~ScopeChannelStrip() = default;

bool ScopeChannelStrip::isChannelEnabled() const {
    return m_checkBox->isChecked();
}

void ScopeChannelStrip::setupUi() {
    setObjectName(QStringLiteral("ScopeChannelStrip"));
    setMinimumSize(QSize(104, 0));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(4);
    rootLayout->setContentsMargins(6, 6, 6, 6);

    m_checkBox = new QCheckBox(this);
    m_checkBox->setObjectName(QStringLiteral("checkBox"));
    m_checkBox->setText(tr("CH1"));
    rootLayout->addWidget(m_checkBox);

    m_descEdit = new QLineEdit(this);
    m_descEdit->setObjectName(QStringLiteral("descEdit"));
    m_descEdit->setMinimumSize(QSize(0, 22));
    m_descEdit->setMaximumSize(QSize(QWIDGETSIZE_MAX, 22));
    m_descEdit->setPlaceholderText(tr("Description"));
    rootLayout->addWidget(m_descEdit);

    m_addrEdit = new QLineEdit(this);
    m_addrEdit->setObjectName(QStringLiteral("addrEdit"));
    m_addrEdit->setMinimumSize(QSize(0, 22));
    m_addrEdit->setMaximumSize(QSize(QWIDGETSIZE_MAX, 22));
    m_addrEdit->setPlaceholderText(tr("0x0000"));
    rootLayout->addWidget(m_addrEdit);
}

void ScopeChannelStrip::connectSignals() {
    connect(m_checkBox, &QCheckBox::toggled, this, [this](bool checked) {
        emit channelToggled(m_index, checked);
    });
    connect(m_descEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit descriptionChanged(m_index, text);
    });
    connect(m_addrEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit addressChanged(m_index, text);
    });
}
