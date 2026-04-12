#include "widgets/scopestylepanel.h"

#include "ui/style_constants.h"

#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QAbstractSpinBox>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QString colorButtonStyle(const QColor &color) {
    return QStringLiteral(
               "QPushButton { background:%1; border:1px solid %2; border-radius:%3px; }"
               "QPushButton:hover { border-color:%4; }")
        .arg(color.name(),
             Style::Color::ScopeInputBorder.name(),
             QString::number(Style::Size::ScopeStyleButtonRadius),
             Style::Color::ScopeInputBorderAlt.name());
}

QString inputStyle() {
    return QStringLiteral(
               "QSpinBox, QComboBox { background:%1; border:1px solid %2; border-radius:%3px; "
               "color:%4; font-size:%5px; padding:1px 3px; min-height:%6px; }"
               "QSpinBox:focus, QComboBox:focus { border-color:%7; }")
        .arg(Style::Color::ScopeChannelBackground.name(),
             Style::Color::ScopeChannelInputBorder.name(),
             QString::number(Style::Size::ScopeStyleButtonRadius),
             Style::Color::ScopeText.name(),
             QString::number(Style::Size::ScopeStyleRowFontPx),
             QString::number(Style::Size::ScopeStyleInputMinHeight),
             Style::Color::ScopeChannelInputFocus.name());
}

QString widthSpinStyle() {
    return QStringLiteral(
               "QSpinBox { background:%1; border:1px solid %2; border-radius:%3px; "
               "color:%4; font-size:%5px; padding:1px 24px 1px 6px; min-height:%6px; }"
               "QSpinBox:focus { border-color:%7; }"
               "QSpinBox::up-button, QSpinBox::down-button { width:22px; subcontrol-origin:border; "
               "background:%8; border-left:1px solid %2; }"
               "QSpinBox::up-button { subcontrol-position:top right; border-top-right-radius:%3px; }"
               "QSpinBox::down-button { subcontrol-position:bottom right; border-top:1px solid %2; "
               "border-bottom-right-radius:%3px; }"
               "QSpinBox::up-button:hover, QSpinBox::down-button:hover { background:%9; }"
               "QSpinBox::up-arrow, QSpinBox::down-arrow { width:10px; height:10px; }")
        .arg(Style::Color::ScopeChannelBackground.name(),
             Style::Color::ScopeChannelInputBorder.name(),
             QString::number(Style::Size::ScopeStyleButtonRadius),
             Style::Color::ScopeText.name(),
             QString::number(Style::Size::ScopeStyleRowFontPx),
             QString::number(Style::Size::ScopeStyleInputMinHeight),
             Style::Color::ScopeChannelInputFocus.name(),
             Style::Color::ScopeToolButtonBackground.name(),
             Style::Color::ScopeToolButtonHover.name());
}

QString defaultButtonStyle() {
    return QStringLiteral(
               "QPushButton { background:%1; border:1px solid %2; border-radius:%3px; color:%4; "
               "font-size:%5px; font-weight:600; min-height:24px; padding:0 8px; }"
               "QPushButton:hover { background:%6; border-color:%7; }"
               "QPushButton:pressed { background:%8; }")
        .arg(Style::Color::ScopeToolButtonBackground.name(),
             Style::Color::ScopeToolButtonBorder.name(),
             QString::number(Style::Size::ScopeStyleButtonRadius),
             Style::Color::ScopeText.name(),
             QString::number(Style::Size::ScopeStyleRowFontPx),
             Style::Color::ScopeToolButtonHover.name(),
             Style::Color::ScopeToolButtonHoverBorder.name(),
             Style::Color::ScopeToolButtonPressed.name());
}

int styleIndexFor(Qt::PenStyle style) {
    switch (style) {
    case Qt::DashLine:
        return 1;
    case Qt::DotLine:
        return 2;
    case Qt::DashDotLine:
        return 3;
    case Qt::SolidLine:
    default:
        return 0;
    }
}
} // namespace

ScopeStylePanel::ScopeStylePanel(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
}

void ScopeStylePanel::setChannelColor(int index, const QColor &color) {
    if (index < 0 || index >= kChannelCount || !color.isValid()) {
        return;
    }

    m_rows[index].currentColor = color;
    updateColorButton(index);
}

void ScopeStylePanel::setChannelLineWidth(int index, int width) {
    if (index < 0 || index >= kChannelCount) {
        return;
    }

    QSignalBlocker blocker(m_rows[index].widthSpin);
    m_rows[index].widthSpin->setValue(width);
}

void ScopeStylePanel::setChannelLineStyle(int index, Qt::PenStyle style) {
    if (index < 0 || index >= kChannelCount) {
        return;
    }

    QSignalBlocker blocker(m_rows[index].styleCombo);
    m_rows[index].styleCombo->setCurrentIndex(styleIndexFor(style));
}

void ScopeStylePanel::setChannelShowDataPoints(int index, bool show) {
    if (index < 0 || index >= kChannelCount) {
        return;
    }

    QSignalBlocker blocker(m_rows[index].styleCombo);
    if (show) {
        m_rows[index].styleCombo->setCurrentIndex(4);
    } else if (m_rows[index].styleCombo->currentIndex() == 4) {
        m_rows[index].styleCombo->setCurrentIndex(0);
    }
}

