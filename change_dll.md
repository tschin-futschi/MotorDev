# 更换供应商 DLL 操作指南

> 适用场景：实验室 / 内网拿到厂家真实 `AW86100.dll` 后，把当前工程内附带的 dummy DLL（stub，所有函数空实现返回 OK）替换为真实 DLL。
>
> 适用范围：AW86006 / AW86100 烧录策略所依赖的厂家 SDK DLL（两个 strategy 共用同一份 `AW86100.dll`）。

---

## 前置说明

MotorDev 工程通过 **`QLibrary` 动态加载** 厂家 DLL，编译期与 DLL 零耦合：

- 工程**不 link** `AW86100.lib`，**不 include** `AW86100.h`
- 5 个导出函数（`AwOIS_ExtFuncInit / Aw_OIS_ExtFuncDeInit / AwBootcontrol / AwMoveBinDownload / AwResetChip`）在运行时通过 `m_library.resolve("函数名")` 拿函数指针
- DLL 文件名固定 `AW86100.dll`，从 `MotorDev.exe` 同级目录加载
- DLL 加载延迟到**首次按下"开始烧录"**时执行；DLL 缺失或符号缺失只让本次烧录失败，不影响程序启动或其他 Tab

因此换 DLL 只是文件替换问题，**不需要重新对接 `.h` 头文件，不需要厂家 `.lib`**。

---

## 推荐方式：替换源码树 + 重新编译

### 步骤

1. **替换源码树里的 DLL**：把真实 `AW86100.dll` 覆盖到：

   ```
   D:\echo.qfq\26_Qt\MotorDev\ThirdParty\FlashLoad\AW86100\x64\AW86100.dll
   ```

   原 dummy 可选备份（重命名为 `AW86100_dummy.dll` 之类）。

2. **重新编译**（即使代码没改也建议跑一次）：

   ```powershell
   D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -C D:\echo.qfq\26_Qt\MotorDev\build_make_qt -j4
   ```

   `CMakeLists.txt:142-148` 的 `add_custom_command(POST_BUILD)` 会自动检测到 DLL 内容变化，把真实 DLL 拷贝到 `build_make_qt\AW86100.dll`（与 `MotorDev.exe` 同级）。

3. **启动并测试**：

   ```powershell
   D:\echo.qfq\26_Qt\MotorDev\build_make_qt\MotorDev.exe
   ```

   进入"固件烧录"页 → 选 AW86006 或 AW86100 → 选合法固件文件 → 按"开始烧录"。首次烧录时才会真正 load DLL。

---

## 一行替换命令参考

```powershell
copy "<真实DLL路径>" "D:\echo.qfq\26_Qt\MotorDev\ThirdParty\FlashLoad\AW86100\x64\AW86100.dll" /Y
D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -C D:\echo.qfq\26_Qt\MotorDev\build_make_qt -j4
D:\echo.qfq\26_Qt\MotorDev\build_make_qt\MotorDev.exe
```

---

## 不要这样做（陷阱）

**❌ 只把真实 DLL 复制到 `build_make_qt\AW86100.dll`（exe 同级），不动源码树**

理由：`CMakeLists.txt:142-148` POST_BUILD 命令用的是 `cmake -E copy_if_different`：

```cmake
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/ThirdParty/FlashLoad/AW86100/x64/AW86100.dll"   # 源（dummy）
    "$<TARGET_FILE_DIR:MotorDev>/AW86100.dll"                             # 目标 = exe 同级（你刚换上的真实 DLL）
```

`copy_if_different` 检测到源（源码树 dummy）≠ 目标（真实 DLL）→ 反向覆盖 → **真实 DLL 被 dummy 静默替换**。

下一次只要触发任何编译（即使代码没改、Qt MOC 触发了），真实 DLL 就被悄悄换回 dummy。**最坑的是 dummy 全返回 OK**，看起来烧录"成功"但其实什么都没做，极难排查。

**结论**：源码树和 exe 同级必须同时是真实 DLL；只动 exe 同级是不可靠的。

---

## 替换前检查清单

| # | 项 | 验证方式 | 不通过的后果 |
|---|-----|---------|-------------|
| 1 | 文件名必须是 `AW86100.dll` | 大小写也要严格一致 | strategy 子类 `dllPath()` 硬编码该文件名，文件名错就 load 失败 |
| 2 | 必须是 **x64（64-bit）** | `dumpbin /headers AW86100.dll \| findstr machine` 或 Dependency Walker 查看 | 工程是 mingw1310_64，Qt 64-bit；32-bit DLL load 会失败，`errorString()` 提示 architecture 不匹配 |
| 3 | 必须导出全部 5 个符号 | `dumpbin /exports AW86100.dll`（VS 命令行工具）查看导出表 | 任一 resolve 失败本次烧录失败、日志面板提示具体哪个符号缺失 |
| 4 | DLL 自身依赖项也要部署 | `dumpbin /dependents AW86100.dll` 列出依赖；常见依赖是 VCRuntime / MSVC runtime / 其他厂家 .dll | 依赖缺失 → load 失败，`errorString()` 可能不够具体，需 Dependency Walker 进一步排查 |
| 5 | 不需要 `.lib`，不需要 `.h` | — | 工程动态加载，编译期零依赖 |

### 必须导出的 5 个符号（区分大小写）

```
AwOIS_ExtFuncInit
Aw_OIS_ExtFuncDeInit
AwBootcontrol
AwMoveBinDownload
AwResetChip
```

> 厂家头文件 `AW86100.h` 里还声明了其他函数（`AwGetDllVersion / AwFlashDownload / AwSet7bitI2CSlaveAddr / AwGet7bitI2CSlaveAddr / AwFlashRead` 等），但本工程**不调用**它们，是否导出不影响烧录。

