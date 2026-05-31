// =============================================================================
// @file    registerrwtab.h
// @brief   寄存器读写页面 — 单行/全量读写、Hex/Dec 切换、批量操作、块读取
//
// RegisterRwTab 是应用主界面的"读写"标签页，提供以下功能：
// - 寄存器表格：支持逐行读/写、地址和值的编辑
// - 全部读取/写入：批量操作所有有效行
// - Hex/Dec 切换：切换数值显示模式
// - 批量读写浮窗：4 个独立操作槽（前 2 写、后 2 读），业务由 BatchRegisterService 承载
// - 块读取浮窗：起始地址 + 个数 → 连续 dump 到 CSV，业务由 BlockReadService 承载
//
// 布局结构：
// ┌──────────┬────────────────────────────────────────────┐
// │ Sidebar  │ ┌────────────────────────────────────────┐ │
// │ 全部读取 │ │ RegisterTable（寄存器表格） stretch=1   │ │
// │ 全部写入 │ │                                        │ │
// │ DEC/HEX  │ ├──────── [批量读写] [块读取] ──────────┤ │  常驻工具条（右对齐 toggle）
// └──────────┴────────────────────────────────────────────┘
//
// 两个浮窗均以独立 Qt::Tool 顶级窗口弹出，由各自的 toggle 按钮控制显隐；
// 用户关闭浮窗时按钮状态自动同步。块读取与批量读写**独立通道**，不互斥
// （独立 RegisterService 实例隔离命令流，串口底层 SerialManager 命令队列共享）。
//
// 寄存器表格内容不再自动持久化；统一由配置页「配置文件」Read/Write 手动存取
// （AppConfigService 调 collectRegisterRows / applyRegisterRows）。
// =============================================================================
#pragma once

#include "services/batchregisterservice.h"
#include "services/blockreadservice.h"

#include <QString>
#include <QWidget>

class QLabel;
class QJsonArray;
class QProgressBar;
class QPushButton;
class QToolButton;
class QLineEdit;
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

    /// @brief 采集寄存器表 120 行（desc/addr/val）为 JSON 数组，供统一配置文件
    QJsonArray collectRegisterRows() const;

    /// @brief 从 JSON 数组回填寄存器表
    void applyRegisterRows(const QJsonArray &rows);

public slots:
    /// @brief 设置串口连接状态，联动启用/禁用操作按钮
    /// @param connected true=已连接，false=未连接
    void setConnected(bool connected);

