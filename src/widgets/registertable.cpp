// =============================================================================
// @file    registertable.cpp
// @brief   寄存器表格实现 — 表格初始化、值解析、状态反馈、JSON 持久化
// =============================================================================
#include "widgets/registertable.h"

#include "ui/style_constants.h"
#include "ui/repolish.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QFile>
#include <QFont>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {

/// @brief 组间分隔线委托 — 在每组第一列左侧绘制竖线作为视觉分隔
class GroupDividerDelegate : public QStyledItemDelegate {
public:
    explicit GroupDividerDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyledItemDelegate::paint(painter, option, index);
        // 每 5 列为一组，在第 5/10/15 列左侧画分隔线
        if (index.column() > 0 && index.column() % 5 == 0) {
            painter->save();
            painter->setPen(QPen(Style::Color::GroupDivider, Style::Size::TableGroupDividerWidth));
            painter->drawLine(option.rect.topLeft(), option.rect.bottomLeft());
            painter->restore();
        }
    }
};

/// @brief 总行数（4 组 × 20 行 = 80）
constexpr int TotalRows = Style::Size::TableGroupCount * Style::Size::TableRowCount;

/// @brief 解析地址文本（支持 "0x" 前缀的十六进制）
/// @param text 输入文本
/// @param value 输出地址值
/// @return true=解析成功
bool parseAddressText(const QString &text, quint16 *value) {
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        trimmed.remove(0, 2);
    }

    bool ok = false;
    const quint16 parsed = trimmed.toUShort(&ok, 16);
    if (!ok) {
        return false;
    }

    if (value != nullptr) {
        *value = parsed;
    }
    return true;
}

/// @brief 解析寄存器值文本（支持 "0x" 十六进制和十进制）
/// @param text 输入文本
/// @param value 输出有符号 16 位值
/// @return true=解析成功
bool parseRegisterValueText(const QString &text, qint16 *value) {
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    // 十六进制模式
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        trimmed.remove(0, 2);
        bool ok = false;
        const quint16 raw = trimmed.toUShort(&ok, 16);
        if (!ok) {
            return false;
        }
        if (value != nullptr) {
            *value = static_cast<qint16>(raw);
        }
        return true;
    }

    // 十进制模式
    bool ok = false;
    const qint16 parsed = trimmed.toShort(&ok, 10);
    if (!ok) {
        return false;
    }

    if (value != nullptr) {
        *value = parsed;
    }
    return true;
}
} // namespace

// =============================================================================
// 构造 / 析构
// =============================================================================

RegisterTable::RegisterTable(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
}

RegisterTable::~RegisterTable() = default;

// =============================================================================
// UI 构建
// =============================================================================

void RegisterTable::setupUi() {
    setObjectName(QStringLiteral("RegisterTable"));
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setObjectName(QStringLiteral("tableWidget"));
    m_tableWidget->setRowCount(20);
    m_tableWidget->setColumnCount(20);  // 4 组 × 5 列
    m_tableWidget->setShowGrid(true);
    m_tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setFocusPolicy(Qt::StrongFocus);
    rootLayout->addWidget(m_tableWidget);

    auto *table = m_tableWidget;
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setVisible(true);
    table->verticalHeader()->setDefaultSectionSize(Style::Size::TableRowHeight);
    table->horizontalHeader()->setFixedHeight(Style::Size::TableHeaderHeight);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->setItemDelegate(new GroupDividerDelegate(table));

    // 设置列头（每组重复：描述 | 地址 | 值 | R | W）
    const QStringList groupHeaders = {tr("描述"), tr("地址"), tr("值"), QStringLiteral("R"), QStringLiteral("W")};
    for (int group = 0; group < Style::Size::TableGroupCount; ++group) {
        for (int column = 0; column < groupHeaders.size(); ++column) {
            table->setHorizontalHeaderItem(group * 5 + column, new QTableWidgetItem(groupHeaders[column]));
        }
    }

    // 等宽字体用于地址和值列
    QFont monoFont(QLatin1String(Style::Font::Monospace));
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(Style::Size::RegisterTableFontSize);

    // 创建所有单元格和 R/W 按钮
    for (int row = 0; row < Style::Size::TableRowCount; ++row) {
        for (int group = 0; group < Style::Size::TableGroupCount; ++group) {
            const int globalRow = group * Style::Size::TableRowCount + row;

            // 描述列（可编辑文本）
            auto *descItem = new QTableWidgetItem(QString{});
            descItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            table->setItem(row, descCol(group), descItem);

            // 地址列（等宽、居中）
            auto *addrItem = new QTableWidgetItem(QString{});
            addrItem->setTextAlignment(Qt::AlignCenter);
            addrItem->setFont(monoFont);
            table->setItem(row, addrCol(group), addrItem);

            // 值列（等宽、居中、特殊颜色）
            auto *valueItem = new QTableWidgetItem(QString{});
            valueItem->setTextAlignment(Qt::AlignCenter);
            valueItem->setFont(monoFont);
            valueItem->setForeground(QBrush(Style::Color::RegisterValueText));
            table->setItem(row, valueCol(group), valueItem);

            // R 按钮
            auto *readButton = new QPushButton(QStringLiteral("R"), table);
            readButton->setFixedHeight(Style::Size::TableRowHeight - 2);
            readButton->setProperty("buttonType", QStringLiteral("read"));
            table->setCellWidget(row, readCol(group), readButton);
            m_readButtons[globalRow] = readButton;

            // W 按钮
            auto *writeButton = new QPushButton(QStringLiteral("W"), table);
            writeButton->setFixedHeight(Style::Size::TableRowHeight - 2);
            writeButton->setProperty("buttonType", QStringLiteral("write"));
            table->setCellWidget(row, writeCol(group), writeButton);
            m_writeButtons[globalRow] = writeButton;
        }
    }

    applyColumnWidths();
}