---

## 替换后快速验证

### 验证 1：DLL 加载成功

启动程序 → 进固件烧录页 → 选 AW86006 → 选合法 `.bin` 或 `.hex` 文件 → 按"开始烧录"。

观察日志面板：

- **若 DLL 加载失败**：出现 `Failed to load AW86100.dll: <reason>` 或 `Failed to resolve <symbol>` 红色行 → 检查清单第 1–4 项
- **若 DLL 加载成功**：进入 5 步序列日志 `[INFO] AwOIS_ExtFuncInit` → `AwBootcontrol` → `AwMoveBinDownload` → ...

### 验证 2：dummy vs 真实 DLL 区分（重要）

`aw_sdk_strategy.cpp:303-306` 的 `onOutputLog` 把 DLL 的 `pFunc_OutputLog` 回调转发到日志面板，前缀 `[DLL]`：

- **dummy DLL**（stub 空实现）：日志面板**不会出现** `[DLL] xxx` 行（stub 不调用 OutputLog 回调）
- **真实 DLL**：必然出现多条 `[DLL] <SDK 内部日志>` 行（厂家 SDK 真实工作时会输出诊断信息）

**判断口诀**：日志面板有 `[DLL] ...` 行 = 跑的是真实 DLL；只有 `[INFO] AwXxx ...` 没有 `[DLL] ...` = 跑的是 dummy。

### 验证 3：源码树与 exe 同级一致

```powershell
fc /b ^
   D:\echo.qfq\26_Qt\MotorDev\ThirdParty\FlashLoad\AW86100\x64\AW86100.dll ^
   D:\echo.qfq\26_Qt\MotorDev\build_make_qt\AW86100.dll
```

输出 `FC: 找不到差异` / `no differences encountered` 即一致。

---

## 故障排查

| 现象 | 可能原因 | 排查方法 |
|------|---------|---------|
| 日志：`Failed to load AW86100.dll: 加载库时出错` | DLL 文件不存在 / 文件名错 / 架构不匹配 / 依赖 DLL 缺失 | 1. 用资源管理器确认 exe 同级目录存在 `AW86100.dll` 且大小写一致<br>2. `dumpbin /headers` 检查 machine 字段是否为 `x64`<br>3. `dumpbin /dependents` 检查依赖项是否齐全 |
| 日志：`Failed to resolve <符号名>` | DLL 没导出该符号 / 符号名拼写不同 | `dumpbin /exports AW86100.dll` 查看完整导出表；与上文 5 个必须符号逐一比对（注意 `Aw_OIS_ExtFuncDeInit` 中下划线位置） |
| 烧录"成功"但 IC 没反应 | 真实 DLL 被 POST_BUILD 反向覆盖回 dummy | 用本文档 **验证 2** 检查日志面板是否有 `[DLL] ...` 行；若无 → 真实 DLL 已被替换回 dummy；按"推荐方式"重新同步源码树 |
| 日志：`AwBootcontrol failed (return -X)` | DLL 加载正常但烧录序列失败 | 不是 DLL 替换问题；查 STM32 端 0x30/0x31 实现是否就绪，或电源/连线/IC 状态 |
| 烧录速度异常慢 / 卡死 | I2C 总线问题 / STM32 端 0x30/0x31 未实现 / 真实 DLL 等待响应时序 | 查 STM32 调试 UART、抓 I2C 总线波形 |

---

## DLL 文件来源与版本管理

- 当前 dummy DLL 来源：`ThirdParty/FlashLoad/AW86100/stub/AW86100_stub.cpp` 编译产物（独立编译，不在 MotorDev CMake 中）
- 真实 DLL 来源：厂家（Awinic）通过内网 / 离线介质提供
- 版本约定：每次更换真实 DLL 时，建议在源码树同级新建 `AW86100_v<版本号>_<日期>.dll` 备份（不要污染主 `AW86100.dll`），便于回滚

---

## 厂家未来更新 `AW86100.h` 时的额外动作

99% 情况下厂家**不会**改 5 个核心函数签名（会破坏所有客户）；若真改了：

1. 用厂家新版 `AW86100.h` 覆盖 `ThirdParty/FlashLoad/AW86100/AW86100.h`（让 dummy stub 编译跟得上）
2. **手动同步** `src/services/flashstrategies/aw_sdk_strategy.h:55-65` 的 typedef 复刻（typedef 必须与厂家签名严格一致）
3. 重编工程

> 复刻同步规则见 `aw_sdk_strategy.h:17` 注释；工程主代码刻意不 include `AW86100.h` 是为了换 DLL 时零编译依赖的设计取舍，代价是签名同步靠人工。

---

## 相关文件速查

| 文件 | 作用 |
|------|------|
| `ThirdParty/FlashLoad/AW86100/x64/AW86100.dll` | DLL 源文件位置（被 POST_BUILD 拷贝到 exe 同级） |
| `ThirdParty/FlashLoad/AW86100/AW86100.h` | 厂家头文件，仅供 dummy stub 编译 + typedef 复刻人工对照，主代码不 include |
| `ThirdParty/FlashLoad/AW86100/stub/AW86100_stub.cpp` | dummy DLL 的实现源码（不在 MotorDev CMake，独立编译） |
| `src/services/flashstrategies/aw_sdk_strategy.h` | DLL 接口签名 typedef 复刻 + 共用基类声明 |
| `src/services/flashstrategies/aw_sdk_strategy.cpp` | 5 步序列编排 + DLL 动态加载实现 + 同步 I2C 透传 |
| `CMakeLists.txt:142-148` | POST_BUILD 拷贝命令 |
| `protocol.md` 0x30 / 0x31 章节 | I2C 透传协议规格（DLL 通过它与 STM32 通信） |
