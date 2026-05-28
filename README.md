# MotorDev

MotorDev 是一款面向电机驱动模组调试的 Qt 上位机工具，将寄存器读写、固件烧录、实时波形观测整合到同一桌面应用中。

通信链路：上位机（Windows） ⇄ 串口 ⇄ STM32 ⇄ I2C ⇄ 电机驱动 IC。

## Features

四个主功能页 + 一个浮动调试模拟器：

| 页面 | 状态 |
|------|------|
| **Config** 串口扫描 / IC 类型 / 从机地址 / PMIC 电压配置 / 设备控制 | 串口、IC、PMIC、设备控制按钮 已实现；Config File 行为 UI stub |
| **RegisterRW** 寄存器读写（4 组 × 30 行） | 单读 / 单写 / 批量读，启动自动加载 + 自动保存到 `AppDataLocation/registers.json`；用户文件选择导入导出为 stub |
| **FwFlash** 固件烧录 | 整体未实现（占位页） |
| **Oscilloscope** 8 通道实时示波 | OpenGL + QPainter 自绘渲染，60 fps 稳定；overlay/stacked 视图、X/Y 拖拽缩放、双击重置、十字光标读值（snap to sample）、通道样式面板（颜色/线宽/线型/数据点）、采样启停、信号发生器（Linear/Cosine/Sawtooth，由 STM32 执行）、寄存器侧面板（单写 / 周期写） |
| **SerialDebug**（浮动） | 串口调试模拟器，可在不连接真实硬件的情况下端到端跑通 I2C 扫描、寄存器读写、采样流帧、发生器命令等 |

公共基础设施已具备：

- 串口管理（独立线程、心跳检测、流帧 backpressure 防 OOM）
- 协议帧编解码（控制帧 0xAA55 + 流帧 0xBB；CRC16 已对齐固件实现）
- 命令分发器（CommandDispatcher 串行化命令，避免响应错配）
- 日志面板（支持 level / category / 源码位置 / 文件持久化）
- Splash screen、QSS 主题、自定义 logo / icon

## Tech Stack

- C++17
- Qt 6 Widgets（含 SerialPort / Svg / SvgWidgets / OpenGLWidgets / PrintSupport）
- CMake 3.16+
- MinGW 13.1.0 64-bit

## Repository Layout

```text
MotorDev/
├─ CMakeLists.txt
├─ build.bat                       # 一键 mingw32-make 构建脚本
├─ README.md
├─ CLAUDE.md                       # AI 协作规则与架构约束
├─ codex.md                        # Codex 工程师协作约束
├─ design_spec.md                  # UI 设计规格（唯一权威）
├─ protocol.md                     # 通信协议（唯一权威，当前 v1.9）
├─ docs/
│  ├─ vision.md                    # 项目愿景
│  ├─ tech_overview.md             # 技术栈与目录结构
│  └─ file_index.md                # src 模块索引
├─ resources/
│  ├─ resources.qrc
│  ├─ motordev.ico
│  ├─ app_icon.rc
│  └─ styles/motordev.qss
├─ src/
│  ├─ main.cpp                     # 入口，安装全局 Qt 消息处理器
│  ├─ mainwindow.{h,cpp}           # 主窗口 Shell
│  ├─ devicecontext.{h,cpp}        # 当前 IC 类型 / 从机地址
│  ├─ frameparser.{h,cpp}          # 帧解析状态机 + 帧编码
│  ├─ serialmanager.{h,cpp}        # 串口管理（独立线程、心跳）
│  ├─ models/                      # ScopeChannelModel / ChannelBuffer
│  ├─ protocol/                    # motor_protocol / register_utils / sampling_config
│  ├─ services/                    # CommandDispatcher / ConfigService /
│  │                                CyclicWriteService / GeneratorService /
│  │                                RegisterService / ScopeService /
│  │                                SimulatorService / SimulatorSerial
│  ├─ tabs/                        # configtab / registerrwtab / fwflashtab /
│  │                                oscilloscoptab / serialdebugtab
│  ├─ ui/                          # style_constants.h / repolish.h / scopeviewmode.h
│  └─ widgets/                     # activitybar / topbar / sidebar / logpanel /
│                                    registertable / scope* 系列
├─ build_make_qt/                  # 默认构建输出目录（含 MotorDev.exe）
├─ TRACKING/                       # 用户可读问题追踪笔记
└─ tmp_handoff/                    # 阶段计划 / Review / 实现回执交接
```

## Requirements

建议使用与 Qt 配套的工具链版本：

- Qt `6.11.0` `mingw_64`
- Qt Tools `mingw1310_64`（MinGW 13.1.0 64-bit）
- Qt Tools `CMake_64`

核心要求不是路径必须一致，而是下面三件事必须成立：

