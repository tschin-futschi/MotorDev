#include "devicecontext.h"

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
