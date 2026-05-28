// =============================================================================
// @file    configtab.h
// @brief   配置页面 — 串口连接、IC 选择、PMIC 电压配置、设备控制
//
// ConfigTab 是应用主界面的"配置"标签页，提供以下功能区域：
// - IC 卡片：选择电机驱动 IC 型号、I2C 扫描、设置从设备地址
// - Serial 卡片：串口扫描/连接/断开、设备复位、电机测试
// - PMIC 卡片：DRVDD/VCMVDD/IOVDD 电压配置与 PMIC 使能/禁用
// - Config File 区域：配置文件读写（当前为占位，功能开发中）
//
// 布局结构（无 Sidebar，主区域占满）：
// ┌──────────────────────────────────────────────────┐
// │ ┌──────────┬──────────┬──────────┐               │
// │ │ IC 卡片  │ Serial   │ PMIC     │  upper area   │
// │ │          │ 卡片     │ 卡片     │               │
// │ ├──────────┴──────────┴──────────┤               │
// │ │ Config File 选择/读写           │  lower area   │
// │ │                                 │               │
// └──────────────────────────────────────────────────┘
//
// 所有业务操作通过 ConfigService 完成，UI 层仅负责控件状态管理和信号转发。
// =============================================================================
#pragma once

#include <QWidget>

#include <cstdint>

class ConfigService;
class CommandDispatcher;
class DeviceContext;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;
class SerialManager;
class QSplitter;

/// @brief 配置页面 Tab — 串口/IC/PMIC 配置与设备控制
///
/// 通过 ConfigService 与底层通信层交互，UI 层不直接操作串口或协议。
/// 串口连接/断开事件通过 serialConnected/serialDisconnected 信号对外广播，
/// 供 MainWindow 同步其他 Tab 的连接状态。
class ConfigTab : public QWidget {
    Q_OBJECT

public:
    /// @brief 构造配置页面
    /// @param serialManager 串口管理器（传给 ConfigService）
    /// @param dispatcher 命令分发器（传给 ConfigService）
    /// @param deviceContext 设备上下文（IC 型号 + 从设备地址）
    /// @param parent 父控件指针
    explicit ConfigTab(SerialManager *serialManager,
                       CommandDispatcher *dispatcher,
                       DeviceContext *deviceContext,
                       QWidget *parent = nullptr);
    ~ConfigTab() override;

signals:
    /// @brief 串口连接成功
    /// @param port 端口名称（如 "COM3"）
    /// @param baudRate 波特率
    void serialConnected(const QString &port, qint32 baudRate);

    /// @brief 串口已断开
    void serialDisconnected();

private:
    /// @brief 构建 UI 布局（三张卡片 + 下方配置文件区，无侧边栏）
    void setupUi();

    /// @brief 连接所有信号槽（UI↔Service、ComboBox↔DeviceContext）
    void connectSignals();

    /// @brief 刷新可用串口列表到 m_portCombo
    void refreshAvailablePorts();

    /// @brief 根据连接状态启用/禁用串口相关控件
    /// @param connected true=已连接，false=未连接
    void setSerialControlsConnected(bool connected);

    // --- 核心依赖 ---
    ConfigService *m_service = nullptr;         ///< 配置服务（封装串口/I2C/PMIC 操作）
    DeviceContext *m_deviceContext = nullptr;    ///< 设备上下文（IC 型号 + 从设备地址）

    // --- 布局容器 ---
    QWidget *m_mainContent = nullptr;           ///< 主内容区容器
    QSplitter *m_mainSplitter = nullptr;        ///< 上下区域的垂直分割器

    // --- 三张卡片 GroupBox ---
    QGroupBox *m_icGroup = nullptr;             ///< IC 配置卡片
    QGroupBox *m_serialGroup = nullptr;         ///< 串口配置卡片
    QGroupBox *m_pmicGroup = nullptr;           ///< PMIC 配置卡片

    // --- IC 卡片控件 ---
    QComboBox *m_icCombo = nullptr;             ///< IC 型号下拉框（AW86006/DW9786/DW9788）
    QComboBox *m_slaveIdCombo = nullptr;        ///< I2C 从设备地址下拉框
    QPushButton *m_icScanButton = nullptr;      ///< I2C 扫描按钮
    QPushButton *m_icConnectButton = nullptr;   ///< IC 连接按钮（设置从设备地址）

    // --- Serial 卡片控件 ---
    QComboBox *m_portCombo = nullptr;           ///< 串口选择下拉框
    QComboBox *m_baudRateCombo = nullptr;       ///< 波特率选择下拉框
    QPushButton *m_scanButton = nullptr;        ///< 刷新串口列表按钮
    QPushButton *m_connectButton = nullptr;     ///< 连接/断开按钮

    // --- PMIC 卡片控件 ---
    QDoubleSpinBox *m_drvddSpin = nullptr;      ///< DRVDD 电压输入（0.60~3.77V）
    QDoubleSpinBox *m_vcmvddSpin = nullptr;     ///< VCMVDD 电压输入（0.60~3.77V）
    QDoubleSpinBox *m_iovddSpin = nullptr;      ///< IOVDD 电压输入（0.60~3.77V）
    QPushButton *m_pmicConfigButton = nullptr;   ///< PMIC 配置按钮
    QPushButton *m_pmicDisableButton = nullptr;  ///< PMIC 禁用按钮

    // --- 设备控制按钮 ---
    QPushButton *m_resetButton = nullptr;       ///< 设备复位按钮
    QPushButton *m_motorTestButton = nullptr;   ///< 电机测试按钮

    // --- Config File 区域控件（功能开发中）---
    QComboBox *m_fileCombo = nullptr;           ///< 配置文件选择框
    QPushButton *m_browseButton = nullptr;      ///< 浏览文件按钮
    QPushButton *m_writeButton = nullptr;       ///< 写入配置按钮
    QPushButton *m_readButton = nullptr;        ///< 读取配置按钮
};
