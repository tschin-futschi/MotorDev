# MotorDev

MotorDev 是一款面向电机驱动模组调试的 Qt 上位机工具，目标是把寄存器读写、固件烧录和实时波形观测整合到同一个桌面应用中。

当前仓库处于早期开发阶段，已经完成：

- 项目愿景、协议和 UI 规格文档
- 基于 Qt6 Widgets 的主窗口和 4 个主功能页
- TopBar / ActivityBar / Sidebar / ContentArea / StatusBar 基础架构
- 串口管理、协议解析和基础日志能力
- CMake 构建配置

## Features

规划中的核心功能：

- 寄存器读写
- 固件烧录
- 实时示波
- 多语言支持
- 配置持久化

当前已经具备：

- UI 主框架
- 串口通信基础层
- 协议帧编解码
- `ConfigTab` 基础业务逻辑

尚未完成的是完整业务联调，以及烧录 / 示波等深层功能实现。

## Tech Stack

- C++17
- Qt 6 Widgets
- CMake
- MinGW

## Repository Layout

```text
MotorDev/
├─ CMakeLists.txt
├─ README.md
├─ CLAUDE.md
├─ codex.md
├─ design_spec.md
├─ protocol.md
├─ setup_qt_env.ps1
├─ docs/
│  ├─ vision.md
│  └─ tech_overview.md
├─ resources/
│  ├─ motordev_logo.svg
│  └─ resources.qrc
├─ src/
│  ├─ main.cpp
│  ├─ mainwindow.cpp
│  ├─ mainwindow.h
│  ├─ ui/
│  │  └─ style_constants.h
│  ├─ tabs/
│  └─ widgets/
└─ tmp_handoff/
```

## Requirements

建议使用与 Qt 配套的工具链：

- Qt `6.11.0`
- Qt MinGW `mingw1310_64`
- Qt CMake `CMake_64`

当前项目已在以下路径组合下验证通过：

- `D:\Qt\6.11.0\mingw_64`
- `D:\Qt\Tools\mingw1310_64`
- `D:\Qt\Tools\CMake_64`

如果你是在另一台机器上编译，核心要求不是路径必须和这里完全一致，而是下面 3 件事必须成立：

- Qt 版本使用 `6.11.0`
- 编译器使用与该 Qt 套件配套的 `MinGW 13.1.0 64-bit`
- `cmake.exe`、`g++.exe`、`mingw32-make.exe` 和 `Qt6_DIR` 必须指向同一套 Qt/MinGW 安装

## Build On Another Machine

推荐直接安装这些组件：

- Qt `6.11.0` `mingw_64`
- Qt Tools 中的 `mingw1310_64`
- Qt Tools 中的 `CMake_64`

安装完成后，先确认你机器上的真实路径。例如：

```text
C:\Qt\6.11.0\mingw_64
C:\Qt\Tools\mingw1310_64
C:\Qt\Tools\CMake_64
```

下面的命令里请把这些路径替换成你本机实际安装目录。

## Build

### Recommended Method

这个项目在当前环境里最稳定的构建方式是：

- 使用 Qt 自带的 `cmake.exe`
- 使用 Qt 对应的 `g++.exe`
- 使用 `MinGW Makefiles`
- 不依赖系统 `PATH` 自动找工具链

推荐在 PowerShell 中执行：

```powershell
$qtRoot = "C:\Qt\6.11.0\mingw_64"
$mingwRoot = "C:\Qt\Tools\mingw1310_64"
$cmakeRoot = "C:\Qt\Tools\CMake_64"

& "$cmakeRoot\bin\cmake.exe" `
  -S . `
  -B build_make_qt `
  -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH="$qtRoot" `
  -DQt6_DIR="$qtRoot\lib\cmake\Qt6" `
  -DCMAKE_CXX_COMPILER="$mingwRoot\bin\g++.exe" `
  -DCMAKE_MAKE_PROGRAM="$mingwRoot\bin\mingw32-make.exe"

& "$mingwRoot\bin\mingw32-make.exe" -C build_make_qt -j4
```

构建完成后可执行文件位于：

```text
build_make_qt/MotorDev.exe
```

### Optional: Configure Environment First

如果你希望先把当前 PowerShell 会话切到 Qt 环境，再执行构建命令，可以参考本仓库的 `setup_qt_env.ps1`。

这份脚本在本机写死的是：

- `D:\Qt\6.11.0\mingw_64`
- `D:\Qt\Tools\mingw1310_64`
- `D:\Qt\Tools\CMake_64`

所以换机器时，你需要先把脚本里的路径改成新机器的实际安装目录，然后再运行：

```powershell
. .\setup_qt_env.ps1
```

再构建：

```powershell
cmake -S . -B build_make_qt -G "MinGW Makefiles"
mingw32-make -C build_make_qt -j4
```

### Why Not Use Ninja By Default

这个仓库最初用过 `Ninja`，但在当前项目环境里，`Ninja` 更容易出现这些问题：

- `Detecting CXX compiler ABI info` 卡住
- 脏的构建目录残留 `.ninja_lock`
- 更难区分是生成器问题还是 Qt 配置问题

因此这里把 `MinGW Makefiles` 作为默认推荐方案。  
如果你已经在自己的机器上稳定验证过 `Ninja`，也可以自行切换，但 README 不把它作为首选。

### Common Problems

1. `Qt6` 找不到

- 检查 `-DCMAKE_PREFIX_PATH`
- 检查 `-DQt6_DIR`
- 确保它们都指向同一套 Qt `6.11.0\mingw_64`

2. `g++` 或 `mingw32-make` 不是 Qt 配套版本

- 不要依赖裸命令 `g++`
- 直接指定绝对路径，例如：

```text
D:\Qt\Tools\mingw1310_64\bin\g++.exe
D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe
```

3. `MotorDev.exe` 链接失败，提示文件被占用

- 先关闭正在运行的 `MotorDev.exe`
- 然后重新执行 `mingw32-make -C build_make_qt -j4`

4. `AutoMoc` 或子进程创建失败

- 如果是在受限终端或沙箱里构建，可能不是代码错误，而是环境限制
- 换到正常本地终端重新执行同一条构建命令

## Deploy

如果需要部署 Qt 运行时，在构建成功后执行：

```powershell
& "C:\Qt\6.11.0\mingw_64\bin\windeployqt.exe" .\build_make_qt\MotorDev.exe
```

## Documentation

项目文档以这些文件为准：

- `docs/vision.md`
- `docs/tech_overview.md`
- `design_spec.md`
- `protocol.md`
- `CLAUDE.md`
- `codex.md`

其中：

- `protocol.md` 是通信协议的唯一权威来源
- `design_spec.md` 是 UI 设计的当前基准
- `tmp_handoff/` 用于阶段计划、实现回执和 review 交接

## Current Status

当前阶段：

- UI 重构已完成
- 串口通信基础层已完成
- `ConfigTab` 基础业务逻辑已接入
- 控制台日志已接入
- 心跳已改为显式启动，不再在打开串口时自动启动

已验证：

- CMake 配置通过
- 项目编译通过
- `MotorDev.exe` 可成功启动
- `Scan / Connect / Disconnect / IC / Slave ID` 基础交互可用
- 串口基础收数链路已做手工验证

## License

Apache-2.0. See `LICENSE`.
