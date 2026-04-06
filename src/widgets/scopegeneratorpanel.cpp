#include "widgets/scopegeneratorpanel.h"

#include "ui/style_constants.h"

#include <QLabel>
#include <QVBoxLayout>

using namespace MotorDev;

ScopeGeneratorPanel::ScopeGeneratorPanel(QWidget *parent)
    : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *titleLabel = new QLabel(tr("Wave Generator"), this);
    titleLabel->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:600;")
                                  .arg(Style::Color::ScopeTextSecondary.name()));

    auto *placeholderLabel = new QLabel(tr("Reserved for future waveform generator UI.\nNo backend logic in this round."), this);
    placeholderLabel->setWordWrap(true);
    placeholderLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    placeholderLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px; line-height:1.4;")
                                        .arg(Style::Color::ScopeTextDim.name()));

    layout->addWidget(titleLabel);
    layout->addWidget(placeholderLabel);
    layout->addStretch();

    setStyleSheet(QStringLiteral("background:%1; border:1px solid %2; border-radius:6px;")
                      .arg(Style::Color::ScopePanelBackground.name())
                      .arg(Style::Color::ScopePanelBorder.name()));
}
