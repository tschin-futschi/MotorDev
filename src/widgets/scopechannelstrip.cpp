#include "widgets/scopechannelstrip.h"

#include "ui_scopechannelstrip.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QSizePolicy>
#include <utility>

ScopeChannelStrip::ScopeChannelStrip(int index, QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ScopeChannelStrip>())
    , m_index(index) {
    ui->setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ui->checkBox->setText(QStringLiteral("CH%1").arg(m_index + 1));
    ui->checkBox->setChecked(m_index < 4);
    ui->descEdit->setText(m_index == 0 ? QStringLiteral("Speed")
                     : m_index == 1 ? QStringLiteral("Torque")
                     : m_index == 2 ? QStringLiteral("Temp")
                     : m_index == 3 ? QStringLiteral("Current")
                                     : QString());
    ui->addrEdit->setText(QStringLiteral("0x%1").arg(0x0010 + m_index * 2, 4, 16, QLatin1Char('0')).toUpper());
    connectSignals();
}

ScopeChannelStrip::~ScopeChannelStrip() = default;

bool ScopeChannelStrip::isChannelEnabled() const {
    return ui->checkBox->isChecked();
}

void ScopeChannelStrip::connectSignals() {
    connect(ui->checkBox, &QCheckBox::toggled, this, [this](bool checked) {
        emit channelToggled(m_index, checked);
    });
    connect(ui->descEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit descriptionChanged(m_index, text);
    });
    connect(ui->addrEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit addressChanged(m_index, text);
    });
}
