#include "widgets/scopegeneratorpanel.h"

#include <QLabel>
#include <QSpacerItem>
#include <QVBoxLayout>

ScopeGeneratorPanel::ScopeGeneratorPanel(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

ScopeGeneratorPanel::~ScopeGeneratorPanel() = default;

void ScopeGeneratorPanel::setupUi() {
    setObjectName(QStringLiteral("ScopeGeneratorPanel"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(6);
    rootLayout->setContentsMargins(8, 8, 8, 8);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));
    m_titleLabel->setText(tr("Wave Generator"));
    rootLayout->addWidget(m_titleLabel);

    m_placeholderLabel = new QLabel(this);
    m_placeholderLabel->setObjectName(QStringLiteral("placeholderLabel"));
    m_placeholderLabel->setWordWrap(true);
    m_placeholderLabel->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignTop);
    m_placeholderLabel->setText(tr("Reserved for future waveform generator UI.\nNo backend logic in this round."));
    rootLayout->addWidget(m_placeholderLabel);

    rootLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}
