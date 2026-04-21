#include "devicecontext.h"

MotorIcType DeviceContext::motorIcTypeFromIndex(int index) {
    switch (index) {
    case 1: return MotorIcType::DW9786;
    case 2: return MotorIcType::DW9788;
    case 0:
    default: return MotorIcType::AW86006;
    }
}

int DeviceContext::indexFromMotorIcType(MotorIcType type) {
    switch (type) {
    case MotorIcType::DW9786: return 1;
    case MotorIcType::DW9788: return 2;
    case MotorIcType::AW86006:
    default: return 0;
    }
}

QString DeviceContext::motorIcTypeToString(MotorIcType type) {
    switch (type) {
    case MotorIcType::AW86006: return QStringLiteral("AW86006");
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
        return;
    }

    m_icType = type;
    emit icTypeChanged(m_icType);
}

uint8_t DeviceContext::slaveId() const {
    return m_slaveId;
}

void DeviceContext::setSlaveId(uint8_t id) {
    if (m_slaveId == id) {
        return;
    }

    m_slaveId = id;
    emit slaveIdChanged(m_slaveId);
}
