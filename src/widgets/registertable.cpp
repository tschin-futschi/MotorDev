// =============================================================================
// @file    registertable.cpp
// @brief   寄存器表格实现 — 固定 TableRowCount 行、值解析、状态反馈、JSON 持久化
// =============================================================================
#include "widgets/registertable.h"

#include "protocol/register_utils.h"
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

/// @brief 总行数 = TableGroupCount × TableRowCount
constexpr int TotalRows = Style::Size::TableGroupCount * Style::Size::TableRowCount;

/// @brief 解析地址文本（支持 "0x" 前缀的十六进制）
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
    m_tableWidget->setRowCount(0);                          // 由 appendOneRow 逐行追加到 TableRowCount
    m_tableWidget->setColumnCount(Style::Size::TableGroupCount * 5);
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

    // 创建固定 TableRowCount 行
    for (int i = 0; i < Style::Size::TableRowCount; ++i) {
        appendOneRow();
    }

    applyColumnWidths();
}

// =============================================================================
// 单行构造（构造时调用 TableRowCount 次）
// =============================================================================

/// @brief 追加一行（4 组 × 5 列 items + R/W 按钮 + 按钮 clicked 信号连接）
void RegisterTable::appendOneRow() {
    const int newRow = m_rowCount;
    m_tableWidget->setRowCount(newRow + 1);

    QFont monoFont(QLatin1String(Style::Font::Monospace));
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(Style::Size::RegisterTableFontSize);

    // 初始化 cell 不应触发 configChanged → 阻塞 itemChanged
    const QSignalBlocker blocker(m_tableWidget);

    for (int group = 0; group < Style::Size::TableGroupCount; ++group) {
        // 描述列（可编辑文本）
        auto *descItem = new QTableWidgetItem(QString{});
        descItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        m_tableWidget->setItem(newRow, descCol(group), descItem);

        // 地址列（等宽、居中）
        auto *addrItem = new QTableWidgetItem(QString{});
        addrItem->setTextAlignment(Qt::AlignCenter);
        addrItem->setFont(monoFont);
        m_tableWidget->setItem(newRow, addrCol(group), addrItem);

        // 值列（等宽、居中、特殊颜色）
        auto *valueItem = new QTableWidgetItem(QString{});
        valueItem->setTextAlignment(Qt::AlignCenter);
        valueItem->setFont(monoFont);
        valueItem->setForeground(QBrush(Style::Color::RegisterValueText));
        m_tableWidget->setItem(newRow, valueCol(group), valueItem);

        // R 按钮
        auto *readButton = new QPushButton(QStringLiteral("R"), m_tableWidget);
        readButton->setFixedHeight(Style::Size::TableRowHeight - 2);
        readButton->setProperty("buttonType", QStringLiteral("read"));
        readButton->setEnabled(m_connected);
        m_tableWidget->setCellWidget(newRow, readCol(group), readButton);

        // W 按钮
        auto *writeButton = new QPushButton(QStringLiteral("W"), m_tableWidget);
        writeButton->setFixedHeight(Style::Size::TableRowHeight - 2);
        writeButton->setProperty("buttonType", QStringLiteral("write"));
        writeButton->setEnabled(m_connected);
        m_tableWidget->setCellWidget(newRow, writeCol(group), writeButton);

        m_readButtons.append(readButton);
        m_writeButtons.append(writeButton);

        const int capturedGroup = group;
        const int capturedTableRow = newRow;
        connect(readButton, &QPushButton::clicked, this, [this, capturedGroup, capturedTableRow]() {
            const int globalRow = capturedGroup * m_rowCount + capturedTableRow;
            if (rowHasAddress(globalRow)) {
                emit readRowRequested(globalRow, rowAddress(globalRow));
            }
        });
        connect(writeButton, &QPushButton::clicked, this, [this, capturedGroup, capturedTableRow]() {
            const int globalRow = capturedGroup * m_rowCount + capturedTableRow;
            if (rowHasAddress(globalRow) && rowHasValue(globalRow)) {
                emit writeRowRequested(globalRow, rowAddress(globalRow), rowValue(globalRow));
            }
        });
    }

    ++m_rowCount;
}

