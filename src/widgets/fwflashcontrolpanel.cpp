// =============================================================================
// @file    fwflashcontrolpanel.cpp
// @brief   烧录控制面板实现 — v2 紧凑布局：按钮行 + 阶段标签同行，进度条第二行
// =============================================================================
#include "widgets/fwflashcontrolpanel.h"

#include "ui/style_constants.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

using namespace MotorDev;

FwFlashControlPanel::FwFlashControlPanel(QWidget *parent)
    : QGroupBox(parent) {
    setTitle(tr("烧录控制"));
    setObjectName(QStringLiteral("FwFlashControlPanel"));
    setupUi();
    resetProgress();
    setStageText(tr("空闲"));
    setStartEnabled(false);
    setCancelEnabled(false);
}

FwFlashControlPanel::~FwFlashControlPanel() = default;

void FwFlashControlPanel::setupUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(Style::Size::ContentSpacing,
                             Style::Size::ContentSpacing + Style::Size::GroupBoxTopMargin,
                             Style::Size::ContentSpacing,
                             Style::Size::ContentSpacing);
    root->setSpacing(8);

    // 第一行：开始 + 取消 + 阶段标签（拉伸吃掉剩余）
    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(8);
    root->addLayout(btnRow);

    m_startBtn = new QPushButton(tr("开始烧录"), this);
    m_startBtn->setObjectName(QStringLiteral("fwFlashStartButton"));
    m_startBtn->setMinimumSize(QSize(Style::Size::FwFlashStartButtonW,
                                      Style::Size::FwFlashStartButtonH));
    btnRow->addWidget(m_startBtn);

    m_cancelBtn = new QPushButton(tr("取消烧录"), this);
    m_cancelBtn->setObjectName(QStringLiteral("fwFlashCancelButton"));
    m_cancelBtn->setMinimumSize(QSize(Style::Size::FwFlashCancelButtonW,
                                       Style::Size::FwFlashCancelButtonH));
    btnRow->addWidget(m_cancelBtn);

    m_stageLabel = new QLabel(this);
    m_stageLabel->setObjectName(QStringLiteral("fwFlashStageLabel"));
    QFont f = m_stageLabel->font();
    f.setPixelSize(11);
    m_stageLabel->setFont(f);
    QPalette pal = m_stageLabel->palette();
    pal.setColor(QPalette::WindowText, Style::Color::FwFlashStageLabelFg);
    m_stageLabel->setPalette(pal);
    m_stageLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    btnRow->addWidget(m_stageLabel, 1);

    // 第二行：进度条（现代化绿色 QSS：圆角 + 渐变 chunk）
    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("fwFlashProgress"));
    m_progress->setMinimumHeight(Style::Size::FwFlashProgressH);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setAlignment(Qt::AlignCenter);
    m_progress->setStyleSheet(QStringLiteral(
        "QProgressBar#fwFlashProgress {"
        "  border: 1px solid %1;"
        "  border-radius: 5px;"
        "  background: %2;"
        "  text-align: center;"
        "  color: %3;"
        "  font-size: 11px;"
        "}"
        "QProgressBar#fwFlashProgress::chunk {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "                                     stop:0 %4, stop:1 %5);"
        "  border-radius: 4px;"
        "  margin: 1px;"
        "}")
        .arg(Style::Color::DefaultBorder.name())
        .arg(Style::Color::WindowBackground.name())
        .arg(Style::Color::AppText.name())
        .arg(Style::Color::FwFlashProgressChunkStart.name())
        .arg(Style::Color::FwFlashProgressChunkEnd.name()));
    root->addWidget(m_progress);

    connect(m_startBtn, &QPushButton::clicked, this, &FwFlashControlPanel::startRequested);
    connect(m_cancelBtn, &QPushButton::clicked, this, &FwFlashControlPanel::cancelRequested);
}

void FwFlashControlPanel::setStartEnabled(bool enabled) {
    m_startBtn->setEnabled(enabled);
}

void FwFlashControlPanel::setCancelEnabled(bool enabled) {
    m_cancelBtn->setEnabled(enabled);
}

void FwFlashControlPanel::setProgress(qint64 sentBytes, qint64 totalBytes) {
    if (totalBytes <= 0) {
        m_progress->setValue(0);
        return;
    }
    const qint64 raw = sentBytes * 100 / totalBytes;
    const int pct = static_cast<int>(qBound<qint64>(qint64{0}, raw, qint64{100}));
    m_progress->setValue(pct);
}

void FwFlashControlPanel::setStageText(const QString &text) {
    m_stageLabel->setText(text);
    // 与当前语言的"空闲"比较，记住是否处于空闲默认态，供语言切换时重译
    m_stageIsIdle = (text == tr("空闲"));
}

void FwFlashControlPanel::resetProgress() {
    m_progress->setValue(0);
}

void FwFlashControlPanel::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QGroupBox::changeEvent(event);
}

void FwFlashControlPanel::retranslateUi() {
    setTitle(tr("烧录控制"));
    if (m_startBtn != nullptr) {
        m_startBtn->setText(tr("开始烧录"));
    }
    if (m_cancelBtn != nullptr) {
        m_cancelBtn->setText(tr("取消烧录"));
    }
    // 阶段文字若处于空闲默认态，随语言切换重译；进行中的真实阶段消息保持原状
    if (m_stageIsIdle && m_stageLabel != nullptr) {
        m_stageLabel->setText(tr("空闲"));
    }
}
