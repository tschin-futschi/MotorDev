// =============================================================================
// @file    registerrwtab.h
// @brief   寄存器读写页面 — 单行/全量读写、Hex/Dec 切换、批量操作
//
// RegisterRwTab 是应用主界面的"读写"标签页，提供以下功能：
// - 寄存器表格：支持逐行读/写、地址和值的编辑
// - 全部读取/写入：批量操作所有有效行
// - Hex/Dec 切换：切换数值显示模式
// - 批量读写区域：支持 4 组文件批量读写（功能开发中）
//
// 布局结构：
// ┌──────────┬────────────────────────────────────────────┐
// │ Sidebar  │ ┌────────────────────────────────────────┐ │
// │ 全部读取 │ │ RegisterTable（寄存器表格）             │ │
// │ 全部写入 │ │                                        │ │
// │ DEC/HEX  │ ├────────────────────────────────────────┤ │
// │          │ │ 批量读写区域（4行 + 2个占位卡片）      │ │
// └──────────┴────────────────────────────────────────────┘
//
// 寄存器表格配置持久化到 AppData/registers.json。
// =============================================================================
#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QLineEdit;
class QSplitter;
class CommandDispatcher;
class RegisterService;
class RegisterTable;

/// @brief 寄存器读写页面 Tab
///
/// 通过 RegisterService 完成寄存器的单行/批量读写操作，
/// 通过 RegisterTable 提供可编辑的地址-值表格。
class RegisterRwTab : public QWidget {
    Q_OBJECT

public:
    /// @brief 构造寄存器读写页面
    /// @param dispatcher 命令分发器（传给 RegisterService）
    /// @param parent 父控件指针
    explicit RegisterRwTab(CommandDispatcher *dispatcher, QWidget *parent = nullptr);
    ~RegisterRwTab() override;

public slots:
    /// @brief 设置串口连接状态，联动启用/禁用操作按钮
    /// @param connected true=已连接，false=未连接
    void setConnected(bool connected);

private:
    /// @brief 构建 UI 布局
    void setupUi();

    /// @brief 连接信号槽（表格↔Service、按钮↔操作）
    void connectSignals();

    /// @brief 获取寄存器配置文件的持久化路径
    /// @return AppData 下的 registers.json 完整路径
    QString configFilePath() const;

    // --- 核心依赖 ---
    RegisterService *m_service = nullptr;       ///< 寄存器读写服务

    // --- 布局容器 ---
    QWidget *m_sidebarContent = nullptr;        ///< 侧边栏内容区
    QWidget *m_mainContent = nullptr;           ///< 主内容区容器
    QSplitter *m_mainSplitter = nullptr;        ///< 上下区域的垂直分割器

    // --- 主要控件 ---
    RegisterTable *m_registerTable = nullptr;   ///< 寄存器表格控件
    QPushButton *m_readAllButton = nullptr;     ///< "全部读取"按钮
    QPushButton *m_writeAllButton = nullptr;    ///< "全部写入"按钮
    QPushButton *m_decButton = nullptr;         ///< DEC 模式切换按钮（互斥）
    QPushButton *m_hexButton = nullptr;         ///< HEX 模式切换按钮（互斥）

    // --- 批量操作控件（功能开发中）---
    QPushButton *m_batchBtn[4] = {};            ///< 4 组批量操作按钮（前 2 写，后 2 读）
    QLineEdit *m_batchDescEdit[4] = {};         ///< 4 组批量操作描述输入框
    QLineEdit *m_batchPathEdit[4] = {};         ///< 4 组批量操作文件路径（只读）
    QPushButton *m_batchBrowseBtn[4] = {};      ///< 4 组文件浏览按钮
};
