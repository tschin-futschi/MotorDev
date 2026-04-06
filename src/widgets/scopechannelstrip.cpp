#include "widgets/scopechannelstrip.h"

#include "ui/style_constants.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QSizePolicy>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QString makeLineEditStyle() {
    return QStringLiteral(
               "QLineEdit { background:%1; color:%2; border:1px solid %3; "
               "border-radius:4px; padding:2px 5px; font-size:10px; }"
               "QLineEdit:focus { border-color:%4; }")
        .arg(Style::Color::White.name(),
             Style::Color::ScopeText.name(),
             Style::Color::ScopeChannelInputBorder.name(),
             Style::Color::ScopeChannelInputFocus.name());
}
} // namespace

ScopeChannelStrip::ScopeChannelStrip(int index, QWidget *parent)
    : QWidget(parent)
    , m_index(index) {
    setupUi();
    connectSignals();
}

bool ScopeChannelStrip::isChannelEnabled() const {
    return m_checkBox->isChecked();
}

void ScopeChannelStrip::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    m_checkBox = new QCheckBox(QStringLiteral("CH%1").arg(m_index + 1), this);
    m_checkBox->setChecked(m_index < 4);
    m_checkBox->setStyleSheet(QStringLiteral(
        "QCheckBox { color:%1; font-size:11px; spacing:4px; margin:0; padding:0; }"
        "QCheckBox::indicator { width:12px; height:12px; }"
        "QCheckBox::indicator:unchecked { background:%2; border:1px solid %3; }"
        "QCheckBox::indicator:checked { background:%4; border:1px solid %5; }")
                                  .arg(Style::Color::ScopeTextChannel.name(),
                                       Style::Color::White.name(),
                                       Style::Color::ScopeCheckboxUncheckedBorder.name(),
                                       Style::Color::ScopeCheckboxCheckedBackground.name(),
                                       Style::Color::ScopeCheckboxCheckedBorder.name()));

    m_descEdit = new QLineEdit(this);
    m_descEdit->setPlaceholderText(tr("Description"));
    m_descEdit->setText(m_index == 0 ? QStringLiteral("Speed")
                    : m_index == 1 ? QStringLiteral("Torque")
                    : m_index == 2 ? QStringLiteral("Temp")
                    : m_index == 3 ? QStringLiteral("Current")
                                    : QString());
    m_descEdit->setStyleSheet(makeLineEditStyle());
    m_descEdit->setMinimumHeight(22);
    m_descEdit->setMaximumHeight(22);

    m_addrEdit = new QLineEdit(this);
    m_addrEdit->setPlaceholderText(QStringLiteral("0x0000"));
    m_addrEdit->setText(QStringLiteral("0x%1").arg(0x0010 + m_index * 2, 4, 16, QLatin1Char('0')).toUpper());
    m_addrEdit->setStyleSheet(makeLineEditStyle());
    m_addrEdit->setMinimumHeight(22);
    m_addrEdit->setMaximumHeight(22);

    layout->addWidget(m_checkBox);
    layout->addWidget(m_descEdit);
    layout->addWidget(m_addrEdit);

    setMinimumWidth(104);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    setStyleSheet(QStringLiteral("background:%1; border:1px solid %2; border-radius:6px;")
                      .arg(Style::Color::ScopeChannelBackground.name())
                      .arg(Style::Color::ScopeChannelBorder.name()));
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
