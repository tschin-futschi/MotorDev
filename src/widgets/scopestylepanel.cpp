#include "widgets/scopestylepanel.h"

#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace {
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
}

ScopeStylePanel::ScopeStylePanel(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
}

ScopeStylePanel::~ScopeStylePanel() = default;

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
            Qt::PenStyle style = Qt::SolidLine;
            bool dots = false;
            switch (comboIndex) {
            case 1: style = Qt::DashLine; break;
            case 2: style = Qt::DotLine; break;
            case 3: style = Qt::DashDotLine; break;
            case 4: style = Qt::SolidLine; dots = true; break;
            default: break;
            }
            emit lineStyleChanged(index, style);
            emit dataPointsChanged(index, dots);
        });
    }
}

void ScopeStylePanel::updateColorButton(int index) {
    if (index < 0 || index >= kChannelCount) {
        return;
    }

    // Runtime-selected color cannot be expressed in static QSS.
    m_rows[index].colorButton->setStyleSheet(QStringLiteral("background:%1;").arg(m_rows[index].currentColor.name()));
}

void ScopeStylePanel::setupUi() {
    setObjectName(QStringLiteral("ScopeStylePanel"));
    setMinimumSize(QSize(200, 0));
    setMaximumSize(QSize(200, QWIDGETSIZE_MAX));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(2);
    rootLayout->setContentsMargins(8, 8, 8, 8);

    auto *titleLabel = new QLabel(this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setText(tr("Channel Style"));
    rootLayout->addWidget(titleLabel);

    m_defaultButton = new QPushButton(this);
    m_defaultButton->setObjectName(QStringLiteral("defaultButton"));
    m_defaultButton->setProperty("buttonRole", QStringLiteral("default"));
    m_defaultButton->setText(tr("Default Setting"));
    rootLayout->addWidget(m_defaultButton);

    const QStringList comboItems = {tr("Solid"), tr("Dash"), tr("Dot"), tr("DashDot"), tr("SolidDot")};
    for (int index = 0; index < kChannelCount; ++index) {
        auto *rowLayout = new QHBoxLayout();
        rowLayout->setObjectName(QStringLiteral("row%1Layout").arg(index));
        rowLayout->setSpacing(2);

        auto *label = new QLabel(this);
        label->setObjectName(QStringLiteral("ch%1Label").arg(index));
        label->setMinimumSize(QSize(30, 0));
        label->setMaximumSize(QSize(30, QWIDGETSIZE_MAX));
        label->setText(QStringLiteral("CH%1").arg(index + 1));
        rowLayout->addWidget(label);

        m_rows[index].colorButton = new QPushButton(this);
        m_rows[index].colorButton->setObjectName(QStringLiteral("colorButton%1").arg(index));
        m_rows[index].colorButton->setMinimumSize(QSize(20, 20));
        m_rows[index].colorButton->setMaximumSize(QSize(20, 20));
        m_rows[index].colorButton->setProperty("buttonRole", QStringLiteral("color"));
        m_rows[index].colorButton->setText(QString());
        rowLayout->addWidget(m_rows[index].colorButton);

        m_rows[index].widthSpin = new QSpinBox(this);
        m_rows[index].widthSpin->setObjectName(QStringLiteral("widthSpin%1").arg(index));
        m_rows[index].widthSpin->setMinimumSize(QSize(92, 24));
        m_rows[index].widthSpin->setMaximumSize(QSize(92, 24));
        m_rows[index].widthSpin->setRange(1, 8);
        m_rows[index].widthSpin->setValue(4);
        m_rows[index].widthSpin->setSuffix(QStringLiteral("px"));
        rowLayout->addWidget(m_rows[index].widthSpin);

        m_rows[index].styleCombo = new QComboBox(this);
        m_rows[index].styleCombo->setObjectName(QStringLiteral("styleCombo%1").arg(index));
        m_rows[index].styleCombo->addItems(comboItems);
        rowLayout->addWidget(m_rows[index].styleCombo);

        rootLayout->addLayout(rowLayout);
    }

    rootLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}
