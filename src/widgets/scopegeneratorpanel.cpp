#include "widgets/scopegeneratorpanel.h"

#include "ui/repolish.h"
#include "ui/style_constants.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {

QLineEdit *createGeneratorEdit(QWidget *parent, const QString &objectName, const QString &placeholder) {
    auto *edit = new QLineEdit(parent);
    edit->setObjectName(objectName);
    edit->setProperty("inputRole", QStringLiteral("scope-generator"));
    edit->setPlaceholderText(placeholder);
    return edit;
}

}

ScopeGeneratorPanel::ScopeGeneratorPanel(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
    setRunning(false);
}

ScopeGeneratorPanel::~ScopeGeneratorPanel() = default;

void ScopeGeneratorPanel::setRunning(bool running) {
    if (m_modeGroup != nullptr) {
        const auto buttons = m_modeGroup->buttons();
        for (QAbstractButton *button : buttons) {
            if (button != nullptr) {
                button->setEnabled(!running);
            }
        }
    }

    if (m_startButton != nullptr) {
        m_startButton->setEnabled(!running);
    }
    if (m_stopButton != nullptr) {
        m_stopButton->setEnabled(running);
    }
}

void ScopeGeneratorPanel::setupUi() {
    setObjectName(QStringLiteral("ScopeGeneratorPanel"));
    setMinimumSize(QSize(Style::Size::GeneratorPanelMinW, Style::Size::GeneratorPanelMinH));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(8);
    rootLayout->setContentsMargins(10, 10, 10, 10);

    auto *titleLabel = new QLabel(tr("Wave Generator"), this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    rootLayout->addWidget(titleLabel);

    auto *modeLayout = new QHBoxLayout();
    modeLayout->setObjectName(QStringLiteral("modeLayout"));
    modeLayout->setSpacing(12);
    rootLayout->addLayout(modeLayout);

    m_modeGroup = new QButtonGroup(this);
    m_linearRadio = new QRadioButton(tr("Linear"), this);
    m_linearRadio->setObjectName(QStringLiteral("linearRadio"));
    m_linearRadio->setChecked(true);
    m_cosineRadio = new QRadioButton(tr("Cosine"), this);
    m_cosineRadio->setObjectName(QStringLiteral("cosineRadio"));
    m_modeGroup->addButton(m_linearRadio, 0);
    m_modeGroup->addButton(m_cosineRadio, 1);
    modeLayout->addWidget(m_linearRadio);
    modeLayout->addWidget(m_cosineRadio);
    modeLayout->addStretch(1);

    m_modeStack = new QStackedWidget(this);
    m_modeStack->setObjectName(QStringLiteral("modeStack"));
    rootLayout->addWidget(m_modeStack, 1);

    auto *linearPage = new QWidget(m_modeStack);
    linearPage->setObjectName(QStringLiteral("linearPage"));
    auto *linearLayout = new QFormLayout(linearPage);
    linearLayout->setObjectName(QStringLiteral("linearLayout"));
    linearLayout->setSpacing(8);
    linearLayout->setContentsMargins(0, 0, 0, 0);

    m_linearAddrEdit = createGeneratorEdit(linearPage, QStringLiteral("linearAddrEdit"), QStringLiteral("0x0020"));
    m_linearMinEdit = createGeneratorEdit(linearPage, QStringLiteral("linearMinEdit"), QStringLiteral("-100"));
    m_linearMaxEdit = createGeneratorEdit(linearPage, QStringLiteral("linearMaxEdit"), QStringLiteral("100"));
    m_linearStepEdit = createGeneratorEdit(linearPage, QStringLiteral("linearStepEdit"), QStringLiteral("5"));
    m_linearIntervalEdit = createGeneratorEdit(linearPage, QStringLiteral("linearIntervalEdit"), QStringLiteral("100"));

    linearLayout->addRow(tr("Register Addr"), m_linearAddrEdit);
    linearLayout->addRow(tr("Min"), m_linearMinEdit);
    linearLayout->addRow(tr("Max"), m_linearMaxEdit);
    linearLayout->addRow(tr("Step"), m_linearStepEdit);
    linearLayout->addRow(tr("Interval (ms)"), m_linearIntervalEdit);
    m_modeStack->addWidget(linearPage);

    auto *cosinePage = new QWidget(m_modeStack);
    cosinePage->setObjectName(QStringLiteral("cosinePage"));
    auto *cosineLayout = new QGridLayout(cosinePage);
    cosineLayout->setObjectName(QStringLiteral("cosineLayout"));
    cosineLayout->setSpacing(8);
    cosineLayout->setContentsMargins(0, 0, 0, 0);
    cosineLayout->setColumnStretch(1, 1);
    cosineLayout->setColumnStretch(3, 1);

    auto addGlobalRow = [cosinePage, cosineLayout](int row, const QString &leftLabel, QLineEdit **leftEdit,
                                                   const QString &leftName, const QString &leftPlaceholder,
                                                   const QString &rightLabel, QLineEdit **rightEdit,
                                                   const QString &rightName, const QString &rightPlaceholder) {
        auto *lLabel = new QLabel(leftLabel, cosinePage);
        auto *rLabel = new QLabel(rightLabel, cosinePage);
        *leftEdit = createGeneratorEdit(cosinePage, leftName, leftPlaceholder);
        *rightEdit = createGeneratorEdit(cosinePage, rightName, rightPlaceholder);
        cosineLayout->addWidget(lLabel, row, 0);
        cosineLayout->addWidget(*leftEdit, row, 1);
        cosineLayout->addWidget(rLabel, row, 2);
        cosineLayout->addWidget(*rightEdit, row, 3);
    };

    addGlobalRow(0, tr("Amplitude"), &m_cosineAmplitudeEdit, QStringLiteral("cosineAmplitudeEdit"), QStringLiteral("1000"),
                 tr("Offset"), &m_cosineOffsetEdit, QStringLiteral("cosineOffsetEdit"), QStringLiteral("0"));
    auto *freqLabel = new QLabel(tr("Freq (Hz)"), cosinePage);
    m_cosineFrequencyEdit = createGeneratorEdit(cosinePage, QStringLiteral("cosineFrequencyEdit"), QStringLiteral("1.0"));
    cosineLayout->addWidget(freqLabel, 1, 0);
    cosineLayout->addWidget(m_cosineFrequencyEdit, 1, 1);

    for (int channel = 0; channel < 3; ++channel) {
        auto *addrLabel = new QLabel(tr("CH%1 Addr").arg(channel + 1), cosinePage);
        auto *phaseLabel = new QLabel(tr("CH%1 Phase (deg)").arg(channel + 1), cosinePage);
        m_cosineAddrEdits[channel] = createGeneratorEdit(
            cosinePage, QStringLiteral("cosineAddrEdit%1").arg(channel), channel == 0 ? QStringLiteral("0x0020") : QString());
        const QString phasePlaceholder = (channel == 0) ? QStringLiteral("0")
                                       : (channel == 1) ? QStringLiteral("120")
                                                        : QStringLiteral("240");
        m_cosinePhaseEdits[channel] = createGeneratorEdit(
            cosinePage, QStringLiteral("cosinePhaseEdit%1").arg(channel), phasePlaceholder);
        cosineLayout->addWidget(addrLabel, channel + 2, 0);
        cosineLayout->addWidget(m_cosineAddrEdits[channel], channel + 2, 1);
        cosineLayout->addWidget(phaseLabel, channel + 2, 2);
        cosineLayout->addWidget(m_cosinePhaseEdits[channel], channel + 2, 3);
    }
    m_modeStack->addWidget(cosinePage);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setObjectName(QStringLiteral("buttonLayout"));
    buttonLayout->addStretch(1);
    rootLayout->addLayout(buttonLayout);

    m_startButton = new QPushButton(tr("Start"), this);
    m_startButton->setObjectName(QStringLiteral("startButton"));
    m_startButton->setProperty("buttonRole", QStringLiteral("generator-start"));
    buttonLayout->addWidget(m_startButton);

    m_stopButton = new QPushButton(tr("Stop"), this);
    m_stopButton->setObjectName(QStringLiteral("stopButton"));
    m_stopButton->setProperty("buttonRole", QStringLiteral("generator-stop"));
    buttonLayout->addWidget(m_stopButton);
}

void ScopeGeneratorPanel::connectSignals() {
    connect(m_linearRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_modeStack != nullptr) {
            m_modeStack->setCurrentIndex(0);
        }
    });
    connect(m_cosineRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_modeStack != nullptr) {
            m_modeStack->setCurrentIndex(1);
        }
    });
    connect(m_startButton, &QPushButton::clicked, this, &ScopeGeneratorPanel::handleStartClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &ScopeGeneratorPanel::stopRequested);
}

