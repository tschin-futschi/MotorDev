#include "widgets/registertable.h"

#include "ui/style_constants.h"
#include "ui/repolish.h"
#include "ui_registertable.h"

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

using namespace MotorDev;

namespace {
class GroupDividerDelegate : public QStyledItemDelegate {
public:
    explicit GroupDividerDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyledItemDelegate::paint(painter, option, index);
        if (index.column() > 0 && index.column() % 5 == 0) {
            painter->save();
            painter->setPen(QPen(Style::Color::GroupDivider, Style::Size::TableGroupDividerWidth));
            painter->drawLine(option.rect.topLeft(), option.rect.bottomLeft());
            painter->restore();
        }
    }
};

constexpr int TotalRows = Style::Size::TableGroupCount * Style::Size::TableRowCount;

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

bool parseRegisterValueText(const QString &text, qint16 *value) {
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

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

RegisterTable::RegisterTable(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::RegisterTable>()) {
    setupUi();
    connectSignals();
}

RegisterTable::~RegisterTable() = default;

void RegisterTable::setupUi() {
    ui->setupUi(this);

    auto *table = ui->tableWidget;
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setVisible(true);
    table->verticalHeader()->setDefaultSectionSize(Style::Size::TableRowHeight);
    table->horizontalHeader()->setFixedHeight(Style::Size::TableHeaderHeight);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->setItemDelegate(new GroupDividerDelegate(table));

    const QStringList groupHeaders = {tr("描述"), tr("地址"), tr("值"), QStringLiteral("R"), QStringLiteral("W")};
    for (int group = 0; group < Style::Size::TableGroupCount; ++group) {
        for (int column = 0; column < groupHeaders.size(); ++column) {
            table->setHorizontalHeaderItem(group * 5 + column, new QTableWidgetItem(groupHeaders[column]));
        }
    }

    QFont monoFont(QStringLiteral("Consolas"));
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(11);

    for (int row = 0; row < Style::Size::TableRowCount; ++row) {
        for (int group = 0; group < Style::Size::TableGroupCount; ++group) {
            const int globalRow = group * Style::Size::TableRowCount + row;

            auto *descItem = new QTableWidgetItem(QString{});
            descItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            table->setItem(row, descCol(group), descItem);

            auto *addrItem = new QTableWidgetItem(QString{});
            addrItem->setTextAlignment(Qt::AlignCenter);
            addrItem->setFont(monoFont);
            table->setItem(row, addrCol(group), addrItem);

            auto *valueItem = new QTableWidgetItem(QString{});
            valueItem->setTextAlignment(Qt::AlignCenter);
            valueItem->setFont(monoFont);
            valueItem->setForeground(QBrush(Style::Color::RegisterValueText));
            table->setItem(row, valueCol(group), valueItem);

            auto *readButton = new QPushButton(QStringLiteral("R"), table);
            readButton->setFixedHeight(Style::Size::TableRowHeight - 2);
            readButton->setProperty("buttonType", QStringLiteral("read"));
            table->setCellWidget(row, readCol(group), readButton);
            m_readButtons[globalRow] = readButton;

            auto *writeButton = new QPushButton(QStringLiteral("W"), table);
            writeButton->setFixedHeight(Style::Size::TableRowHeight - 2);
            writeButton->setProperty("buttonType", QStringLiteral("write"));
            table->setCellWidget(row, writeCol(group), writeButton);
            m_writeButtons[globalRow] = writeButton;
        }
    }

    applyColumnWidths();
}

void RegisterTable::connectSignals() {
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

    connect(ui->tableWidget, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (item == nullptr) {
            return;
        }
        const int columnMod = item->column() % 5;
        if (columnMod == 2) {
            qint16 value = 0;
            if (parseRegisterValueText(item->text(), &value)) {
                const int group = item->column() / 5;
                const int globalRow = group * Style::Size::TableRowCount + item->row();
                updateRowValue(globalRow, value);
            }
        }
        if (columnMod == 0 || columnMod == 1 || columnMod == 2) {
            emit configChanged();
        }
    });
}

