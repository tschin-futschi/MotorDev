// =============================================================================
// @file    ftdi_latency_helper.cpp
// @brief   FT232 Latency Timer 自动设置工具实现
//
// 实现要点：
//   - 运行时 LoadLibrary("ftd2xx.dll")，避免编译期对 FTDI SDK 的硬依赖
//   - 通过 QSerialPortInfo 拿 portName 对应设备的 serialNumber，
//     用 FT_OpenEx + FT_OPEN_BY_SERIAL_NUMBER 精确匹配 FT232 设备
//   - 设置完 LatencyTimer 后立即 FT_Close，不持有 D2XX 句柄，
//     避免与后续 QSerialPort（VCP 模式）打开互锁
//   - LatencyTimer 写入 FT232 芯片寄存器是持久的，D2XX 关闭后
//     VCP 仍生效，直到 USB 拔插再恢复 Windows 注册表配置
// =============================================================================

#include "ftdi_latency_helper.h"

#include <QSerialPortInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace FtdiLatencyHelper {

#ifdef Q_OS_WIN

namespace {

// FTDI D2XX 公开 API 签名（来自 D2XX Programmer's Guide v1.4）。
// 这些类型在 .cpp 内部声明，避免对 ftd2xx.h 的编译期依赖。
using FT_STATUS = unsigned long;
using FT_HANDLE = void *;

constexpr FT_STATUS FT_OK = 0;
constexpr unsigned long FT_OPEN_BY_SERIAL_NUMBER = 1;
constexpr unsigned short FTDI_VENDOR_ID = 0x0403;

using PFN_FT_CreateDeviceInfoList = FT_STATUS (WINAPI *)(unsigned long *);
using PFN_FT_OpenEx               = FT_STATUS (WINAPI *)(void *, unsigned long, FT_HANDLE *);
using PFN_FT_SetLatencyTimer      = FT_STATUS (WINAPI *)(FT_HANDLE, unsigned char);
using PFN_FT_Close                = FT_STATUS (WINAPI *)(FT_HANDLE);

struct D2xxProcs {
    HMODULE module = nullptr;
    PFN_FT_CreateDeviceInfoList createDeviceInfoList = nullptr;
    PFN_FT_OpenEx               openEx               = nullptr;
    PFN_FT_SetLatencyTimer      setLatencyTimer      = nullptr;
    PFN_FT_Close                closeHandle          = nullptr;

    bool valid() const {
        return (module != nullptr) && (createDeviceInfoList != nullptr)
               && (openEx != nullptr) && (setLatencyTimer != nullptr)
               && (closeHandle != nullptr);
    }
};

D2xxProcs loadD2xx() {
    D2xxProcs procs;
    procs.module = ::LoadLibraryW(L"ftd2xx.dll");
    if (procs.module == nullptr) {
        return procs;
    }
    procs.createDeviceInfoList = reinterpret_cast<PFN_FT_CreateDeviceInfoList>(
        ::GetProcAddress(procs.module, "FT_CreateDeviceInfoList"));
    procs.openEx = reinterpret_cast<PFN_FT_OpenEx>(
        ::GetProcAddress(procs.module, "FT_OpenEx"));
    procs.setLatencyTimer = reinterpret_cast<PFN_FT_SetLatencyTimer>(
        ::GetProcAddress(procs.module, "FT_SetLatencyTimer"));
    procs.closeHandle = reinterpret_cast<PFN_FT_Close>(
        ::GetProcAddress(procs.module, "FT_Close"));
    return procs;
}

void unloadD2xx(D2xxProcs &procs) {
    if (procs.module != nullptr) {
        ::FreeLibrary(procs.module);
        procs.module = nullptr;
    }
}

// 通过 QSerialPortInfo 查找 portName 对应设备的 FTDI 序列号；
// 同时确认 VID == 0x0403，避免误把非 FTDI 桥（CP210x / CH340）当 FTDI 处理。
QString findFtdiSerialForPort(const QString &portName, QString &outErrorMessage) {
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &info : ports) {
        if (info.portName() != portName) {
            continue;
        }
        if (!info.hasVendorIdentifier() || info.vendorIdentifier() != FTDI_VENDOR_ID) {
            outErrorMessage = QStringLiteral("port %1 is not an FTDI device (VID=0x%2)")
                                  .arg(portName)
                                  .arg(info.hasVendorIdentifier() ? info.vendorIdentifier() : 0,
                                       4, 16, QLatin1Char('0'));
            return {};
        }
        const QString serial = info.serialNumber();
        if (serial.isEmpty()) {
            outErrorMessage = QStringLiteral("FTDI port %1 has empty serial number").arg(portName);
            return {};
        }
        return serial;
    }
    outErrorMessage = QStringLiteral("port %1 not found in available ports").arg(portName);
    return {};
}

}  // namespace

bool setLatencyTimerForPort(const QString &portName,
                             quint8 latencyMs,
                             QString &outSerialNumber,
                             QString &outErrorMessage) {
    outSerialNumber.clear();
    outErrorMessage.clear();

    const QString serial = findFtdiSerialForPort(portName, outErrorMessage);
    if (serial.isEmpty()) {
        return false;
    }

    D2xxProcs procs = loadD2xx();
    if (!procs.valid()) {
        unloadD2xx(procs);
        outErrorMessage = QStringLiteral("ftd2xx.dll not available or missing symbols");
        return false;
    }

    bool ok = false;
    do {
        const QByteArray serialAscii = serial.toLatin1();
        FT_HANDLE handle = nullptr;
        FT_STATUS st = procs.openEx(const_cast<char *>(serialAscii.constData()),
                                     FT_OPEN_BY_SERIAL_NUMBER,
                                     &handle);
        if (st != FT_OK || handle == nullptr) {
            outErrorMessage = QStringLiteral("FT_OpenEx failed for serial=%1 (st=%2)")
                                  .arg(serial).arg(st);
            break;
        }

        st = procs.setLatencyTimer(handle, latencyMs);
        if (st != FT_OK) {
            outErrorMessage = QStringLiteral("FT_SetLatencyTimer(%1) failed (st=%2)")
                                  .arg(latencyMs).arg(st);
            (void)procs.closeHandle(handle);
            break;
        }

        (void)procs.closeHandle(handle);
        outSerialNumber = serial;
        ok = true;
    } while (false);

    unloadD2xx(procs);
    return ok;
}

#else  // !Q_OS_WIN

bool setLatencyTimerForPort(const QString &portName,
                             quint8 /*latencyMs*/,
                             QString &outSerialNumber,
                             QString &outErrorMessage) {
    outSerialNumber.clear();
    outErrorMessage = QStringLiteral(
                          "D2XX latency adjustment only available on Windows (port=%1)")
                          .arg(portName);
    return false;
}

#endif

}  // namespace FtdiLatencyHelper
