// =============================================================================
// @file    devicecontext.h
// @brief   设备上下文（数据模型层）
//
// 持有当前连接设备的 IC 类型和 I2C 从机地址状态。
// 提供 IC 类型枚举与索引/字符串之间的转换工具方法。
//
// 使用方式：由 MainWindow 创建，ConfigTab 负责读写，其他 Tab 可读取。
// =============================================================================

#pragma once

#include <QObject>
#include <QString>

#include <cstdint>

/// @brief 电机驱动 IC 类型枚举
enum class MotorIcType {
    AW86008,   ///< Awinic AW86008
    DW9786,    ///< Dongwoon DW9786
    DW9788     ///< Dongwoon DW9788
};

/// @brief 设备上下文
///
/// 保存当前 IC 类型和 I2C 从机地址的全局状态。
/// 当状态变化时发出对应信号，方便 UI 响应更新。
///
/// 线程安全：仅在主线程使用。
class DeviceContext : public QObject {
    Q_OBJECT

public:
    explicit DeviceContext(QObject *parent = nullptr);

    /// @brief IC 类型枚举与下拉框索引互转
    /// @param index  下拉框索引（0=AW86008, 1=DW9786, 2=DW9788）
    /// @return 对应的 IC 类型
    static MotorIcType motorIcTypeFromIndex(int index);

    /// @brief IC 类型转下拉框索引
    static int indexFromMotorIcType(MotorIcType type);

    /// @brief IC 类型转显示字符串
    static QString motorIcTypeToString(MotorIcType type);

    /// @brief 验证 I2C 从机地址是否合法（1~0x7F）
    static bool isValidSlaveAddress(uint addr) { return addr >= 1 && addr <= 0x7F; }

    /// @brief 获取当前 IC 类型
    MotorIcType icType() const;

    /// @brief 设置 IC 类型（值不变时不发信号）
    void setIcType(MotorIcType type);

    /// @brief 获取当前 I2C 从机地址
    uint8_t slaveId() const;

    /// @brief 设置 I2C 从机地址（值不变时不发信号）
    void setSlaveId(uint8_t id);

signals:
    /// @brief IC 类型变更信号
    void icTypeChanged(MotorIcType type);

    /// @brief 从机地址变更信号
    void slaveIdChanged(uint8_t id);

private:
    MotorIcType m_icType = MotorIcType::AW86008;  ///< 当前 IC 类型（默认 AW86008）
    uint8_t m_slaveId = 0x00;                      ///< 当前 I2C 从机地址（默认 0x00）
};