// =============================================================================
// 信号槽连接（仅表格 itemChanged；按钮信号在 appendOneRow 内单行连接）
// =============================================================================

void RegisterTable::connectSignals() {
    // 单元格编辑完成 → 值自动格式化 + configChanged 信号
    connect(m_tableWidget, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (item == nullptr) {
            return;
        }
        const int columnMod = item->column() % 5;
        // 值列编辑 → 自动格式化为当前 ValueMode
        if (columnMod == 2) {
            qint16 value = 0;
            if (RegisterUtils::parseSignedValue(item->text(), &value)) {
                const int group = item->column() / 5;
                const int globalRow = group * m_rowCount + item->row();
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
///
/// R / W 两列尺寸固定（按钮紧凑），每组的「描述 / 地址 / 值」三列改为
/// QHeaderView::Stretch — Qt 会将 viewport 富余宽度均分给所有 Stretch 列，
/// 形成 4 组 × 3 列 = 12 列等宽分布，描述 : 地址 : 值 = 1 : 1 : 1。
void RegisterTable::applyColumnWidths() {
    auto *table = m_tableWidget;
    auto *header = table->horizontalHeader();
    for (int group = 0; group < Style::Size::TableGroupCount; ++group) {
        header->setSectionResizeMode(descCol(group), QHeaderView::Stretch);
        header->setSectionResizeMode(addrCol(group), QHeaderView::Stretch);
        header->setSectionResizeMode(valueCol(group), QHeaderView::Stretch);
        table->setColumnWidth(readCol(group), Style::Size::ColReadWidth);
        table->setColumnWidth(writeCol(group), Style::Size::ColWriteWidth);
    }
}

// =============================================================================
// 状态控制
// =============================================================================

void RegisterTable::setConnected(bool connected) {
    m_connected = connected;
    for (auto *button : m_readButtons) {
        if (button != nullptr) {
            button->setEnabled(connected);
        }
    }
    for (auto *button : m_writeButtons) {
        if (button != nullptr) {
            button->setEnabled(connected);
        }
    }
}

/// @brief 切换 Hex/Dec 模式，遍历所有行重新格式化已有值
void RegisterTable::setValueMode(ValueMode mode) {
    if (mode == m_valueMode) {
        return;
    }

    m_valueMode = mode;

    for (int globalRow = 0; globalRow < TotalRows; ++globalRow) {
        const int group = globalRow / m_rowCount;
        const int row = globalRow % m_rowCount;
        auto *item = m_tableWidget->item(row, valueCol(group));
        if (item == nullptr) {
            continue;
        }

        const QString text = item->text().trimmed();
        if (text.isEmpty() || text == QStringLiteral("--") || text == QStringLiteral("...")) {
            continue;
        }

        qint16 signedValue = 0;
        if (RegisterUtils::parseSignedValue(text, &signedValue)) {
            updateRowValue(globalRow, signedValue);
        }
    }
}

// =============================================================================
// 行值更新与状态反馈
// =============================================================================

void RegisterTable::updateRowValue(int globalRow, qint16 value) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }
    const int group = globalRow / m_rowCount;
    const int row = globalRow % m_rowCount;
    auto *item = m_tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return;
    }
    const QSignalBlocker blocker(m_tableWidget);
    item->setText(RegisterUtils::formatValue(value, m_valueMode == ValueMode::Hex));
    item->setForeground(QBrush(Style::Color::RegisterValueText));
}

void RegisterTable::markRowError(int globalRow) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }
    const int group = globalRow / m_rowCount;
    const int row = globalRow % m_rowCount;
    auto *item = m_tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return;
    }
    const QSignalBlocker blocker(m_tableWidget);
    item->setText(QStringLiteral("--"));
    item->setForeground(QBrush(Style::Color::RegisterErrorText));
}