// =============================================================================
// 信号槽连接
// =============================================================================

void RegisterTable::connectSignals() {
    // R/W 按钮 → readRowRequested / writeRowRequested
    for (int globalRow = 0; globalRow < TotalRows; ++globalRow) {
        connect(m_readButtons[globalRow], &QPushButton::clicked, this, [this, globalRow]() {
            if (rowHasAddress(globalRow)) {
                emit readRowRequested(globalRow, rowAddress(globalRow));
            }
        });
        connect(m_writeButtons[globalRow], &QPushButton::clicked, this, [this, globalRow]() {
            if (rowHasAddress(globalRow) && rowHasValue(globalRow)) {
                emit writeRowRequested(globalRow, rowAddress(globalRow), rowValue(globalRow));
            }
        });
    }

    // 单元格编辑完成 → 值自动格式化 + configChanged 信号
    connect(m_tableWidget, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (item == nullptr) {
            return;
        }
        const int columnMod = item->column() % 5;
        // 值列编辑 → 自动格式化为当前 ValueMode
        if (columnMod == 2) {
            qint16 value = 0;
            if (parseRegisterValueText(item->text(), &value)) {
                const int group = item->column() / 5;
                const int globalRow = group * Style::Size::TableRowCount + item->row();
                updateRowValue(globalRow, value);
            }
        }
        // 描述/地址/值任意变化 → 触发自动保存
        if (columnMod == 0 || columnMod == 1 || columnMod == 2) {
            emit configChanged();
        }
    });
}

/// @brief 设置各列宽度
void RegisterTable::applyColumnWidths() {
    auto *table = m_tableWidget;
    for (int group = 0; group < Style::Size::TableGroupCount; ++group) {
        table->setColumnWidth(descCol(group), Style::Size::ColDescWidth);
        table->setColumnWidth(addrCol(group), Style::Size::ColAddrWidth);
        table->setColumnWidth(valueCol(group), Style::Size::ColValueWidth);
        table->setColumnWidth(readCol(group), Style::Size::ColReadWidth);
        table->setColumnWidth(writeCol(group), Style::Size::ColWriteWidth);
    }
}

// =============================================================================
// 状态控制
// =============================================================================

void RegisterTable::setConnected(bool connected) {
    for (int i = 0; i < TotalRows; ++i) {
        if (m_readButtons[i] != nullptr) {
            m_readButtons[i]->setEnabled(connected);
        }
        if (m_writeButtons[i] != nullptr) {
            m_writeButtons[i]->setEnabled(connected);
        }
    }
}

/// @brief 切换 Hex/Dec 模式，遍历所有行重新格式化已有值
void RegisterTable::setValueMode(ValueMode mode) {
    if (mode == m_valueMode) {
        return;
    }

    const ValueMode oldMode = m_valueMode;
    m_valueMode = mode;

    for (int globalRow = 0; globalRow < TotalRows; ++globalRow) {
        const int group = globalRow / Style::Size::TableRowCount;
        const int row = globalRow % Style::Size::TableRowCount;
        auto *item = m_tableWidget->item(row, valueCol(group));
        if (item == nullptr) {
            continue;
        }

        const QString text = item->text().trimmed();
        if (text.isEmpty() || text == QStringLiteral("--") || text == QStringLiteral("...")) {
            continue;
        }

        qint16 signedValue = 0;
        Q_UNUSED(oldMode);
        if (parseRegisterValueText(text, &signedValue)) {
            updateRowValue(globalRow, signedValue);
        }
    }
}

// =============================================================================
// 行值更新与状态反馈
// =============================================================================

