// =============================================================================
// @file    serialdebugtab.h
// @brief   串口调试模拟器页面 — 模拟 STM32 侧行为，用于无硬件环境下的协议调试
//
// SerialDebugTab 是一个独立窗口（Qt::Window），模拟 STM32 端的串口通信行为：
// - 连接到真实串口（通常通过虚拟串口对与主程序配对）
// - 接收上位机命令并生成模拟响应
// - 可配置模拟参数：I2C 扫描地址、IC 连接结果、寄存器读返回值、写入结果、响应延迟
// - 活动日志实时展示收发帧的详细内容
//
// 通过 debugStreamGenerated / debugStreamActiveChanged 信号将模拟波形数据
// 转发给 OscilloscopTab（经 MainWindow 中继）。
// =============================================================================
#pragma once

#include <QVector>
#include <QWidget>

#include <cstdint>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QSplitter;
class QTextEdit;
class SimulatorService;

/// @brief 串口调试模拟器页面（独立窗口）
///
/// 模拟 STM32 侧的协议行为，通过真实串口与上位机主程序通信。
/// 用户可配置模拟参数，实时查看收发帧日志。
class SerialDebugTab : public QWidget {
    Q_OBJECT

public:
    /// @brief 构造串口调试模拟器窗口
    /// @param parent 父控件指针
    explicit SerialDebugTab(QWidget *parent = nullptr);
    ~SerialDebugTab() override;

signals:
    /// @brief 模拟器生成的调试流数据（转发给示波器）
    /// @param channelMask 通道掩码（bit0~bit7 对应 CH1~CH8）
    /// @param samples 采样数据（各通道交错排列）
    void debugStreamGenerated(uint8_t channelMask, const QVector<int16_t> &samples);

    /// @brief 模拟器调试流启停状态变化
    /// @param active true=采样中，false=已停止
    void debugStreamActiveChanged(bool active);

private:
    /// @brief 构建 UI 布局（连接栏 + 左侧配置 + 右侧日志）
    void setupUi();

    /// @brief 连接信号槽（UI↔Service、Service→外部信号）
    void connectSignals();

    /// @brief 刷新可用串口列表
    void refreshPortList();

    /// @brief 切换连接/未连接状态的控件外观
    /// @param connected true=已连接
    void setConnectedState(bool connected);

    /// @brief 追加系统日志（蓝色/红色文本）
    /// @param message 日志内容
    /// @param isError 是否为错误信息
    void appendSysLog(const QString &message, bool isError = false);

    /// @brief 追加帧日志（格式化显示 cmd/seq/data）
    /// @param dir 方向标记（"RX" 或 "TX"）
    /// @param cmd 命令字节
    /// @param seq 序列号
    /// @param data 载荷数据
    /// @param note 附加说明
    void appendLog(const QString &dir, uint8_t cmd, uint8_t seq, const QByteArray &data, const QString &note = {});

    // --- 核心依赖 ---
    SimulatorService *m_service = nullptr;      ///< 模拟器服务（命令分发 + 响应生成）

    // --- 连接栏控件 ---
    QComboBox *m_portCombo = nullptr;           ///< 串口选择
    QComboBox *m_baudCombo = nullptr;           ///< 波特率选择
    QPushButton *m_scanButton = nullptr;        ///< 刷新串口按钮
    QPushButton *m_connectButton = nullptr;     ///< 连接/断开按钮
    QLabel *m_statusBadge = nullptr;            ///< 连接状态标签（● 已连接/未连接）

    // --- 左侧应答配置控件 ---
    QLineEdit *m_scanAddrEdit = nullptr;        ///< I2C 扫描地址（逗号分隔，如 "0x5A"）
    QComboBox *m_icAddrResultCombo = nullptr;   ///< IC 连接结果（成功/失败）
    QLineEdit *m_regReadValueEdit = nullptr;    ///< 寄存器读返回值（如 "0x0000"）
    QComboBox *m_writeResultCombo = nullptr;    ///< 寄存器写结果（ACK/失败）
    QSpinBox *m_delaySpinBox = nullptr;         ///< 响应延迟（0~500ms）

    // --- 右侧日志区控件 ---
    QSplitter *m_mainSplitter = nullptr;        ///< 左右区域水平分割器
    QTextEdit *m_logEdit = nullptr;             ///< 活动日志（HTML 富文本）
    QPushButton *m_clearLogButton = nullptr;    ///< 清除日志按钮
};
