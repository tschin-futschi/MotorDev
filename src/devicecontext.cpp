// =============================================================================
// @file    devicecontext.cpp
// @brief   设备上下文实现
//
// IC 类型与索引/字符串的映射，以及 IC 类型和从机地址的存取逻辑。
// =============================================================================

#include "devicecontext.h"

/// @brief 下拉框索引转 IC 类型
/// @param index  下拉框索引
/// @return 对应的 IC 类型；无效索引默认返回 AW86008
MotorIcType DeviceContext::motorIcTypeFromIndex(int index) {
    switch (index) {
    case 1: return MotorIcType::AW86100;
    case 2: return MotorIcType::DW9786;
    case 3: return MotorIcType::DW9788;
    case 0:
    default: return MotorIcType::AW86008;
    }
}

/// @brief IC 类型转下拉框索引
/// @param type  IC 类型
/// @return 对应的下拉框索引（0/1/2）
int DeviceContext::indexFromMotorIcType(MotorIcType type) {
    switch (type) {
    case MotorIcType::AW86100: return 1;
    case MotorIcType::DW9786: return 2;
    case MotorIcType::DW9788: return 3;
    case MotorIcType::AW86008:
    default: return 0;
    }
}

/// @brief IC 类型转显示用字符串
/// @param type  IC 类型
/// @return 型号名称字符串
QString DeviceContext::motorIcTypeToString(MotorIcType type) {
    switch (type) {
    case MotorIcType::AW86008: return QStringLiteral("AW86008");
    case MotorIcType::AW86100: return QStringLiteral("AW86100");
    case MotorIcType::DW9786: return QStringLiteral("DW9786");
    case MotorIcType::DW9788: return QStringLiteral("DW9788");
    }
    return QStringLiteral("Unknown");
}

DeviceContext::DeviceContext(QObject *parent)
    : QObject(parent) {
}

MotorIcType DeviceContext::icType() const {
    return m_icType;
}

void DeviceContext::setIcType(MotorIcType type) {
    if (m_icType == type) {
        return;  // 值未变化，不发信号
    }

    m_icType = type;
    emit icTypeChanged(m_icType);
}

uint8_t DeviceContext::slaveId() const {
    return m_slaveId;
}

void DeviceContext::setSlaveId(uint8_t id) {
    if (m_slaveId == id) {
        return;  // 值未变化，不发信号
    }

    m_slaveId = id;
    emit slaveIdChanged(m_slaveId);
}
