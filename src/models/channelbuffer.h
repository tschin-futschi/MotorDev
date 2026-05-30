// =============================================================================
// @file    channelbuffer.h
// @brief   示波器单通道数据缓冲区
//
// 实现双层环形缓冲区架构：
// - 原始数据环（rawRing）：存储所有采样点，支持高频写入
// - 显示数据环（uiRing）：对原始数据做 1:N 抽稀后的结果，供绘图控件使用
//
// 降采样策略：每 bucketWidth 个原始样本取末值（桶内最后一个样本），写入 uiRing。
// 取末值而非中位数：保留单调信号（如锯齿波）的实际步长，避免均值化导致的步长伪影；
// 在高采样率下既保持 UI 绘制性能，又便于链路完整性肉眼验证。
//
// 使用方 ScopePlotWidget 直接读取 uiRing 数组进行零堆分配渲染。
// =============================================================================

#pragma once

#include <QVector>

#include <array>
#include <vector>

/// @brief 单通道数据缓冲区
///
/// 双层环形缓冲区：raw 层接收原始采样，经 1:N 末值抽稀后写入 UI 层供绘图使用。
/// 非线程安全，仅在主线程中使用。
class ChannelBuffer {
public:
    static constexpr int kUiRingSize = 8192;  ///< UI 环形缓冲区容量（最大显示样本数）

    /// @brief 配置缓冲区参数（调用后自动清空）
    /// @param rawRingSize   原始数据环容量
    /// @param bucketWidth   降采样桶宽度（每多少个原始样本产出一个 UI 样本）
    void configure(int rawRingSize, int bucketWidth);

    /// @brief 追加一个原始采样值
    /// @param sample  采样值（int16 → double）
    void appendRaw(double sample);

    /// @brief 清空所有缓冲区数据
    void clear();

    // --- UI 环形缓冲区访问接口（供 ScopePlotWidget 零堆分配读取）---

    /// @brief 当前 UI 环中的有效样本数
    int uiCount() const { return m_uiCount; }

    /// @brief 当前 UI 环的写入头位置
    int uiHead() const { return m_uiHead; }

    /// @brief 获取 UI 环的底层数组引用（只读）
    const std::array<float, kUiRingSize> &uiRing() const { return m_uiRing; }

    /// @brief 当前原始数据环中的有效样本数
    int rawCount() const { return m_rawCount; }

private:
    /// @brief 就地计算中值（会修改输入数组的顺序）
    /// @param values  采样值数组
    /// @param count   有效元素个数
    /// @return 中值
    static double medianValueInPlace(std::vector<double> &values, int count);

    /// @brief 将一个降采样后的值写入 UI 环
    void writeDisplaySample(double value);

    // --- 原始数据环 ---
    QVector<double> m_rawRing;    ///< 原始数据环形缓冲区
    int m_rawHead = 0;            ///< 原始环写入头位置
    int m_rawCount = 0;           ///< 原始环有效样本数

    // --- UI 显示数据环（固定大小，栈/静态分配，避免堆分配）---
    std::array<float, kUiRingSize> m_uiRing {};  ///< UI 显示环形缓冲区
    int m_uiHead = 0;             ///< UI 环写入头位置
    int m_uiCount = 0;            ///< UI 环有效样本数

    // --- 降采样桶 ---
    std::vector<double> m_bucketBuffer;  ///< 当前桶内积累的原始样本
    int m_bucketFill = 0;                ///< 当前桶内已填充的样本数
};
