# MotorDev

MotorDev 是一款面向电机驱动模组调试的 Qt 上位机工具，目标是把寄存器读写、固件烧录和实时波形观测整合到同一个桌面应用中。

当前仓库处于早期开发阶段，已经完成：

- 项目愿景、协议和 UI 规格文档
- 基于 Qt6 Widgets 的主窗口骨架
- TopBar / ActivityBar / Sidebar / ContentArea / StatusBar 基础架构
- 3 个功能页的占位页面
- CMake 构建配置

## Features

规划中的核心功能：

- 寄存器读写
- 固件烧录
- 实时示波
- 多语言支持
- 配置持久化

当前已实现的是 UI skeleton，不包含串口通信、协议解析、烧录逻辑和示波逻辑。

## Tech Stack

- C++17
- Qt 6 Widgets
- CMake
- MinGW
- Ninja

## Repository Layout

```text
MotorDev/
├─ CMakeLists.txt
├─ README.md
├─ CLAUDE.md
├─ agent.md
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
- Qt Ninja

当前项目已在以下路径组合下验证通过：

- `D:\Qt\6.11.0\mingw_64`
- `D:\Qt\Tools\mingw1310_64`
- `D:\Qt\Tools\CMake_64`
- `D:\Qt\Tools\Ninja`

## Build

如果你的系统环境已经正确配置到 Qt 工具链，直接执行：

```powershell
cmake -S . -B build_qt -G Ninja
cmake --build build_qt
```

如果当前终端没有切到正确的 Qt 环境，可以先运行：

```powershell
. .\setup_qt_env.ps1
```

然后再构建：

```powershell
cmake -S . -B build_qt -G Ninja
cmake --build build_qt
```

构建完成后可执行文件位于：

```text
build_qt/MotorDev.exe
```

如果需要部署 Qt 运行时：

```powershell
windeployqt .\build_qt\MotorDev.exe
```

## Documentation

项目文档以这些文件为准：

- `docs/vision.md`
- `docs/tech_overview.md`
- `design_spec.md`
- `protocol.md`
- `CLAUDE.md`
- `agent.md`

其中：

- `protocol.md` 是通信协议的唯一权威来源
- `design_spec.md` 是 UI 设计的当前基准
- `tmp_handoff/` 用于阶段计划、实现回执和 review 交接

## Current Status

当前阶段：`ui_skeleton`

已验证：

- CMake 配置通过
- 项目编译通过
- `MotorDev.exe` 可成功启动

## License

Apache-2.0. See [LICENSE](/D:/echo.qfq/26_Qt/MotorDev/LICENSE).
