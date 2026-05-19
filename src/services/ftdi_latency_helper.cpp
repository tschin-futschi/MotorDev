// =============================================================================
// @file    ftdi_latency_helper.cpp
// @brief   FT232 Latency Timer 自动设置工具实现
//
// 实现要点：
//   - 运行时 LoadLibrary("ftd2xx.dll")，避免编译期对 FTDI SDK 的硬依赖
//   - 通过 FT_CreateDeviceInfoList 枚举所有 FTDI 设备，逐个 FT_Open(index)
//     后调 FT_GetComPortNumber 反查 COM 编号，与 portName（"COM<n>"）匹配
//     —— 不依赖 Qt 暴露的 serialNumber（部分 FT232 出厂未烧 serial 时
//     QSerialPortInfo 会返回 Windows USB 实例 ID，导致按 serial 打开失败）
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
constexpr unsigned short FTDI_VENDOR_ID = 0x0403;

using PFN_FT_CreateDeviceInfoList = FT_STATUS (WINAPI *)(unsigned long *);
using PFN_FT_Open                 = FT_STATUS (WINAPI *)(int, FT_HANDLE *);
using PFN_FT_GetComPortNumber     = FT_STATUS (WINAPI *)(FT_HANDLE, long *);
using PFN_FT_SetLatencyTimer      = FT_STATUS (WINAPI *)(FT_HANDLE, unsigned char);
using PFN_FT_Close                = FT_STATUS (WINAPI *)(FT_HANDLE);

struct D2xxProcs {
    HMODULE module = nullptr;
    PFN_FT_CreateDeviceInfoList createDeviceInfoList = nullptr;
    PFN_FT_Open                 openByIndex          = nullptr;
    PFN_FT_GetComPortNumber     getComPortNumber     = nullptr;
    PFN_FT_SetLatencyTimer      setLatencyTimer      = nullptr;
    PFN_FT_Close                closeHandle          = nullptr;

    bool valid() const {
        return (module != nullptr) && (createDeviceInfoList != nullptr)
               && (openByIndex != nullptr) && (getComPortNumber != nullptr)
               && (setLatencyTimer != nullptr) && (closeHandle != nullptr);
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
    procs.openByIndex = reinterpret_cast<PFN_FT_Open>(
        ::GetProcAddress(procs.module, "FT_Open"));
    procs.getComPortNumber = reinterpret_cast<PFN_FT_GetComPortNumber>(
        ::GetProcAddress(procs.module, "FT_GetComPortNumber"));
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

// 仅通过 VID 确认 portName 对应的是 FTDI 桥（不再依赖 serialNumber），
// 用于在加载 D2XX 前快速过滤掉 CP210x / CH340 等非 FTDI 设备，给出清晰错误。
bool isFtdiPortByVid(const QString &portName, QString &outErrorMessage) {
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
            return false;
        }
        return true;
    }
    outErrorMessage = QStringLiteral("port %1 not found in available ports").arg(portName);
    return false;
}

// 从 "COM<n>" 形式解析 n；非该形式返回 -1。
int parseComPortNumber(const QString &portName) {
    const QString trimmed = portName.trimmed();
    if (!trimmed.startsWith(QLatin1String("COM"), Qt::CaseInsensitive)) {
        return -1;
    }
    bool ok = false;
    const int n = trimmed.mid(3).toInt(&ok);
    return (ok && n > 0) ? n : -1;
}

}  // namespace

bool setLatencyTimerForPort(const QString &portName,
                             quint8 latencyMs,
                             QString &outSerialNumber,
                             QString &outErrorMessage) {
    outSerialNumber.clear();
    outErrorMessage.clear();

    // 0. 通过 VID 快速过滤非 FTDI 桥，避免无谓加载 D2XX
    if (!isFtdiPortByVid(portName, outErrorMessage)) {
        return false;
    }

    // 1. 解析目标 COM 号（FT_GetComPortNumber 返回的就是这个整数）
    const int targetComNo = parseComPortNumber(portName);
    if (targetComNo < 0) {
        outErrorMessage = QStringLiteral("port name %1 is not in COM<n> form").arg(portName);
        return false;
    }

    // 2. 动态加载 ftd2xx.dll
    D2xxProcs procs = loadD2xx();
    if (!procs.valid()) {
        unloadD2xx(procs);
        outErrorMessage = QStringLiteral("ftd2xx.dll not available or missing symbols");
        return false;
    }

    // 3. 枚举 FTDI 设备，逐个 FT_Open(index) → FT_GetComPortNumber 反查 COM 号匹配
    bool ok = false;
    do {
        unsigned long numDevs = 0;
        FT_STATUS st = procs.createDeviceInfoList(&numDevs);
        if (st != FT_OK) {
            outErrorMessage = QStringLiteral("FT_CreateDeviceInfoList failed (st=%1)").arg(st);
            break;
        }
        if (numDevs == 0) {
            outErrorMessage = QStringLiteral("no FTDI devices found via D2XX");
            break;
        }

        QString lastInnerError;
        for (unsigned long i = 0; i < numDevs; ++i) {
            FT_HANDLE handle = nullptr;
            st = procs.openByIndex(static_cast<int>(i), &handle);
            if (st != FT_OK || handle == nullptr) {
                lastInnerError = QStringLiteral("FT_Open(index=%1) failed (st=%2)")
                                     .arg(i).arg(st);
                continue;
            }

            long comNo = -1;
            st = procs.getComPortNumber(handle, &comNo);
            if (st != FT_OK) {
                lastInnerError = QStringLiteral("FT_GetComPortNumber(index=%1) failed (st=%2)")
                                     .arg(i).arg(st);
                (void)procs.closeHandle(handle);
                continue;
            }
            if (comNo < 0 || comNo != targetComNo) {
                (void)procs.closeHandle(handle);
                continue;
            }

            // 匹配上目标 COM 号 —— 设置 LatencyTimer 后立即关闭，让 VCP 接管
            st = procs.setLatencyTimer(handle, latencyMs);
            if (st != FT_OK) {
                outErrorMessage = QStringLiteral(
                    "FT_SetLatencyTimer(%1) failed for index=%2 (st=%3)")
                    .arg(latencyMs).arg(i).arg(st);
                (void)procs.closeHandle(handle);
                break;
            }
            (void)procs.closeHandle(handle);
            outSerialNumber = QStringLiteral("index=%1,COM%2").arg(i).arg(comNo);
            ok = true;
            break;
        }

        if (!ok && outErrorMessage.isEmpty()) {
            if (!lastInnerError.isEmpty()) {
                outErrorMessage = QStringLiteral(
                    "no FTDI device matched %1 (scanned %2 devices; last error: %3)")
                    .arg(portName).arg(numDevs).arg(lastInnerError);
            } else {
                outErrorMessage = QStringLiteral(
                    "no FTDI device matched %1 (scanned %2 devices, none reported this COM)")
                    .arg(portName).arg(numDevs);
            }
        }
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