/// @brief 更新行值显示，根据当前 ValueMode 格式化
void RegisterTable::updateRowValue(int globalRow, qint16 value) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = m_tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return;
    }
    const QSignalBlocker blocker(m_tableWidget);
    if (m_valueMode == ValueMode::Hex) {
        const quint16 rawValue = static_cast<quint16>(value);
        item->setText(QStringLiteral("0x") + QString::number(rawValue, 16).toUpper().rightJustified(4, QLatin1Char('0')));
    } else {
        item->setText(QString::number(value));
    }
    item->setForeground(QBrush(Style::Color::RegisterValueText));
}

/// @brief 标记行为错误状态：显示 "--" 红色文字
void RegisterTable::markRowError(int globalRow) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = m_tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return;
    }
    const QSignalBlocker blocker(m_tableWidget);
    item->setText(QStringLiteral("--"));
    item->setForeground(QBrush(Style::Color::RegisterErrorText));
}

/// @brief 标记行为等待状态：显示 "..." 灰色文字
void RegisterTable::markRowPending(int globalRow) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = m_tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return;
    }
    const QSignalBlocker blocker(m_tableWidget);
    item->setText(QStringLiteral("..."));
    item->setForeground(QBrush(Style::Color::MutedText));
}

/// @brief W 按钮短暂闪烁反馈：200ms 后清除 feedback 属性
void RegisterTable::markWriteButtonFeedback(int globalRow, bool success) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }

    auto *button = m_writeButtons[globalRow];
    if (button == nullptr) {
        return;
    }

    button->setProperty("feedback", success ? QStringLiteral("success") : QStringLiteral("error"));
    UiUtil::repolish(button);

    QTimer::singleShot(200, button, [button]() {
        button->setProperty("feedback", QString());
        UiUtil::repolish(button);
    });
}

// =============================================================================
// 行数据查询
// =============================================================================

bool RegisterTable::rowHasAddress(int globalRow) const {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return false;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = m_tableWidget->item(row, addrCol(group));
    if (item == nullptr || item->text().trimmed().isEmpty()) {
        return false;
    }
    quint16 dummy = 0;
    return parseAddressText(item->text(), &dummy);
}

bool RegisterTable::rowHasValue(int globalRow) const {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return false;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = m_tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return false;
    }
    qint16 value = 0;
    return parseRegisterValueText(item->text(), &value);
}

quint16 RegisterTable::rowAddress(int globalRow) const {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return 0;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = m_tableWidget->item(row, addrCol(group));
    if (item == nullptr) {
        return 0;
    }
    quint16 addr = 0;
    return parseAddressText(item->text(), &addr) ? addr : 0;
}

qint16 RegisterTable::rowValue(int globalRow) const {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return 0;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = m_tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return 0;
    }
    qint16 value = 0;
    return parseRegisterValueText(item->text(), &value) ? value : 0;
}

// =============================================================================
// JSON 持久化
// =============================================================================

/// @brief 保存所有行到 JSON：{version: 1, registers: [{desc, addr, val}, ...]}
void RegisterTable::saveConfig(const QString &path) const {
    QJsonArray registers;
    for (int globalRow = 0; globalRow < TotalRows; ++globalRow) {
        const int group = globalRow / Style::Size::TableRowCount;
        const int row = globalRow % Style::Size::TableRowCount;
        QJsonObject entry;
        auto *descItem = m_tableWidget->item(row, descCol(group));
        auto *addrItem = m_tableWidget->item(row, addrCol(group));
        auto *valueItem = m_tableWidget->item(row, valueCol(group));
        entry[QStringLiteral("desc")] = descItem != nullptr ? descItem->text() : QString{};
        entry[QStringLiteral("addr")] = addrItem != nullptr ? addrItem->text() : QString{};
        entry[QStringLiteral("val")] = valueItem != nullptr ? valueItem->text() : QString{};
        registers.append(entry);
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("registers")] = registers;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

/// @brief 从 JSON 加载寄存器配置到表格（阻塞信号避免触发 configChanged）
void RegisterTable::loadConfig(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return;
    }

    const QJsonArray registers = document.object().value(QStringLiteral("registers")).toArray();
    const QSignalBlocker blocker(m_tableWidget);
    for (int globalRow = 0; globalRow < TotalRows && globalRow < registers.size(); ++globalRow) {
        const int group = globalRow / Style::Size::TableRowCount;
        const int row = globalRow % Style::Size::TableRowCount;
        const QJsonObject entry = registers.at(globalRow).toObject();
        if (auto *descItem = m_tableWidget->item(row, descCol(group)); descItem != nullptr) {
            descItem->setText(entry.value(QStringLiteral("desc")).toString());
        }
        if (auto *addrItem = m_tableWidget->item(row, addrCol(group)); addrItem != nullptr) {
            addrItem->setText(entry.value(QStringLiteral("addr")).toString());
        }
        if (auto *valueItem = m_tableWidget->item(row, valueCol(group)); valueItem != nullptr) {
            valueItem->setText(entry.value(QStringLiteral("val")).toString());
        }
    }
}
