// =============================================================================
// @file    fwflashtab.h
// @brief   固件烧录页面 — IC 选择、文件选择、文件信息、烧录控制、操作日志
//
// 五区主内容布局（左侧 Sidebar 仅放说明文字）：
//   1. IC 型号选择栏
//   2. 固件文件选择栏（.bin / .hex）
//   3. FwFileInfoPanel
//   4. FwFlashControlPanel
//   5. FwFlashLogPanel
//
// 业务逻辑全部下沉到 FwFlashService；本类仅负责 UI 状态与信号转发。
// 烧录前置序列（停采样/停发生器/停循环写入）通过外部注入的 3 个
// std::function 回调由 FwFlashService 触发；MainWindow 在持有 ScopeService /
// GeneratorService / CyclicWriteService 的对象上 findChild 取实例并 wrap 为
// 回调注入本 Tab。回调未设置时 Service 仅写日志，跳过该步骤。
// PMIC 不参与前置序列：烧录期间 IC 必须保持正常供电。
// =============================================================================
#pragma once

#include "services/fwflashservice.h"

#include <QWidget>

#include <functional>
#include <memory>

class CommandDispatcher;
class FlashStrategyRegistry;
class FwFileInfoPanel;
class FwFlashControlPanel;
class FwFlashLogPanel;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSplitter;
struct FirmwareInfo;

class FwFlashTab : public QWidget {
    Q_OBJECT

public:
    explicit FwFlashTab(CommandDispatcher *dispatcher, QWidget *parent = nullptr);
    ~FwFlashTab() override;

    /// @brief 注入"立即停止"回调，由 MainWindow 绑定到对应 Service。
    /// 未设置时 FwFlashService 跳过该步骤（仅写日志，不阻断烧录）。
    void setStopScopeCallback(std::function<void()> cb);
    void setStopGeneratorCallback(std::function<void()> cb);
    void setStopCyclicWriteCallback(std::function<void()> cb);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onBrowseClicked();
    void onClearFileClicked();
    void onIcChanged(int index);
    void onStartRequested();
    void onCancelRequested();

    void onServiceStateChanged(FwFlashService::State newState);
    void onServiceStage(const QString &message);
    void onServiceProgress(qint64 sent, qint64 total);
    void onServiceLog(FwFlashService::LogLevel level, const QString &message);
    void onServiceFinished(bool success, const QString &summary);

private:
    void setupUi();
    void connectSignals();
    void rebuildIcCombo();
    void parseAndShowFile(const QString &path);
    void clearFileState();
    void updateStartEnabled();

    // --- 核心依赖 ---
    std::unique_ptr<FlashStrategyRegistry> m_registry;
    FwFlashService *m_service = nullptr;

    // --- 当前选中文件解析结果 ---
    QString m_currentFilePath;
    bool m_currentFileValid = false;
    QByteArray m_currentFirmwareData;
    qint64 m_currentFirmwareTotal = 0;

    // --- 布局容器 ---
    QWidget *m_mainContent = nullptr;
    QWidget *m_leftPane = nullptr;
    QWidget *m_rightPane = nullptr;
    QSplitter *m_splitter = nullptr;
    bool m_splitterInitialSized = false;  ///< 仅第一次 show 时设初始 1:1，之后由用户拖动决定

    // --- 1. IC 选择栏 ---
    QComboBox *m_icCombo = nullptr;
    QLabel *m_icDescLabel = nullptr;

    // --- 2. 文件选择栏 ---
    QLineEdit *m_pathEdit = nullptr;
    QPushButton *m_browseBtn = nullptr;
    QPushButton *m_clearFileBtn = nullptr;

    // --- 3/4/5 面板 ---
    FwFileInfoPanel *m_infoPanel = nullptr;
    FwFlashControlPanel *m_controlPanel = nullptr;
    FwFlashLogPanel *m_logPanel = nullptr;
};