void RegisterTable::markRowPending(int globalRow) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }
    const int group = globalRow / m_rowCount;
    const int row = globalRow % m_rowCount;
    auto *item = m_tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return;
    }
    const QSignalBlocker blocker(m_tableWidget);
    item->setText(QStringLiteral("..."));
    item->setForeground(QBrush(Style::Color::MutedText));
}

void RegisterTable::markWriteButtonFeedback(int globalRow, bool success) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }
    const int group = globalRow / m_rowCount;
    const int row = globalRow % m_rowCount;
    const int idx = buttonIndex(group, row);
    if (idx < 0 || idx >= m_writeButtons.size()) {
        return;
    }
    auto *button = m_writeButtons.at(idx);
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
    const int group = globalRow / m_rowCount;
    const int row = globalRow % m_rowCount;
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
    const int group = globalRow / m_rowCount;
    const int row = globalRow % m_rowCount;
    auto *item = m_tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return false;
    }
    qint16 value = 0;
    return RegisterUtils::parseSignedValue(item->text(), &value);
}

quint16 RegisterTable::rowAddress(int globalRow) const {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return 0;
    }
    const int group = globalRow / m_rowCount;
    const int row = globalRow % m_rowCount;
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
    const int group = globalRow / m_rowCount;
    const int row = globalRow % m_rowCount;
    auto *item = m_tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return 0;
    }
    qint16 value = 0;
    return RegisterUtils::parseSignedValue(item->text(), &value) ? value : 0;
}

// =============================================================================
// JSON 持久化（固定 TableRowCount × TableGroupCount 条）
// =============================================================================

/// @brief 保存所有行到 JSON：按 group-major × row 顺序，共 TotalRows 条
void RegisterTable::saveConfig(const QString &path) const {
    QJsonArray registers;
    for (int group = 0; group < Style::Size::TableGroupCount; ++group) {
        for (int row = 0; row < Style::Size::TableRowCount; ++row) {
            QJsonObject entry;
            auto *descItem = m_tableWidget->item(row, descCol(group));
            auto *addrItem = m_tableWidget->item(row, addrCol(group));
            auto *valueItem = m_tableWidget->item(row, valueCol(group));
            entry[QStringLiteral("desc")] = descItem != nullptr ? descItem->text() : QString{};
            entry[QStringLiteral("addr")] = addrItem != nullptr ? addrItem->text() : QString{};
            entry[QStringLiteral("val")] = valueItem != nullptr ? valueItem->text() : QString{};
            registers.append(entry);
        }
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("registers")] = registers;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

/// @brief 从 JSON 加载描述/地址/值到表格
///
/// 兼容不同版本：按 jsonSize / TableGroupCount 推断旧文件 rowsPerGroup，
/// 加载到表格前 min(rowsPerGroup, TableRowCount) 行。
/// 若 jsonSize 不是 TableGroupCount 的整数倍，放弃加载。
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
    const int jsonSize = registers.size();
    if (jsonSize <= 0) {
        return;
    }

    const int rowsPerGroupInJson = jsonSize / Style::Size::TableGroupCount;
    if (rowsPerGroupInJson <= 0
        || rowsPerGroupInJson * Style::Size::TableGroupCount != jsonSize) {
        return;
    }

    const QSignalBlocker blocker(m_tableWidget);
    for (int group = 0; group < Style::Size::TableGroupCount; ++group) {
        const int rowsToLoad = qMin(rowsPerGroupInJson, Style::Size::TableRowCount);
        for (int row = 0; row < rowsToLoad; ++row) {
            const int jsonIdx = group * rowsPerGroupInJson + row;
            const QJsonObject entry = registers.at(jsonIdx).toObject();
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
}