protected:
    /// @brief 监听浮动批量读写 / 块读取浮窗的 Close 事件，同步对应 toggle 按钮状态。
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// @brief 谁占用了全局互斥位（4 个批量槽 + Sidebar 全部读写 之间互斥）
    ///
    /// 注：块读取走「独立通道」（决策 D7），不参与本枚举，亦不变灰互斥按钮。
    enum class BusyOwner {
        None,
        BatchSlot1,
        BatchSlot2,
        BatchSlot3,
        BatchSlot4,
        SidebarAll,     ///< Sidebar 全部读取 / 全部写入
    };

    /// @brief 构建 UI 布局
    void setupUi();

    /// @brief 连接信号槽（表格↔Service、按钮↔操作）
    void connectSignals();

    // --- 批量浮窗 UI 行为（仅控件交互，业务在 BatchRegisterService）---
    void onBatchBrowseClicked(int slotIndex);
    void onBatchActionClicked(int slotIndex);

    // --- BatchRegisterService 信号回调（UI 渲染层）---
    void onBatchServiceStageMessage(const QString &message);
    void onBatchServiceLog(BatchRegisterService::LogLevel level, const QString &message);
    void onBatchServiceFinished(bool success, const QString &summary);

    // --- 块读取浮窗 UI 行为（仅控件交互，业务在 BlockReadService）---
    void onBlockReadBrowseDirClicked();
    void onBlockReadStartClicked();
    void onBlockReadCancelClicked();

    // --- BlockReadService 信号回调（UI 渲染层）---
    void onBlockReadServiceStateChanged(BlockReadService::State state);
    void onBlockReadServiceProgress(int done, int total);
    void onBlockReadServiceStageMessage(const QString &message);
    void onBlockReadServiceLog(BlockReadService::LogLevel level, const QString &message);
    void onBlockReadServiceFinished(bool success, const QString &summary, const QString &savedPath);

    // --- 互斥锁 ---
    void setBusyOwner(BusyOwner owner);
    void updateBusyUi();
    bool isBusy() const { return m_busyOwner != BusyOwner::None; }
    static BusyOwner slotIndexToOwner(int slotIndex);

    // --- 核心依赖 ---
    RegisterService *m_service = nullptr;             ///< 寄存器读写服务（RegisterTable + Sidebar 全部读/写）
    BatchRegisterService *m_batchService = nullptr;   ///< 批量读写业务服务（封装文件解析 + 状态机 + 逐条收发）
    BlockReadService *m_blockReadService = nullptr;   ///< 块读取业务服务（连续地址段 → CSV 文件，独立通道）

    // --- 布局容器 ---
    QWidget *m_sidebarContent = nullptr;        ///< 侧边栏内容区
    QWidget *m_mainContent = nullptr;           ///< 主内容区容器

    // --- 主要控件 ---
    RegisterTable *m_registerTable = nullptr;   ///< 寄存器表格控件
    QPushButton *m_readAllButton = nullptr;     ///< "全部读取"按钮
    QPushButton *m_writeAllButton = nullptr;    ///< "全部写入"按钮
    QPushButton *m_decButton = nullptr;         ///< DEC 模式切换按钮（互斥）
    QPushButton *m_hexButton = nullptr;         ///< HEX 模式切换按钮（互斥）

    // --- 工具条操作按钮 ---
    QToolButton *m_clearPageBtn = nullptr;      ///< 页面清除（清空表格所有描述/地址/值）

    // --- 批量读写浮动窗口 ---
    QToolButton *m_batchToggleBtn = nullptr;    ///< 常驻工具条 toggle（checkable，默认未弹出）
    QWidget *m_batchPanel = nullptr;            ///< 批量读写浮窗（Qt::Tool 顶级窗口，parent=nullptr，析构手动 delete）
    QLabel *m_batchStatusLabel = nullptr;       ///< 浮窗底部状态文字（进度 / 完成 / 错误）

    // --- 批量操作控件 ---
    // 每行 3 列：[批量写入/批量读出 按钮] [文件路径 只读] [浏览 按钮]
    QPushButton *m_batchBtn[4] = {};            ///< 4 组批量操作按钮（前 2 写，后 2 读）
    QLineEdit *m_batchPathEdit[4] = {};         ///< 4 组批量操作文件路径（只读）
    QPushButton *m_batchBrowseBtn[4] = {};      ///< 4 组文件浏览按钮

    // --- 块读取浮动窗口 ---
    QToolButton *m_blockReadToggleBtn = nullptr;    ///< 常驻工具条 toggle（checkable）
    QWidget *m_blockReadPanel = nullptr;            ///< 块读取浮窗（Qt::Tool 顶级窗口，parent=nullptr，析构手动 delete）
    QLineEdit *m_blockReadStartEdit = nullptr;      ///< 起始地址输入（hex，接受 0xXXXX / XXXX）
    QLineEdit *m_blockReadCountEdit = nullptr;      ///< 寄存器个数输入（十进制 ≥ 1）
    QLineEdit *m_blockReadDirEdit = nullptr;        ///< 保存目录显示（只读，由浏览按钮设置）
    QPushButton *m_blockReadBrowseBtn = nullptr;    ///< 浏览目录按钮
    QPushButton *m_blockReadStartBtn = nullptr;     ///< 开始读取按钮
    QPushButton *m_blockReadCancelBtn = nullptr;    ///< 取消按钮（运行时启用，空闲时禁用）
    QProgressBar *m_blockReadProgressBar = nullptr; ///< 进度条（0~100）
    QLabel *m_blockReadStatusLabel = nullptr;       ///< 状态文字标签

    // --- 互斥锁 + 浏览状态 ---
    BusyOwner m_busyOwner = BusyOwner::None;    ///< 当前互斥位归属（不含块读取）
    QString m_batchLastBrowseDir;               ///< 批量读写浏览对话框上次使用的目录（会话内）
    QString m_blockReadLastDir;                 ///< 块读取保存目录上次使用值（会话内，仿 m_batchLastBrowseDir）
};
