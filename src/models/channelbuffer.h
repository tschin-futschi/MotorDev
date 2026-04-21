#pragma once

#include <QVector>

#include <array>
#include <vector>

class ChannelBuffer {
public:
    static constexpr int kUiRingSize = 3000;

    void configure(int rawRingSize, int bucketWidth);
    void appendRaw(double sample);
    void clear();

    int uiCount() const { return m_uiCount; }
    int uiHead() const { return m_uiHead; }
    const std::array<float, kUiRingSize> &uiRing() const { return m_uiRing; }
    int rawCount() const { return m_rawCount; }

private:
    static double medianValueInPlace(std::vector<double> &values, int count);
    void writeDisplaySample(double value);

    QVector<double> m_rawRing;
    int m_rawHead = 0;
    int m_rawCount = 0;

    std::array<float, kUiRingSize> m_uiRing {};
    int m_uiHead = 0;
    int m_uiCount = 0;

    std::vector<double> m_bucketBuffer;
    int m_bucketFill = 0;
};