void RegisterTable::applyColumnWidths() {
    auto *table = ui->tableWidget;
    for (int group = 0; group < Style::Size::TableGroupCount; ++group) {
        table->setColumnWidth(descCol(group), Style::Size::ColDescWidth);
        table->setColumnWidth(addrCol(group), Style::Size::ColAddrWidth);
        table->setColumnWidth(valueCol(group), Style::Size::ColValueWidth);
        table->setColumnWidth(readCol(group), Style::Size::ColReadWidth);
        table->setColumnWidth(writeCol(group), Style::Size::ColWriteWidth);
    }
}

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

void RegisterTable::setValueMode(ValueMode mode) {
    if (mode == m_valueMode) {
        return;
    }

    const ValueMode oldMode = m_valueMode;
    m_valueMode = mode;

    for (int globalRow = 0; globalRow < TotalRows; ++globalRow) {
        const int group = globalRow / Style::Size::TableRowCount;
        const int row = globalRow % Style::Size::TableRowCount;
        auto *item = ui->tableWidget->item(row, valueCol(group));
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

void RegisterTable::updateRowValue(int globalRow, qint16 value) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = ui->tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return;
    }
    const QSignalBlocker blocker(ui->tableWidget);
    if (m_valueMode == ValueMode::Hex) {
        const quint16 rawValue = static_cast<quint16>(value);
        item->setText(QStringLiteral("0x") + QString::number(rawValue, 16).toUpper().rightJustified(4, QLatin1Char('0')));
    } else {
        item->setText(QString::number(value));
    }
    item->setForeground(QBrush(Style::Color::RegisterValueText));
}

void RegisterTable::markRowError(int globalRow) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = ui->tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return;
    }
    const QSignalBlocker blocker(ui->tableWidget);
    item->setText(QStringLiteral("--"));
    item->setForeground(QBrush(Style::Color::RegisterErrorText));
}

void RegisterTable::markRowPending(int globalRow) {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = ui->tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return;
    }
    const QSignalBlocker blocker(ui->tableWidget);
    item->setText(QStringLiteral("..."));
    item->setForeground(QBrush(Style::Color::MutedText));
}

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

bool RegisterTable::rowHasAddress(int globalRow) const {
    if (globalRow < 0 || globalRow >= TotalRows) {
        return false;
    }
    const int group = globalRow / Style::Size::TableRowCount;
    const int row = globalRow % Style::Size::TableRowCount;
    auto *item = ui->tableWidget->item(row, addrCol(group));
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
    auto *item = ui->tableWidget->item(row, valueCol(group));
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
    auto *item = ui->tableWidget->item(row, addrCol(group));
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
    auto *item = ui->tableWidget->item(row, valueCol(group));
    if (item == nullptr) {
        return 0;
    }
    qint16 value = 0;
    return parseRegisterValueText(item->text(), &value) ? value : 0;
}

void RegisterTable::saveConfig(const QString &path) const {
    QJsonArray registers;
    for (int globalRow = 0; globalRow < TotalRows; ++globalRow) {
        const int group = globalRow / Style::Size::TableRowCount;
        const int row = globalRow % Style::Size::TableRowCount;
        QJsonObject entry;
        auto *descItem = ui->tableWidget->item(row, descCol(group));
        auto *addrItem = ui->tableWidget->item(row, addrCol(group));
        auto *valueItem = ui->tableWidget->item(row, valueCol(group));
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
    const QSignalBlocker blocker(ui->tableWidget);
    for (int globalRow = 0; globalRow < TotalRows && globalRow < registers.size(); ++globalRow) {
        const int group = globalRow / Style::Size::TableRowCount;
        const int row = globalRow % Style::Size::TableRowCount;
        const QJsonObject entry = registers.at(globalRow).toObject();
        if (auto *descItem = ui->tableWidget->item(row, descCol(group)); descItem != nullptr) {
            descItem->setText(entry.value(QStringLiteral("desc")).toString());
        }
        if (auto *addrItem = ui->tableWidget->item(row, addrCol(group)); addrItem != nullptr) {
            addrItem->setText(entry.value(QStringLiteral("addr")).toString());
        }
        if (auto *valueItem = ui->tableWidget->item(row, valueCol(group)); valueItem != nullptr) {
            valueItem->setText(entry.value(QStringLiteral("val")).toString());
        }
    }
}