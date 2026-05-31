// =============================================================================
// @file    aboutdialog.h
// @brief   关于对话框 — 展示软件名称、版本、构建、支持 IC、协议、版权与作者
//
// 由侧边栏「关于」按钮触发（ActivityBar::aboutRequested → MainWindow 弹出）。
// 模态对话框，每次打开时按当前语言构建（无需运行时 retranslate）。
// 软件元信息（版本/作者/支持 IC/协议）集中定义在 .cpp 顶部具名常量，便于维护。
// =============================================================================
#pragma once

#include <QDialog>

class QSvgRenderer;
class QPaintEvent;

/// @brief 软件「关于」对话框（模态）
///
/// 背景为软件图标（motordev_logo.svg）全幅展开（拉伸填满整个对话框），
/// 其上叠加半透明遮罩以保证前景文字可读。
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog() override;

protected:
    /// @brief 绘制全幅图标背景 + 半透明遮罩（前景控件由各自 widget 叠加其上）
    void paintEvent(QPaintEvent *event) override;

private:
    /// @brief 构建对话框 UI（标题/简介 + 信息表 + 版权/仓库/作者 + 确定按钮）
    void setupUi();

    QSvgRenderer *m_bgRenderer = nullptr;   ///< 背景图标（SVG）渲染器
};
