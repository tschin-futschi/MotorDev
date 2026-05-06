// =============================================================================
// @file    channelbuffer.cpp
// @brief   示波器单通道数据缓冲区实现
//
// 实现原始数据接收、中值降采样和双层环形缓冲区管理。
// =============================================================================

#include "models/channelbuffer.h"

#include <algorithm>

void ChannelBuffer::configure(int rawRingSize, int bucketWidth) {
    m_rawRing.fill(0.0, qMax(1, rawRingSize));
    m_bucketBuffer.assign(static_cast<size_t>(qMax(1, bucketWidth)), 0.0);
    clear();
}

/// @brief 追加一个原始采样值
///
/// 流程：
/// 1. 写入原始数据环（覆盖式环形写入）
/// 2. 桶宽计数器递增
/// 3. 桶满时取末值（当前样本）写入 UI 环，然后清零计数器
///
/// 取末值而非中位数：保留单调信号（如 0x58 锯齿波）的实际步长，
/// 让链路完整性测试可用肉眼直接验证。bucket > 1 时仍是 1:N 抽稀，
/// 但不再产生均值化导致的"步长翻倍"伪影。
void ChannelBuffer::appendRaw(double sample) {
    // 防御性检查：未配置时自动用最小参数初始化
    if (m_rawRing.isEmpty() || m_bucketBuffer.empty()) {
        configure(1, 1);
    }

    // 写入原始数据环
    m_rawRing[m_rawHead] = sample;
    m_rawHead = (m_rawHead + 1) % m_rawRing.size();
    m_rawCount = qMin(m_rawCount + 1, m_rawRing.size());

    // 桶宽计数：每 bucketWidth 个原始样本写一个 UI 点（取末值）
    ++m_bucketFill;
    if (m_bucketFill >= static_cast<int>(m_bucketBuffer.size())) {
        writeDisplaySample(sample);
        m_bucketFill = 0;
    }
}

void ChannelBuffer::clear() {
    std::fill(m_uiRing.begin(), m_uiRing.end(), 0.0f);
    std::fill(m_rawRing.begin(), m_rawRing.end(), 0.0);
    std::fill(m_bucketBuffer.begin(), m_bucketBuffer.end(), 0.0);
    m_rawHead = 0;
    m_rawCount = 0;
    m_uiHead = 0;
    m_uiCount = 0;
    m_bucketFill = 0;
}

/// @brief 就地计算中值（当前未被调用，保留备用）
///
/// 使用 std::nth_element 实现 O(N) 中值查找（会修改输入数组元素顺序）。
/// 偶数个元素时取中间两个的平均值。
///
/// `appendRaw` 当前在桶满时取末值，不调用本函数。保留是为了未来切换回
/// 中位数降采样时无需重新实现。
double ChannelBuffer::medianValueInPlace(std::vector<double> &values, int count) {
    if (count <= 0) {
        return 0.0;
    }
    if (count == 1) {
        return values[0];
    }

    const int mid = count / 2;
    if ((count % 2) != 0) {
        // 奇数个元素：直接取中间值
        std::nth_element(values.begin(), values.begin() + mid, values.begin() + count);
        return values[static_cast<size_t>(mid)];
    }

    // 偶数个元素：取中间两个值的平均
    std::nth_element(values.begin(), values.begin() + mid, values.begin() + count);
    const double upper = values[static_cast<size_t>(mid)];
    std::nth_element(values.begin(), values.begin() + mid - 1, values.begin() + mid);
    return (values[static_cast<size_t>(mid - 1)] + upper) / 2.0;
}

/// @brief 将降采样后的值写入 UI 环形缓冲区
void ChannelBuffer::writeDisplaySample(double value) {
    m_uiRing[static_cast<size_t>(m_uiHead)] = static_cast<float>(value);
    m_uiHead = (m_uiHead + 1) % kUiRingSize;
    m_uiCount = qMin(m_uiCount + 1, kUiRingSize);
}
