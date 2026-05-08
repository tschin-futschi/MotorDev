// =============================================================================
// @file    fwfileinfopanel.cpp
// @brief   固件信息视图实现 — v2 起作为内嵌 QWidget，由父卡片提供 GroupBox 外框
// =============================================================================
#include "widgets/fwfileinfopanel.h"

#include "protocol/firmware_parser.h"
#include "ui/style_constants.h"

#include <QGridLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {

QLabel *makeLabel(QWidget *parent, const QString &text, bool valueRole) {
    auto *l = new QLabel(text, parent);
    l->setObjectName(valueRole ? QStringLiteral("infoValue") : QStringLiteral("infoLabel"));
    QFont f = l->font();
    if (valueRole) f.setFamily(QString::fromLatin1(Style::Font::Monospace));
    f.setPixelSize(11);
    l->setFont(f);
    QPalette p = l->palette();
    p.setColor(QPalette::WindowText,
               valueRole ? Style::Color::FwFlashFileInfoValueFg
                         : Style::Color::FwFlashFileInfoLabelFg);
    l->setPalette(p);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QString formatSize(qint64 bytes) {
    return QStringLiteral("%1 字节 (%2 KB)")
        .arg(bytes)
        .arg(QString::number(bytes / 1024.0, 'f', 1));
}

QString formatHex32(quint32 v) {
    return QStringLiteral("0x%1").arg(v, 8, 16, QLatin1Char('0')).toUpper().replace("0X", "0x");
}

}  // namespace

FwFileInfoPanel::FwFileInfoPanel(QWidget *parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("FwFileInfoPanel"));
    setupUi();
    clear();
}

FwFileInfoPanel::~FwFileInfoPanel() = default;

void FwFileInfoPanel::setupUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("infoStack"));
    root->addWidget(m_stack);

    // 页 0：空状态——单行灰色提示
    auto *emptyPage = new QWidget(m_stack);
    auto *emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    m_emptyLabel = new QLabel(tr("请先选择固件文件"), emptyPage);
    m_emptyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QPalette emptyPal = m_emptyLabel->palette();
    emptyPal.setColor(QPalette::WindowText, Style::Color::MutedText);
    m_emptyLabel->setPalette(emptyPal);
    QFont emptyFont = m_emptyLabel->font();
    emptyFont.setPixelSize(11);
    m_emptyLabel->setFont(emptyFont);
    emptyLayout->addWidget(m_emptyLabel);
    m_stack->addWidget(emptyPage);

    // 页 1：合法字段（网格 2 列）
    auto *validPage = new QWidget(m_stack);
    auto *grid = new QGridLayout(validPage);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(Style::Size::FormSpacing);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);

    int row = 0;
    grid->addWidget(makeLabel(validPage, tr("文件名"), false), row, 0, Qt::AlignLeft);
    m_fileNameValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_fileNameValue, row++, 1, Qt::AlignLeft);

    grid->addWidget(makeLabel(validPage, tr("文件大小"), false), row, 0, Qt::AlignLeft);
    m_fileSizeValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_fileSizeValue, row++, 1, Qt::AlignLeft);

    grid->addWidget(makeLabel(validPage, tr("文件格式"), false), row, 0, Qt::AlignLeft);
    m_formatValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_formatValue, row++, 1, Qt::AlignLeft);

    grid->addWidget(makeLabel(validPage, tr("CRC32"), false), row, 0, Qt::AlignLeft);
    m_crc32Value = makeLabel(validPage, QString(), true);
    grid->addWidget(m_crc32Value, row++, 1, Qt::AlignLeft);

    m_segCountLabel = makeLabel(validPage, tr("段数"), false);
    grid->addWidget(m_segCountLabel, row, 0, Qt::AlignLeft);
    m_segCountValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_segCountValue, row++, 1, Qt::AlignLeft);

    m_addrRangeLabel = makeLabel(validPage, tr("地址范围"), false);
    grid->addWidget(m_addrRangeLabel, row, 0, Qt::AlignLeft);
    m_addrRangeValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_addrRangeValue, row++, 1, Qt::AlignLeft);

    m_effectiveLabel = makeLabel(validPage, tr("有效字节"), false);
    grid->addWidget(m_effectiveLabel, row, 0, Qt::AlignLeft);
    m_effectiveValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_effectiveValue, row++, 1, Qt::AlignLeft);

    grid->setRowStretch(row, 1);
    m_stack->addWidget(validPage);

    // 页 2：错误
    auto *errorPage = new QWidget(m_stack);
    auto *errorLayout = new QVBoxLayout(errorPage);
    errorLayout->setContentsMargins(0, 0, 0, 0);
    m_errorLabel = new QLabel(errorPage);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QPalette errPal = m_errorLabel->palette();
    errPal.setColor(QPalette::WindowText, Style::Color::FwFlashFileInfoErrorFg);
    m_errorLabel->setPalette(errPal);
    QFont errFont = m_errorLabel->font();
    errFont.setPixelSize(11);
    m_errorLabel->setFont(errFont);
    errorLayout->addWidget(m_errorLabel);
    errorLayout->addStretch();
    m_stack->addWidget(errorPage);
}

void FwFileInfoPanel::clear() {
    m_stack->setCurrentIndex(0);
}

void FwFileInfoPanel::setInfo(const FirmwareInfo &info) {
    if (!info.valid) {
        m_errorLabel->setText(tr("解析失败：%1").arg(info.errorMessage));
        m_stack->setCurrentIndex(2);
        return;
    }

    m_fileNameValue->setText(info.fileName);
    m_fileSizeValue->setText(formatSize(info.fileSizeBytes));
    m_formatValue->setText(info.format == FirmwareFormat::Hex
                               ? tr("Intel HEX")
                               : tr("Binary"));
    m_crc32Value->setText(formatHex32(info.crc32));

    const bool isHex = info.format == FirmwareFormat::Hex;
    m_segCountLabel->setVisible(isHex);
    m_segCountValue->setVisible(isHex);
    m_addrRangeLabel->setVisible(isHex);
    m_addrRangeValue->setVisible(isHex);
    m_effectiveLabel->setVisible(isHex);
    m_effectiveValue->setVisible(isHex);

    if (isHex) {
        m_segCountValue->setText(QString::number(info.segments.size()));
        m_addrRangeValue->setText(QStringLiteral("%1 - %2")
                                      .arg(formatHex32(info.minAddress))
                                      .arg(formatHex32(info.maxAddress)));
        m_effectiveValue->setText(QStringLiteral("%1 字节").arg(info.effectiveBytes));
    }

    m_stack->setCurrentIndex(1);
}
