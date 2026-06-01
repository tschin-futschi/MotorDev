// =============================================================================
// @file    aboutdialog.cpp
// @brief   关于对话框实现 — 软件元信息集中定义 + 版面构建
// =============================================================================
#include "widgets/aboutdialog.h"

#include "ui/style_constants.h"

#include <QColor>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {

// --- 软件元信息（集中维护，改版本/作者/支持 IC/协议只改这里）---
constexpr auto kAppName       = "MotorDev";
constexpr auto kAppVersion    = "v1.0.0";
constexpr auto kSupportedIcs  = "AW86008 / AW86100 / DW9786 / DW9788";
constexpr auto kProtocol      = "generator_v1 / v2.11";
constexpr auto kAuthor        = "Qin Fu Qi";
constexpr auto kRepoUrl       = "github.com/tschin-futschi/MotorDev";
constexpr auto kCopyright     = "© 2026  ·  MIT License";

// 版面尺寸
constexpr int kDialogMinWidth  = 380;
constexpr int kTitleFontPx     = 22;
constexpr int kTaglineFontPx   = 12;

// 背景遮罩浓度：0 = 图标全清晰（文字最难读），1 = 全遮住（看不到图标）。
// 取 0.82 让图标作为淡水印透出、同时保证前景文字清晰可读。
constexpr double kBackdropScrimOpacity = 0.82;

}  // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent) {
    setObjectName(QStringLiteral("AboutDialog"));
    setModal(true);
    // 背景图标 = 软件 Logo（与窗口图标 / 启动图 / 顶栏 Logo 同源）
    m_bgRenderer = new QSvgRenderer(MotorDev::Style::Text::LogoResource, this);
    setupUi();
}

AboutDialog::~AboutDialog() = default;

void AboutDialog::setupUi() {
    setWindowTitle(tr("关于 %1").arg(QString::fromLatin1(kAppName)));
    setMinimumWidth(kDialogMinWidth);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(10);

    // --- 应用名（大字、加粗、居中）---
    auto *titleLabel = new QLabel(QString::fromLatin1(kAppName), this);
    titleLabel->setObjectName(QStringLiteral("aboutTitle"));
    titleLabel->setAlignment(Qt::AlignHCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(kTitleFontPx);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    root->addWidget(titleLabel);

    // --- 一句话定位（居中、弱化）---
    auto *taglineLabel = new QLabel(tr("电机驱动模组调试上位机"), this);
    taglineLabel->setObjectName(QStringLiteral("aboutTagline"));
    taglineLabel->setAlignment(Qt::AlignHCenter);
    QFont taglineFont = taglineLabel->font();
    taglineFont.setPixelSize(kTaglineFontPx);
    taglineLabel->setFont(taglineFont);
    root->addWidget(taglineLabel);

    // --- 分隔线 ---
    auto *divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("aboutDivider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    root->addWidget(divider);

    // --- 信息表：版本 / 构建 / 支持 IC / 通信协议 / 作者 ---
    auto *form = new QFormLayout();
    form->setObjectName(QStringLiteral("aboutForm"));
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(8);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 值可选中复制
    auto *valueIcs = new QLabel(QString::fromLatin1(kSupportedIcs), this);
    valueIcs->setWordWrap(true);
    auto makeValue = [this](const QString &text) {
        auto *l = new QLabel(text, this);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return l;
    };

    const QString buildInfo =
        QStringLiteral("Qt %1 · %2").arg(QString::fromLatin1(QT_VERSION_STR),
                                         QString::fromLatin1(__DATE__));

    form->addRow(tr("版本"), makeValue(QString::fromLatin1(kAppVersion)));
    form->addRow(tr("构建"), makeValue(buildInfo));
    valueIcs->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("支持 IC"), valueIcs);
    form->addRow(tr("通信协议"), makeValue(QString::fromLatin1(kProtocol)));
    form->addRow(tr("作者"), makeValue(QString::fromLatin1(kAuthor)));
    root->addLayout(form);

    // --- 德语题词（固定德语，两种界面语言下均原样显示，不进 i18n）---
    auto *quoteDivider = new QFrame(this);
    quoteDivider->setObjectName(QStringLiteral("aboutQuoteDivider"));
    quoteDivider->setFrameShape(QFrame::HLine);
    quoteDivider->setFrameShadow(QFrame::Sunken);
    root->addWidget(quoteDivider);

    auto makeQuote = [this](const QString &text) {
        auto *l = new QLabel(text, this);
        l->setObjectName(QStringLiteral("aboutQuote"));
        l->setAlignment(Qt::AlignHCenter);
        l->setWordWrap(true);
        QFont f = l->font();
        f.setItalic(true);
        l->setFont(f);
        return l;
    };
    root->addWidget(makeQuote(QStringLiteral(
        "„Antworte mir: Worin liegt der Sinn des Daseins?“")));
    root->addWidget(makeQuote(QStringLiteral(
        "„Bist du heute ein anderer geworden – "
        "oder bloß um einen Tag gealtert?“")));

    // --- 版权 + 仓库（居中、弱化，可选中复制仓库地址）---
    auto *copyrightLabel = new QLabel(QString::fromLatin1(kCopyright), this);
    copyrightLabel->setObjectName(QStringLiteral("aboutCopyright"));
    copyrightLabel->setAlignment(Qt::AlignHCenter);
    root->addWidget(copyrightLabel);

    auto *repoLabel = new QLabel(QString::fromLatin1(kRepoUrl), this);
    repoLabel->setObjectName(QStringLiteral("aboutRepo"));
    repoLabel->setAlignment(Qt::AlignHCenter);
    repoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(repoLabel);

    // --- 确定按钮（右对齐）---
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    if (QPushButton *okBtn = buttons->button(QDialogButtonBox::Ok)) {
        okBtn->setText(tr("确定"));
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &AboutDialog::accept);
    root->addSpacing(4);
    root->addWidget(buttons);
}

void AboutDialog::paintEvent(QPaintEvent * /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 1. 软件图标全幅展开：拉伸 SVG 填满整个对话框矩形
    if (m_bgRenderer != nullptr && m_bgRenderer->isValid()) {
        m_bgRenderer->render(&painter, rect());
    }
    // 2. 半透明遮罩（取主题窗口底色）压低图标对比度，保证前景文字可读
    QColor scrim = palette().color(QPalette::Window);
    scrim.setAlphaF(kBackdropScrimOpacity);
    painter.fillRect(rect(), scrim);
    // 前景控件（标题/信息表/按钮）由各自 widget 叠加绘制在其上
}
