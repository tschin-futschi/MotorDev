// =============================================================================
// @file    aw_sdk_strategy.h
// @brief   Awinic SDK 烧录策略共用基类（AW86006 / AW86100 等共用同一份 DLL）
//
// 核心职责：
// - 通过 QLibrary 动态加载厂家 DLL（默认 AW86100.dll），运行时 resolve 函数指针
// - 编排 5 步流程：
//     AwOIS_ExtFuncInit → AwBootcontrol → AwMoveBinDownload → AwResetChip → AW_OIS_ExtFuncDeInit
// - 实现 3 个 ExtFunc 回调：
//     pFunc_I2C_Read  → 通过 CommandDispatcher 同步发送 0x31 等待响应
//     pFunc_I2C_Write → 通过 CommandDispatcher 同步发送 0x30 等待响应；下载阶段累计字节驱动进度
//     pFunc_OutputLog → 调 LogSink 转发到 FwFlashLogPanel
// - 取消传播：cancelFlag 翻 true 时 I2C 回调返回非零让 DLL 主动中止
// - 失败 / 取消收尾：强制 AwResetChip + AW_OIS_ExtFuncDeInit 让 IC 回到正常模式 + 释放 DLL 资源
// - 返回值统一：DLL 函数返回 0 视为成功，非零均失败（不区分枚举来源）
//
// 不 include AW86100.h；DLL 接口签名以 typedef 复刻在本文件，避免 dummy / 真实头文件耦合。
//
// 子类只需 override icModel() / icDescription() / dllPath()。
// =============================================================================
#pragma once

#include "services/flashstrategy.h"

#include <QLibrary>
#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>

class CommandDispatcher;

class AwSdkStrategy : public FlashStrategy {
public:
    /// @brief 烧录日志级别（与 FwFlashLogPanel 4 类着色对齐）
    enum class LogLevel { Info, Warn, Error, Ok };

    /// @brief 日志接收 sink；实现方需自行 marshal 到 GUI 线程（推荐 invokeMethod + QueuedConnection）
    using LogSink = std::function<void(LogLevel level, const QString &message)>;

    /// @brief IC 7-bit I2C 从机地址 provider；烧录开始时实时取 configtab 已配置的地址，
    /// 通过 `AwSet7bitI2CSlaveAddr` 同步给真实 DLL（DLL 内部默认地址未必等于用户的 IC 地址）。
    /// 合法范围 1~0x7F；返回 0 视为"未配置"，flash() 会以错误终止并提示用户先去配置 Tab 设置。
    using AddrProvider = std::function<uint8_t()>;

    AwSdkStrategy(CommandDispatcher *dispatcher, LogSink logSink, AddrProvider addrProvider);
    ~AwSdkStrategy() override;

    bool flash(const QByteArray &firmware,
               std::function<void(qint64 sentBytes)> progress,
               const std::atomic<bool> &cancelFlag,
               QString *errorMessage) override;

protected:
    /// @brief 子类返回 DLL 文件名（仅文件名，无路径），由基类与 `QCoreApplication::applicationDirPath()`
    /// 拼成绝对路径后加载，避免依赖 CWD 或系统默认搜索顺序。
    virtual QString dllPath() const = 0;

private:
    // ---- DLL 接口签名（与 AW86100.h 严格一致，但本文件不 include 该头）----
    // INT/BYTE/UINT32 在 windows.h 中分别为 int / unsigned char / unsigned int
    //
    // DevId 约定：以下所有 I2C 回调中的 devId 均为 7-bit I2C 从机地址（0x00–0x7F），
    // 不携带 R/W 位；AW SDK 文档保证 DLL 传出值始终满足该约束。STM32 端在生成总线
    // 字节时自行 `(devId << 1) | R/W` 形成 8-bit 总线地址。
    using DllInt    = int;
    using DllByte   = unsigned char;
    using DllUInt32 = unsigned int;

    using FnReadI2c   = DllInt (*)(DllByte devId, DllByte addrSize, DllByte *pAddr,
                                    DllByte rdSize, DllByte *pRdBuf);
    using FnWriteI2c  = DllInt (*)(DllByte devId, DllByte wrSize, DllByte *wrData);
    using FnOutputLog = DllInt (*)(const char *str);

    struct DllExtFuncList {
        FnReadI2c   pFunc_I2C_Read;
        FnWriteI2c  pFunc_I2C_Write;
        FnOutputLog pFunc_OutputLog;
    };

