// =============================================================================
// @file    appconfigservice.h
// @brief   统一配置文件存取服务（采集 / 回填各页面功能参数）
//
// 把各页面的功能参数采集成一个统一 JSON 配置文件（手动 Write），或从 JSON 读回
// 回填到各页面（手动 Read）。写入仍**全手动**：参数变化不自动保存，路径由用户在配置
// 页 ConfigFile 指定。
//
// 路径记忆 + 启动自动回填（持久化逻辑在 MainWindow，本服务不读写 QSettings）：
// MainWindow 监听本服务 finished(ok,path)，Write/Read 成功时把路径记入 QSettings
// (config/lastFile)；下次启动时把该路径填回配置页输入框并触发一次 onReadRequested 自动
// 回填各页面。文件不存在 / 损坏时记日志并优雅跳过，不阻断启动。
//
// Read 语义：**只回填参数值，不触发任何串口动作**（不连接 / 不下发 / 不写寄存器 /
// 不启动采样 / 不烧录）。各页面的 applyXxx 负责具体回填。启动自动回填同样零串口副作用。
//
// JSON 结构（version=1）：
//   { "version":1,
//     "device":     {...},   // ConfigTab：串口/波特率/IC/从机/PMIC
//     "registers":  [...],   // RegisterRwTab：120 行 desc/addr/val
//     "scope":      {...},   // OscilloscopTab：8 通道 + 间隔 + 记录目录
//     "flash":      {...},   // FwFlashTab：上次烧录文件路径
//     "flashStore": {...} }  // FlashStorageTab：上次目录
//
// 由 MainWindow 持有；接收 ConfigTab 的 readConfigRequested / writeConfigRequested。
// =============================================================================

#pragma once

#include <QObject>
#include <QString>

class ConfigTab;
class RegisterRwTab;
class OscilloscopTab;
class FwFlashTab;
class FlashStorageTab;

/// @brief 统一配置文件存取服务
class AppConfigService : public QObject {
    Q_OBJECT

public:
    AppConfigService(ConfigTab *configTab,
                     RegisterRwTab *registerTab,
                     OscilloscopTab *scopeTab,
                     FwFlashTab *fwFlashTab,
                     FlashStorageTab *flashStorageTab,
                     QObject *parent = nullptr);

    /// @brief 采集各页面功能参数写入 JSON 文件
    /// @return 成功与否；失败时 errorMessage 含原因
    bool write(const QString &path, QString *errorMessage = nullptr);

    /// @brief 从 JSON 文件读取并回填各页面（缺段跳过 + 日志，不整体失败）
    bool read(const QString &path, QString *errorMessage = nullptr);

public slots:
    /// @brief 处理「Write」请求（来自 ConfigTab），写文件并发 finished / logMessage
    void onWriteRequested(const QString &path);

    /// @brief 处理「Read」请求（来自 ConfigTab），读文件回填并发 finished / logMessage
    void onReadRequested(const QString &path);

signals:
    /// @brief 过程日志（由 MainWindow 路由到底部 LogPanel）
    void logMessage(const QString &line);

    /// @brief 一次存/取结束
    void finished(bool ok, const QString &message);

private:
    static constexpr int kConfigVersion = 1;  ///< 配置文件结构版本

    ConfigTab       *m_configTab = nullptr;
    RegisterRwTab   *m_registerTab = nullptr;
    OscilloscopTab  *m_scopeTab = nullptr;
    FwFlashTab      *m_fwFlashTab = nullptr;
    FlashStorageTab *m_flashStorageTab = nullptr;
};