void ScopeGeneratorPanel::setFieldError(QLineEdit *edit, bool error) {
    if (edit == nullptr) {
        return;
    }
    edit->setProperty("hasError", error);
    MotorDev::UiUtil::repolish(edit);
}

bool ScopeGeneratorPanel::parseUint16Field(QLineEdit *edit, quint16 *out, bool allowEmpty) {
    if (edit == nullptr || out == nullptr) {
        return false;
    }

    const QString trimmed = edit->text().trimmed();
    if (trimmed.isEmpty()) {
        setFieldError(edit, !allowEmpty);
        return allowEmpty;
    }

    bool ok = false;
    uint value = trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)
                     ? trimmed.mid(2).toUInt(&ok, 16)
                     : trimmed.toUInt(&ok, 10);
    const bool valid = ok && value <= 0xFFFFu;
    setFieldError(edit, !valid);
    if (valid) {
        *out = static_cast<quint16>(value);
    }
    return valid;
}

bool ScopeGeneratorPanel::parseInt16Field(QLineEdit *edit, qint16 *out) {
    if (edit == nullptr || out == nullptr) {
        return false;
    }

    const QString trimmed = edit->text().trimmed();
    bool ok = false;
    const int value = trimmed.toInt(&ok, 10);
    const bool valid = ok && value >= -32768 && value <= 32767;
    setFieldError(edit, !valid);
    if (valid) {
        *out = static_cast<qint16>(value);
    }
    return valid;
}

