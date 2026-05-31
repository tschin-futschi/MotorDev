// =============================================================================
// @file    fwfileinfopanel.h
// @brief   固件信息视图 — 内嵌于"固件文件"卡片下半部分的字段展示
//
// v2 重构：原本作为独立 GroupBox 出现的卡片已合并到固件文件卡片，
// 因此本类降级为 QWidget 子类（无外框）。
//
// 三种状态由内部 QStackedWidget 切换：
//   - 空：单行灰色提示"请先选择固件文件"
//   - 合法：标签-值网格（.bin 4 行；.hex 7 行）
//   - 错误：单行红色 `解析失败：<原因>`
// =============================================================================
#pragma once

#include <QWidget>

class QEvent;
class QLabel;
class QStackedWidget;
struct FirmwareInfo;

class FwFileInfoPanel : public QWidget {
    Q_OBJECT

public:
    explicit FwFileInfoPanel(QWidget *parent = nullptr);
    ~FwFileInfoPanel() override;

    /// @brief 显示一份合法 / 非法的解析结果
    void setInfo(const FirmwareInfo &info);

    /// @brief 清空显示，回到空提示
    void clear();

protected:
    /// @brief 语言切换（QEvent::LanguageChange）时刷新固定字段标签
    void changeEvent(QEvent *event) override;

private:
    void setupUi();

    /// @brief 重设空提示与各固定字段标签文字（值文本为动态数据，不在此处理）
    void retranslateUi();

    QStackedWidget *m_stack = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_errorLabel = nullptr;

    // 固定字段标签（提为成员以支持语言即时切换）
    QLabel *m_fileNameLabel = nullptr;
    QLabel *m_fileSizeLabel = nullptr;
    QLabel *m_formatLabel = nullptr;
    QLabel *m_crc32Label = nullptr;

    QLabel *m_fileNameValue = nullptr;
    QLabel *m_fileSizeValue = nullptr;
    QLabel *m_formatValue = nullptr;
    QLabel *m_crc32Value = nullptr;

    QLabel *m_segCountLabel = nullptr;
    QLabel *m_segCountValue = nullptr;
    QLabel *m_addrRangeLabel = nullptr;
    QLabel *m_addrRangeValue = nullptr;
    QLabel *m_effectiveLabel = nullptr;
    QLabel *m_effectiveValue = nullptr;

    // Hl9788Hex 补齐路径专属：原始行数 + 填充行数 + footer CRC32
    QLabel *m_paddingLabel = nullptr;
    QLabel *m_paddingValue = nullptr;
};