- Qt 版本使用 `6.11.0`
- 编译器使用与该 Qt 套件配套的 `MinGW 13.1.0 64-bit`
- `cmake.exe`、`g++.exe`、`mingw32-make.exe` 与 `Qt6_DIR` 必须指向同一套 Qt/MinGW 安装

仓库默认验证路径：

```text
D:\Qt\6.11.0\mingw_64
D:\Qt\Tools\mingw1310_64
D:\Qt\Tools\CMake_64
```

## Build

### Quick Build（首次成功配置后）

`build.bat` 是一键增量构建脚本，前提是 `build_make_qt/` 已通过下面 "First-time CMake Configure" 步骤完成 cmake 配置：

```cmd
build.bat
```

脚本默认 `MINGW_PATH=D:\Qt\Tools\mingw1310_64\bin`。换机器时改这一行即可。

### First-time CMake Configure

PowerShell 中执行（替换为本机实际路径）：

```powershell
$qtRoot    = "D:\Qt\6.11.0\mingw_64"
$mingwRoot = "D:\Qt\Tools\mingw1310_64"
$cmakeRoot = "D:\Qt\Tools\CMake_64"

& "$cmakeRoot\bin\cmake.exe" `
  -S . `
  -B build_make_qt `
  -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH="$qtRoot" `
  -DQt6_DIR="$qtRoot\lib\cmake\Qt6" `
  -DCMAKE_CXX_COMPILER="$mingwRoot\bin\g++.exe" `
  -DCMAKE_MAKE_PROGRAM="$mingwRoot\bin\mingw32-make.exe"

& "$mingwRoot\bin\mingw32-make.exe" -C build_make_qt -j8
```

构建产物：

```text
build_make_qt\MotorDev.exe
```

### Why MinGW Makefiles, Not Ninja

仓库最初尝试过 Ninja，但在当前项目环境里更容易遇到：

- `Detecting CXX compiler ABI info` 卡住
- 脏构建目录残留 `.ninja_lock`
- 难以区分是生成器问题还是 Qt 配置问题

因此默认推荐 `MinGW Makefiles`。如果你已经在自己机器上稳定验证过 Ninja，可自行切换。

### Common Problems

1. **`Qt6` 找不到** —— 检查 `-DCMAKE_PREFIX_PATH` 与 `-DQt6_DIR`，确认两者都指向同一套 `6.11.0\mingw_64`。
2. **`g++` 不是 Qt 配套版本** —— 不要依赖裸 `g++`，直接传绝对路径 `D:\Qt\Tools\mingw1310_64\bin\g++.exe`。
3. **`MotorDev.exe` 链接失败、提示文件被占用** —— 先关闭正在运行的实例，再重新 `mingw32-make` 或 `build.bat`。
4. **`AutoMoc` 或子进程创建失败** —— 受限终端 / 沙箱环境的常见问题，换到正常本地终端重试。

## Deploy

打包 Qt 运行时：

```powershell
& "D:\Qt\6.11.0\mingw_64\bin\windeployqt.exe" .\build_make_qt\MotorDev.exe
```

## Documentation

| 文件 | 用途 |
|------|------|
| `docs/vision.md` | 项目愿景与设计原则 |
| `docs/tech_overview.md` | 技术栈、目录结构、开发优先级 |
| `docs/file_index.md` | `src/` 模块功能索引 |
| `protocol.md` | 串口通信协议（**唯一权威**） |
| `design_spec.md` | UI 设计与交互规格（**唯一权威**） |
| `CLAUDE.md` | Claude Code 协作规则与架构约束 |
| `codex.md` | Codex 工程师实现侧约束 |
| `tmp_handoff/` | 阶段计划 / Review / 实现回执 |
| `TRACKING/` | 用户可读问题追踪笔记（HTML） |

## Current Status

已完成并联调通过：

- Config / RegisterRW / Oscilloscope 三个 Tab 的核心交互
- 串口扫描 / 连接 / 断开 / 心跳检测 / 重连
- I2C 扫描、IC 类型与从机地址选择
- PMIC 电压配置（DRVDD / VCMVDD / IOVDD）与 LDO 使能
- 寄存器单读 / 单写 / 批量读 + 自动持久化
- 示波器 8 通道 60 fps 渲染、视图切换、拖拽缩放、十字光标读值
- 信号发生器 Linear / Cosine / Sawtooth（参数下发，由 STM32 执行波形）
- 周期写、命令分发串行化、流帧 backpressure
- 协议 CRC16 与固件对齐，采样间隔表对齐协议 v1.9
- 串口调试模拟器（端到端验证不依赖真实硬件）

仍为占位 / stub：

- FwFlash 固件烧录（整体未实现）
- ConfigTab 的 Config File 行（文件选 / Browse / Write / Read）
- RegisterRW 底部"批量读写"用户文件选择
- TopBar 多语言切换 combo
- ActivityBar 设置按钮

## License

Apache-2.0. See `LICENSE`.
