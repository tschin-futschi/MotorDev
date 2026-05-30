// =============================================================================
// @file    scoperegisterpanel.h
// @brief   示波器寄存器辅助面板 — 8 行寄存器读写 + 循环写入控制
//
// ScopeRegisterPanel 嵌入在示波器底部面板中，提供：
// - 8 行寄存器操作：描述 | 地址 | 值 | R按钮 | W按钮
// - 数值栏 HEX/DEC 输入与显示切换（输入进制由 0x 前缀决定，非法标红）
// - 循环写入：间隔设置 + 启动/停止
// - 清空面板（含描述）和 HEX/DEC 切换按钮
// =============================================================================
#pragma once

#include <QString>
#include <QWidget>

class QLineEdit;
class QPushButton;

/// @brief 示波器寄存器辅助面板
///
/// 提供 8 行寄存器的快捷读写操作，以及循环写入功能。
/// 信号按行索引（0~7）发出，由 OscilloscopTab 转发给 RegisterService。
class ScopeRegisterPanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeRegisterPanel(QWidget *parent = nullptr);
    ~ScopeRegisterPanel() override;

    /// @brief 获取指定行的地址文本
    QString addressText(int row) const;

    /// @brief 获取指定行的值文本
    QString valueText(int row) const;

    /// @brief 获取循环写入间隔文本
    QString intervalText() const;

    /// @brief 设置指定行的值文本（原样写入，不做格式化）
    void setValueText(int row, const QString &text);

    /// @brief 按当前显示进制（HEX/DEC）格式化并写入指定行的值（用于读回填）
    void setValueNumeric(int row, qint16 value);

    /// @brief 设置指定行的地址输入框错误样式
    void setAddressError(int row, bool error);

    /// @brief 设置指定行的值输入框错误样式（数值非法时红框）
    void setValueError(int row, bool error);

    /// @brief 当前数值栏是否为 HEX 显示模式（false=DEC）
    bool isHexMode() const { return m_hexMode; }

    /// @brief 设置 R/W 按钮反馈状态（"ok"/"error"/"pending"）
    /// @param row 行索引
    /// @param isRead true=读按钮，false=写按钮
    /// @param state 状态字符串
    void setButtonFeedback(int row, bool isRead, const QString &state);

    /// @brief 设置循环写入运行状态（切换启动/停止按钮可用性）
    void setCyclicRunning(bool running);

    /// @brief 清空所有行的内容
    void clearAll();

    /// @brief 面板行数（固定 8 行）
    static constexpr int rowCount() { return RowCount; }

signals:
    void readRequested(int row);        ///< 单行读取请求
    void writeRequested(int row);       ///< 单行写入请求
    void startRequested();              ///< 启动循环写入
    void stopRequested();               ///< 停止循环写入
    void clearPanelRequested();         ///< 清空面板

private:
    static constexpr int RowCount = 8;  ///< 面板行数

    void setupUi();
    void connectSignals();

    /// @brief 校验指定行数值文本，非法（非空且解析失败）则标红
    void validateValueRow(int row);

    /// @brief 切换数值栏 HEX/DEC 显示模式并重排所有行
    void toggleValueFormat();

    /// @brief 按当前模式重新格式化所有合法数值，非法行保留原文并标红
    void reformatAllValues();

    /// @brief 刷新切换按钮文案为当前模式（HEX/DEC）
    void refreshFormatButtonText();

    QLineEdit *m_descEdits[RowCount] = {};      ///< 8 行描述输入框
    QLineEdit *m_addrEdits[RowCount] = {};      ///< 8 行地址输入框
    QLineEdit *m_valueEdits[RowCount] = {};     ///< 8 行值输入框
    QPushButton *m_readButtons[RowCount] = {};  ///< 8 个读按钮
    QPushButton *m_writeButtons[RowCount] = {}; ///< 8 个写按钮
    QLineEdit *m_intervalEdit = nullptr;        ///< 循环写入间隔输入框
    QPushButton *m_startButton = nullptr;       ///< 启动循环写入按钮
    QPushButton *m_stopButton = nullptr;        ///< 停止循环写入按钮
    QPushButton *m_clearButton = nullptr;       ///< 清空按钮
    QPushButton *m_formatToggleButton = nullptr; ///< HEX/DEC 显示切换按钮
    bool m_hexMode = true;                      ///< 数值栏显示模式（true=HEX，默认）
};