    using FnExtFuncInit     = DllInt (*)(DllExtFuncList *);
    using FnExtFuncDeInit   = DllInt (*)();
    using FnBootcontrol     = DllInt (*)(DllInt times, DllInt nDelayTime);
    using FnMoveBinDownload = DllInt (*)(DllByte *bypBinBuf, DllUInt32 u32Len, DllInt eNumOfPackType);
    using FnResetChip       = DllInt (*)();
    using FnSetSlaveAddr    = DllInt (*)(DllByte byAddr);

    // ---- DLL 加载 ----
    bool ensureDllLoaded(QString *errorMessage);

    // ---- 5 步流程 ----
    bool doInit(QString *errorMessage);
    bool doBootcontrol(QString *errorMessage);
    bool doDownload(QByteArray firmware, QString *errorMessage);
    bool doResetChip(QString *errorMessage);
    void doDeInit();

    // ---- 把用户在 configtab 配置的 IC 7-bit 地址同步给 DLL（必须在 doInit 之后、doBootcontrol 之前调用） ----
    bool applySlaveAddress(QString *errorMessage);

    // ---- 异常退出收尾（失败 / 取消） ----
    void emergencyCleanup();

    // ---- C-style thunk（DLL 直接调用，通过 thread_local 实例指针路由到当前 strategy） ----
    static int extReadI2cThunk(DllByte devId, DllByte addrSize, DllByte *pAddr,
                                DllByte rdSize, DllByte *pRdBuf);
    static int extWriteI2cThunk(DllByte devId, DllByte wrSize, DllByte *wrData);
    static int extOutputLogThunk(const char *str);

    // ---- thunk 转发到的实例方法 ----
    int onReadI2c(DllByte devId, DllByte addrSize, DllByte *pAddr,
                   DllByte rdSize, DllByte *pRdBuf);
    int onWriteI2c(DllByte devId, DllByte wrSize, DllByte *wrData);
    int onOutputLog(const char *str);

    // ---- 同步 I2C 透传（在 worker 线程 marshal 到 dispatcher 主线程并阻塞等待） ----
    // 参数约定：
    //   devId    — 7-bit I2C 从机地址（0x00–0x7F），不携带 R/W 位；调用方必须保证
    //              devId < 0x80（Debug 编译有断言）；编码层亦有 `& 0x7F` 兜底防呆
    //   addrSize / addr — 寄存器地址字节数与缓冲；addrSize == 0 时 addr 可为 nullptr
    int syncI2cWrite(uint8_t devId, uint8_t addrSize, const uint8_t *addr,
                     uint8_t dataLen, const uint8_t *data, int timeoutMs);
    int syncI2cRead(uint8_t devId, uint8_t addrSize, const uint8_t *addr,
                    uint8_t readLen, uint8_t *outBuf, int timeoutMs);

    // ---- 进度上报（节流）----
    void notifyProgress(qint64 sentDelta);

    // ---- 日志辅助 ----
    void log(LogLevel level, const QString &message) const;

    CommandDispatcher *m_dispatcher = nullptr;
    LogSink m_logSink;
    AddrProvider m_addrProvider;

    QLibrary          m_library;
    FnExtFuncInit     m_fnInit = nullptr;
    FnExtFuncDeInit   m_fnDeInit = nullptr;
    FnBootcontrol     m_fnBootcontrol = nullptr;
    FnMoveBinDownload m_fnMoveBinDownload = nullptr;
    FnResetChip       m_fnResetChip = nullptr;
    FnSetSlaveAddr    m_fnSetSlaveAddr = nullptr;

    // ---- flash() 调用期间使用 ----
    const std::atomic<bool> *m_cancelFlag = nullptr;
    std::function<void(qint64)> m_progress;
    qint64 m_sentBytes = 0;
    qint64 m_lastReportedBytes = 0;
    qint64 m_lastReportTimeMs = 0;
    bool m_inDownload = false;
    bool m_initCalled = false;   ///< 是否已成功 ExtFuncInit（决定收尾是否需要 DeInit）
    bool m_bootEntered = false;  ///< 是否已成功 Bootcontrol（决定收尾是否需要 ResetChip）

    // ---- 静态 thread_local 实例指针（C-style 回调路由） ----
    static thread_local AwSdkStrategy *s_currentInstance;
};
