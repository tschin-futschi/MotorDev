// =============================================================================
// @file    scopegeneratorpanel.h
// @brief   波形生成器面板 — 线性/余弦模式参数配置与启停控制
//
// ScopeGeneratorPanel 嵌入在示波器底部面板中，支持两种波形生成模式：
// - 线性模式：地址 + min/max/step + 间隔 → 线性递增/递减写入
// - 余弦模式：幅值 + 偏移 + 频率 + 最多 3 通道（地址+相位）→ 余弦波形写入
//
// 波形生成由 STM32 侧执行，上位机仅下发参数和启停命令。
// =============================================================================
#pragma once

#include <QVector>
#include <QWidget>

class QButtonGroup;
class QLineEdit;
class QRadioButton;
class QPushButton;
class QStackedWidget;

/// @brief 余弦通道参数结构体
struct ScopeGeneratorCosineChannel {
    quint16 addr = 0;       ///< 寄存器地址
    double phaseDeg = 0.0;  ///< 相位偏移（度）
};

/// @brief 波形生成器面板 — 线性/余弦模式切换 + 参数输入 + 启停
class ScopeGeneratorPanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeGeneratorPanel(QWidget *parent = nullptr);
    ~ScopeGeneratorPanel() override;

    /// @brief 设置运行状态（切换启动/停止按钮可用性）
    void setRunning(bool running);

signals:
    /// @brief 请求启动线性波形生成
    /// @param addr 目标寄存器地址
    /// @param min 最小值
    /// @param max 最大值
    /// @param step 步进值
    /// @param intervalMs 写入间隔（毫秒）
    void linearStartRequested(quint16 addr, qint16 min, qint16 max, qint16 step, int intervalMs);

    /// @brief 请求启动余弦波形生成
    /// @param amplitude 幅值
    /// @param offset 偏移量
    /// @param frequencyHz 频率（Hz）
    /// @param channels 通道列表（最多 3 个）
    void cosineStartRequested(qint16 amplitude, qint16 offset, double frequencyHz,
                              const QVector<ScopeGeneratorCosineChannel> &channels);

    /// @brief 请求启动锯齿波测试发生器
    /// @param addr 目标寄存器地址
    /// @param min 最小值
    /// @param max 最大值
    /// @param step 步进值
    void sawtoothStartRequested(quint16 addr, qint16 min, qint16 max, qint16 step);

    /// @brief 请求停止波形生成
    void stopRequested();

private:
    void setupUi();
    void connectSignals();

    /// @brief 设置输入框错误样式
    void setFieldError(QLineEdit *edit, bool error);

    /// @brief 解析 uint16 字段（支持 0x 前缀）
    bool parseUint16Field(QLineEdit *edit, quint16 *out, bool allowEmpty = false);

    /// @brief 解析 int16 字段
    bool parseInt16Field(QLineEdit *edit, qint16 *out);

    /// @brief 解析正整数字段
    bool parsePositiveIntField(QLineEdit *edit, int *out);

    /// @brief 解析浮点字段
    bool parseDoubleField(QLineEdit *edit, double *out, bool mustBePositive);

    /// @brief 启动按钮点击处理（分发到线性/余弦）
    void handleStartClicked();
    void handleLinearStart();
    void handleCosineStart();
    void handleSawtoothStart();

    /// @brief 清除所有输入框的错误样式
    void clearErrors();

    // --- 模式选择 ---
    QButtonGroup *m_modeGroup = nullptr;        ///< 线性/余弦互斥按钮组
    QRadioButton *m_linearRadio = nullptr;      ///< 线性模式单选按钮
    QRadioButton *m_cosineRadio = nullptr;      ///< 余弦模式单选按钮
    QRadioButton *m_sawtoothRadio = nullptr;    ///< 锯齿波测试模式单选按钮
    QStackedWidget *m_modeStack = nullptr;      ///< 模式参数切换容器

    // --- 线性模式参数 ---
    QLineEdit *m_linearAddrEdit = nullptr;      ///< 目标地址
    QLineEdit *m_linearMinEdit = nullptr;       ///< 最小值
    QLineEdit *m_linearMaxEdit = nullptr;       ///< 最大值
    QLineEdit *m_linearStepEdit = nullptr;      ///< 步进值
    QLineEdit *m_linearIntervalEdit = nullptr;  ///< 写入间隔（ms）

    // --- 锯齿波测试模式参数 ---
    QLineEdit *m_sawtoothAddrEdit = nullptr;      ///< 目标地址
    QLineEdit *m_sawtoothMinEdit = nullptr;        ///< 最小值
    QLineEdit *m_sawtoothMaxEdit = nullptr;        ///< 最大值
    QLineEdit *m_sawtoothStepEdit = nullptr;       ///< 步进值

    // --- 余弦模式参数 ---
    QLineEdit *m_cosineAmplitudeEdit = nullptr;     ///< 幅值
    QLineEdit *m_cosineOffsetEdit = nullptr;        ///< 偏移量
    QLineEdit *m_cosineFrequencyEdit = nullptr;     ///< 频率（Hz）
    QLineEdit *m_cosineAddrEdits[3] = {};           ///< 3 通道地址
    QLineEdit *m_cosinePhaseEdits[3] = {};          ///< 3 通道相位偏移

    // --- 控制按钮 ---
    QPushButton *m_startButton = nullptr;       ///< 启动按钮
    QPushButton *m_stopButton = nullptr;        ///< 停止按钮
};