bool ScopeGeneratorPanel::parsePositiveIntField(QLineEdit *edit, int *out) {
    if (edit == nullptr || out == nullptr) {
        return false;
    }

    const QString trimmed = edit->text().trimmed();
    bool ok = false;
    const int value = trimmed.toInt(&ok, 10);
    const bool valid = ok && value > 0;
    setFieldError(edit, !valid);
    if (valid) {
        *out = value;
    }
    return valid;
}

bool ScopeGeneratorPanel::parseDoubleField(QLineEdit *edit, double *out, bool mustBePositive) {
    if (edit == nullptr || out == nullptr) {
        return false;
    }

    const QString trimmed = edit->text().trimmed();
    bool ok = false;
    const double value = trimmed.toDouble(&ok);
    const bool valid = ok && (!mustBePositive || value > 0.0);
    setFieldError(edit, !valid);
    if (valid) {
        *out = value;
    }
    return valid;
}

void ScopeGeneratorPanel::handleStartClicked() {
    clearErrors();
    if (m_linearRadio != nullptr && m_linearRadio->isChecked()) {
        handleLinearStart();
        return;
    }
    handleCosineStart();
}

void ScopeGeneratorPanel::handleLinearStart() {
    quint16 addr = 0;
    qint16 minValue = 0;
    qint16 maxValue = 0;
    qint16 step = 0;
    int intervalMs = 0;

    const bool ok = parseUint16Field(m_linearAddrEdit, &addr)
                    && parseInt16Field(m_linearMinEdit, &minValue)
                    && parseInt16Field(m_linearMaxEdit, &maxValue)
                    && parseInt16Field(m_linearStepEdit, &step)
                    && parsePositiveIntField(m_linearIntervalEdit, &intervalMs);
    if (!ok) {
        return;
    }

    if (maxValue <= minValue) {
        setFieldError(m_linearMinEdit, true);
        setFieldError(m_linearMaxEdit, true);
        return;
    }
    if (step <= 0) {
        setFieldError(m_linearStepEdit, true);
        return;
    }

    emit linearStartRequested(addr, minValue, maxValue, step, intervalMs);
}

void ScopeGeneratorPanel::handleCosineStart() {
    qint16 amplitude = 0;
    qint16 offset = 0;
    double frequencyHz = 0.0;

    const bool globalsOk = parseInt16Field(m_cosineAmplitudeEdit, &amplitude)
                           && parseInt16Field(m_cosineOffsetEdit, &offset)
                           && parseDoubleField(m_cosineFrequencyEdit, &frequencyHz, true);
    if (!globalsOk) {
        return;
    }
    if (amplitude <= 0) {
        setFieldError(m_cosineAmplitudeEdit, true);
        return;
    }

    QVector<ScopeGeneratorCosineChannel> channels;
    channels.reserve(3);
    bool hasChannel = false;
    bool valid = true;
    for (int i = 0; i < 3; ++i) {
        const QString addrText = m_cosineAddrEdits[i]->text().trimmed();
        const QString phaseText = m_cosinePhaseEdits[i]->text().trimmed();
        if (addrText.isEmpty()) {
            setFieldError(m_cosineAddrEdits[i], false);
            setFieldError(m_cosinePhaseEdits[i], false);
            continue;
        }

        quint16 addr = 0;
        double phaseDeg = 0.0;
        const bool channelOk = parseUint16Field(m_cosineAddrEdits[i], &addr)
                               && parseDoubleField(m_cosinePhaseEdits[i], &phaseDeg, false);
        valid = valid && channelOk;
        if (channelOk) {
            hasChannel = true;
            channels.push_back({addr, phaseDeg});
        }
    }

    if (!valid || !hasChannel) {
        if (!hasChannel) {
            setFieldError(m_cosineAddrEdits[0], true);
        }
        return;
    }

    emit cosineStartRequested(amplitude, offset, frequencyHz, channels);
}

void ScopeGeneratorPanel::clearErrors() {
    const QList<QLineEdit *> edits = findChildren<QLineEdit *>();
    for (QLineEdit *edit : edits) {
        setFieldError(edit, false);
    }
}
