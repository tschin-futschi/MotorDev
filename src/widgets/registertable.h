// =============================================================================
// @file    registertable.h
// @brief   寄存器表格控件 — 多组寄存器的地址/值编辑、单行读写、状态反馈
//
// RegisterTable 提供一个 20行 × 20列 的 QTableWidget，分为 4 组（每组 5 列）：
// 每组包含：描述 | 地址 | 值 | R(读按钮) | W(写按钮)
// 共 4 组 × 20 行 = 80 个寄存器行（globalRow 0~79）。
//
// 功能：
// - 单行读写：点击 R/W 按钮发出 readRowRequested/writeRowRequested 信号
// - 值显示模式切换：Hex（0x####）/ Dec（有符号十进制）
// - 行状态反馈：pending（等待中 "..."）、error（失败 "--"）、写入反馈闪烁
// - 配置持久化：保存/加载到 JSON 文件
//
// globalRow 与表格坐标的映射：
//   globalRow = group * TableRowCount + tableRow
//   group = globalRow / TableRowCount
//   tableRow = globalRow % TableRowCount
// =============================================================================
#pragma once

#include <QWidget>

#include <cstdint>

class QPushButton;
class QTableWidget;

/// @brief 寄存器表格控件
///
/// 封装 QTableWidget，提供寄存器地址/值编辑和读写操作的 UI。
/// 表格分为 4 组（Style::Size::TableGroupCount），每组之间有分隔线。
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

    /// @brief 更新指定行的值显示（根据当前 ValueMode 格式化）
    /// @param globalRow 全局行索引（0~79）
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
    void loadConfig(const QString &path);

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
    /// @brief 构建 UI（表格 + 列头 + 单元格 + R/W 按钮）
    void setupUi();

    /// @brief 连接 R/W 按钮和 itemChanged 信号
    void connectSignals();

    /// @brief 应用各列宽度设置
    void applyColumnWidths();

    // --- 列索引计算辅助 ---
    static int descCol(int group) { return group * 5 + 0; }   ///< 描述列
    static int addrCol(int group) { return group * 5 + 1; }   ///< 地址列
    static int valueCol(int group) { return group * 5 + 2; }  ///< 值列
    static int readCol(int group) { return group * 5 + 3; }   ///< 读按钮列
    static int writeCol(int group) { return group * 5 + 4; }  ///< 写按钮列

    QTableWidget *m_tableWidget = nullptr;      ///< 底层 QTableWidget
    QPushButton *m_readButtons[80] = {};        ///< 80 个读按钮
    QPushButton *m_writeButtons[80] = {};       ///< 80 个写按钮
    ValueMode m_valueMode = ValueMode::Hex;     ///< 当前值显示模式
};
