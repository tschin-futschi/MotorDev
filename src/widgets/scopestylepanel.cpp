#include "widgets/scopestylepanel.h"

#include "ui_scopestylepanel.h"

#include <QColorDialog>
#include <QComboBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <utility>

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
    : QWidget(parent)
    , ui(std::make_unique<Ui::ScopeStylePanel>()) {
    ui->setupUi(this);
    m_rows[0].colorButton = ui->colorButton0; m_rows[0].widthSpin = ui->widthSpin0; m_rows[0].styleCombo = ui->styleCombo0;
    m_rows[1].colorButton = ui->colorButton1; m_rows[1].widthSpin = ui->widthSpin1; m_rows[1].styleCombo = ui->styleCombo1;
    m_rows[2].colorButton = ui->colorButton2; m_rows[2].widthSpin = ui->widthSpin2; m_rows[2].styleCombo = ui->styleCombo2;
    m_rows[3].colorButton = ui->colorButton3; m_rows[3].widthSpin = ui->widthSpin3; m_rows[3].styleCombo = ui->styleCombo3;
    m_rows[4].colorButton = ui->colorButton4; m_rows[4].widthSpin = ui->widthSpin4; m_rows[4].styleCombo = ui->styleCombo4;
    m_rows[5].colorButton = ui->colorButton5; m_rows[5].widthSpin = ui->widthSpin5; m_rows[5].styleCombo = ui->styleCombo5;
    m_rows[6].colorButton = ui->colorButton6; m_rows[6].widthSpin = ui->widthSpin6; m_rows[6].styleCombo = ui->styleCombo6;
    m_rows[7].colorButton = ui->colorButton7; m_rows[7].widthSpin = ui->widthSpin7; m_rows[7].styleCombo = ui->styleCombo7;
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
    connect(ui->defaultButton, &QPushButton::clicked, this, [this]() {
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
