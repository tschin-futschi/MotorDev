// =============================================================================
// @file    scopechannelstrip.h
// @brief   示波器通道条 — 单通道的启用开关、描述、地址配置
//
// ScopeChannelStrip 是示波器底部面板中的通道配置条，每个通道一个：
// [✓ CH1] [描述输入框] [地址输入框]
// 提供通道启用/禁用、描述和地址的编辑功能。
// =============================================================================
#pragma once

#include <QCheckBox>
#include <QLineEdit>
#include <QWidget>

/// @brief 示波器单通道配置条
///
/// 包含复选框（启用/禁用）、描述输入框和地址输入框。
/// 信号携带通道索引，供父控件统一处理。
class ScopeChannelStrip : public QWidget {
    Q_OBJECT

public:
    /// @brief 构造通道条
    /// @param index 通道索引（0~7 对应 CH1~CH8）
    /// @param parent 父控件
    explicit ScopeChannelStrip(int index, QWidget *parent = nullptr);
    ~ScopeChannelStrip() override;

    /// @brief 通道是否启用
    bool isChannelEnabled() const;

    /// @brief 回填通道配置（统一配置文件 Read 用）。**阻塞控件信号**，不向外发
    /// channelToggled/descriptionChanged/addressChanged（model 由调用方直接写）。
    void setChannelEnabled(bool enabled);
    void setDescriptionText(const QString &text);
    void setAddressText(const QString &text);

signals:
    /// @brief 通道启用/禁用切换
    void channelToggled(int index, bool enabled);

    /// @brief 通道描述文本变化
    void descriptionChanged(int index, const QString &text);

    /// @brief 通道地址文本变化
    void addressChanged(int index, const QString &text);

protected:
    /// @brief 语言切换（QEvent::LanguageChange）时刷新占位文字
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void connectSignals();

    /// @brief 重设所有用户可见文字（setupUi 末尾 + 语言切换时调用）
    void retranslateUi();

    QCheckBox *m_checkBox = nullptr;    ///< 通道启用复选框（显示 "CH#"）
    QLineEdit *m_descEdit = nullptr;    ///< 描述输入框
    QLineEdit *m_addrEdit = nullptr;    ///< 地址输入框（十六进制）
    int m_index = 0;                    ///< 通道索引（0~7）
};