void ScopeStylePanel::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Style::Size::ScopeStylePanelMargin,
                               Style::Size::ScopeStylePanelMargin,
                               Style::Size::ScopeStylePanelMargin,
                               Style::Size::ScopeStylePanelMargin);
    layout->setSpacing(Style::Size::ScopeStylePanelRowSpacing);

    auto *title = new QLabel(tr("Channel Style"), this);
    title->setStyleSheet(QStringLiteral("color:%1; font-size:%2px; font-weight:700;")
                             .arg(Style::Color::ScopeTextHeader.name())
                             .arg(Style::Size::ScopeStyleTitleFontPx));
    layout->addWidget(title);

    m_defaultButton = new QPushButton(tr("Default Setting"), this);
    m_defaultButton->setStyleSheet(defaultButtonStyle());
    layout->addWidget(m_defaultButton);

    const QString commonInputStyle = inputStyle();

    for (int index = 0; index < kChannelCount; ++index) {
        auto *rowLayout = new QHBoxLayout();
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(Style::Size::ScopeStylePanelRowSpacing);

        auto *label = new QLabel(QStringLiteral("CH%1").arg(index + 1), this);
        label->setFixedWidth(Style::Size::ScopeStyleChannelLabelWidth);
        label->setStyleSheet(QStringLiteral("color:%1; font-size:%2px;")
                                 .arg(Style::Color::ScopeTextLabel.name())
                                 .arg(Style::Size::ScopeStyleRowFontPx));

        m_rows[index].colorButton = new QPushButton(this);
        m_rows[index].colorButton->setFixedSize(Style::Size::ScopeStyleColorButtonSize,
                                                Style::Size::ScopeStyleColorButtonSize);

        m_rows[index].widthSpin = new QSpinBox(this);
        m_rows[index].widthSpin->setRange(1, 8);
        m_rows[index].widthSpin->setValue(4);
        m_rows[index].widthSpin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
        m_rows[index].widthSpin->setSuffix(QStringLiteral("px"));
        m_rows[index].widthSpin->setFixedWidth(Style::Size::ScopeStyleWidthSpinWidth);
        m_rows[index].widthSpin->setStyleSheet(widthSpinStyle());

        m_rows[index].styleCombo = new QComboBox(this);
        m_rows[index].styleCombo->addItem(QStringLiteral("Solid"), static_cast<int>(Qt::SolidLine));
        m_rows[index].styleCombo->addItem(QStringLiteral("Dash"), static_cast<int>(Qt::DashLine));
        m_rows[index].styleCombo->addItem(QStringLiteral("Dot"), static_cast<int>(Qt::DotLine));
        m_rows[index].styleCombo->addItem(QStringLiteral("DashDot"), static_cast<int>(Qt::DashDotLine));
        m_rows[index].styleCombo->addItem(QStringLiteral("SolidDot"), static_cast<int>(Qt::SolidLine) | (1 << 16));
        m_rows[index].styleCombo->setCurrentIndex(0);
        m_rows[index].styleCombo->setStyleSheet(commonInputStyle);

        rowLayout->addWidget(label);
        rowLayout->addWidget(m_rows[index].colorButton);
        rowLayout->addWidget(m_rows[index].widthSpin);
        rowLayout->addWidget(m_rows[index].styleCombo, 1);

        layout->addLayout(rowLayout);
    }

    layout->addStretch();

    setFixedWidth(Style::Size::ScopeStylePanelWidth);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setStyleSheet(QStringLiteral("background:%1; border-left:%2px solid %3;")
                      .arg(Style::Color::ScopePanelBackground.name())
                      .arg(Style::Size::BorderThin)
                      .arg(Style::Color::ScopePanelBorder.name()));
}

void ScopeStylePanel::connectSignals() {
    connect(m_defaultButton, &QPushButton::clicked, this, [this]() {
        emit defaultSettingsRequested();
    });

    for (int index = 0; index < kChannelCount; ++index) {
        connect(m_rows[index].colorButton, &QPushButton::clicked, this, [this, index]() {
            const QColor selected = QColorDialog::getColor(
                m_rows[index].currentColor,
                this,
                tr("Select channel color"));
            if (!selected.isValid()) {
                return;
            }

            m_rows[index].currentColor = selected;
            updateColorButton(index);
            emit colorChanged(index, selected);
        });
        connect(m_rows[index].widthSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this, index](int width) {
            emit lineWidthChanged(index, width);
        });
        connect(m_rows[index].styleCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, index](int comboIndex) {
            const int rawData = m_rows[index].styleCombo->itemData(comboIndex).toInt();
            const bool dots = (rawData >> 16) != 0;
            const Qt::PenStyle style = static_cast<Qt::PenStyle>(rawData & 0xFFFF);
            emit lineStyleChanged(index, style);
            emit dataPointsChanged(index, dots);
        });
    }
}

void ScopeStylePanel::updateColorButton(int index) {
    if (index < 0 || index >= kChannelCount) {
        return;
    }

    m_rows[index].colorButton->setStyleSheet(colorButtonStyle(m_rows[index].currentColor));
}
