// =============================================================================
// @file    registertable.h
// @brief   寄存器表格控件 — 多组寄存器的地址/值编辑、单行读写、状态反馈
//
// RegisterTable 提供一个 TableRowCount × (TableGroupCount × 5) 的 QTableWidget，
// 分为 TableGroupCount 组（每组 5 列）：
// 每组包含：描述 | 地址 | 值 | R(读按钮) | W(写按钮)
// 行数固定 Style::Size::TableRowCount（30），通过 totalRows() 暴露给外部。
//
// 功能：
// - 单行读写：点击 R/W 按钮发出 readRowRequested/writeRowRequested 信号
// - 值显示模式切换：Hex（0x####）/ Dec（有符号十进制）
// - 行状态反馈：pending（等待中 "..."）、error（失败 "--"）、写入反馈闪烁
// - 配置持久化：保存到 JSON，记录数 = TableRowCount × TableGroupCount
//
// globalRow 与表格坐标的映射：
//   globalRow = group * TableRowCount + tableRow
//   group = globalRow / TableRowCount
//   tableRow = globalRow % TableRowCount
// =============================================================================
#pragma once

#include "ui/style_constants.h"

#include <QVector>
#include <QWidget>

#include <cstdint>

class QPushButton;
class QTableWidget;

/// @brief 寄存器表格控件
///
/// 封装 QTableWidget，提供寄存器地址/值编辑和读写操作的 UI。
/// 表格分为 4 组（Style::Size::TableGroupCount），每组之间有分隔线。
/// 行数固定 Style::Size::TableRowCount。
class RegisterTable : public QWidget {
    Q_OBJECT

public:
    /// @brief 数值显示模式
    enum class ValueMode {
        Dec,    ///< 有符号十进制
        Hex,    ///< 十六进制（0x####）
    };

    explicit RegisterTable(QWidget *parent = nullptr);
    ~RegisterTable() override;

    /// @brief 设置串口连接状态，联动 R/W 按钮的启用/禁用
    void setConnected(bool connected);

    /// @brief 切换值显示模式（Hex ↔ Dec），自动转换已有值
    void setValueMode(ValueMode mode);

    /// @brief 当前表格的全局行总数（TableRowCount × TableGroupCount）
    ///
    /// 外部模块（如 RegisterRwTab 的全部读 / 全部写循环）应通过此接口获取行数，
    /// 避免在多处硬编码 `Style::Size::TableRowCount × TableGroupCount`。
    int totalRows() const { return m_rowCount * MotorDev::Style::Size::TableGroupCount; }

    /// @brief 更新指定行的值显示（根据当前 ValueMode 格式化）
    /// @param globalRow 全局行索引（0 ~ totalRows()-1）
    /// @param value 寄存器值
    void updateRowValue(int globalRow, qint16 value);

    /// @brief 标记指定行为错误状态（显示 "--" 红色）
    void markRowError(int globalRow);

    /// @brief 标记指定行为等待状态（显示 "..." 灰色）
    void markRowPending(int globalRow);

    /// @brief 写入按钮视觉反馈（成功=绿色闪烁，失败=红色闪烁，200ms 后恢复）
    void markWriteButtonFeedback(int globalRow, bool success);

    /// @brief 检查指定行是否有有效地址
    bool rowHasAddress(int globalRow) const;

    /// @brief 检查指定行是否有有效值
    bool rowHasValue(int globalRow) const;

    /// @brief 获取指定行的地址（解析十六进制）
    quint16 rowAddress(int globalRow) const;

    /// @brief 获取指定行的值（解析十六进制或十进制）
    qint16 rowValue(int globalRow) const;

    /// @brief 保存所有行的描述/地址/值到 JSON 文件
    void saveConfig(const QString &path) const;

    /// @brief 从 JSON 文件加载描述/地址/值到表格
    ///
    /// 兼容旧版本不同 TableRowCount 的 JSON：按 jsonSize / TableGroupCount
    /// 推断旧文件 rowsPerGroup，加载到表格前 min(rowsPerGroup, TableRowCount) 行。
    void loadConfig(const QString &path);

    /// @brief 清空所有行的描述/地址/值（含错误态复位），并触发一次 configChanged 持久化
    void clearAll();

signals:
    /// @brief 请求读取指定行
    /// @param globalRow 全局行索引
    /// @param addr 寄存器地址
    void readRowRequested(int globalRow, quint16 addr);

    /// @brief 请求写入指定行
    /// @param globalRow 全局行索引
    /// @param addr 寄存器地址
    /// @param value 要写入的值
    void writeRowRequested(int globalRow, quint16 addr, qint16 value);

    /// @brief 表格内容变化（地址/值/描述修改时触发，用于自动保存）
    void configChanged();

private:
    /// @brief 构建 UI（表格框架 + 列头 + TableRowCount 行）
    void setupUi();

    /// @brief 连接 itemChanged 信号（R/W 按钮信号在 appendOneRow 内单行连接）
    void connectSignals();

    /// @brief 应用各列宽度设置（描述/地址/值 Stretch；R/W 固定）
    void applyColumnWidths();

    /// @brief 追加一行（创建 4 组 × 5 列的 items + R/W 按钮 + 信号连接），m_rowCount++
    void appendOneRow();

    // --- 列索引计算辅助 ---
    static int descCol(int group) { return group * 5 + 0; }   ///< 描述列
    static int addrCol(int group) { return group * 5 + 1; }   ///< 地址列
    static int valueCol(int group) { return group * 5 + 2; }  ///< 值列
    static int readCol(int group) { return group * 5 + 3; }   ///< 读按钮列
    static int writeCol(int group) { return group * 5 + 4; }  ///< 写按钮列

    /// @brief 按钮数组下标 (tableRow × TableGroupCount + group)
    ///
    /// 按钮数组采用 row-major 布局，使 appendOneRow 只需在数组尾部连续 push_back
    /// 4 个按钮（每组 1 个）。注意 globalRow 仍按外部约定 = group × m_rowCount + tableRow。
    static int buttonIndex(int group, int tableRow) {
        return tableRow * MotorDev::Style::Size::TableGroupCount + group;
    }

    QTableWidget *m_tableWidget = nullptr;          ///< 底层 QTableWidget
    int m_rowCount = 0;                             ///< 表格行数（构造时由 appendOneRow 累加至 TableRowCount）
    QVector<QPushButton*> m_readButtons;            ///< 读按钮（大小 = TableRowCount × TableGroupCount）
    QVector<QPushButton*> m_writeButtons;           ///< 写按钮（同上）
    ValueMode m_valueMode = ValueMode::Hex;         ///< 当前值显示模式
    bool m_connected = true;                        ///< 当前串口连接状态（用于新建行的按钮启用态）
};
