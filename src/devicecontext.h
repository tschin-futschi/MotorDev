#pragma once

#include <QObject>
#include <QString>

#include <cstdint>

enum class MotorIcType {
    AW86006,
    DW9786,
    DW9788
};

class DeviceContext : public QObject {
    Q_OBJECT

public:
    explicit DeviceContext(QObject *parent = nullptr);

    static MotorIcType motorIcTypeFromIndex(int index);
    static int indexFromMotorIcType(MotorIcType type);
    static QString motorIcTypeToString(MotorIcType type);
    static bool isValidSlaveAddress(uint addr) { return addr >= 1 && addr <= 0x7F; }

    MotorIcType icType() const;
    void setIcType(MotorIcType type);

    uint8_t slaveId() const;
    void setSlaveId(uint8_t id);

signals:
    void icTypeChanged(MotorIcType type);
    void slaveIdChanged(uint8_t id);

private:
    MotorIcType m_icType = MotorIcType::AW86006;
    uint8_t m_slaveId = 0x00;
};
