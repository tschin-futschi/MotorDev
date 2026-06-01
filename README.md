# MotorDev

MotorDev 是一款面向电机驱动模组调试的 Qt 桌面上位机，将**寄存器读写、固件烧录、实时波形观测**整合到同一应用中，为电机调试提供一站式工具体验。

通信链路：上位机（Windows） ⇄ 串口（UART，默认 460800） ⇄ STM32 ⇄ I2C ⇄ 电机驱动 IC。

> 完整愿景见 [`docs/vision.md`](docs/vision.md)；通信协议权威定义见 [`protocol.md`](protocol.md)；UI 规格权威定义见 [`design_spec.md`](design_spec.md)。

---

## Features

六个功能页（含一个浮动调试模拟器），核心链路均已落地：

| 页面 | 能力 |
|------|------|
| **Config** | 串口扫描 / 连接 / 断开、心跳保活；I2C 总线扫描、IC 型号与从机地址（SlaveID）选择；PMIC 三路电压（DRVDD / VCMVDD / IOVDD）配置与使能 / 禁用；设备复位 / 电机测试；底部「统一配置文件」行：把各页功能参数采集为单个 JSON 手动读写（Browse / Write / Read） |
| **RegisterRW** | 寄存器表（4 组 × 30 行）单读 / 单写 / 全部读写、Hex/Dec 切换、页面清除；**批量读写浮窗**（4 槽独立，配置文件 ↔ 芯片寄存器往返，`<addr>, <value>` + `//` 注释格式）；**块读取浮窗**（连续地址段 dump → `Bulkread_HHMMSS.csv`，失败即停 / 协作式取消） |
| **FwFlash** | 固件烧录，四款 IC 全部落地：AW86008 / AW86100 走 STM32 本地 ISP（协议 0x32~0x37）+ 0x38 真实进度；DW9786 / DW9788（HL9788N）走 Dongwoon 原厂 vendor SDK + I2C 透传桥接（OTA 模式默认保留校准）；支持 `.bin` / Intel `.hex` / DW 自定义 hex 解析 + CRC32，策略模式 + 心跳暂停/恢复 + 协作式取消 |
| **Oscilloscope** | 8 通道实时示波，OpenGL + QPainter 自绘，稳定 60 fps；overlay / stacked 视图、X/Y 拖拽缩放、双击全屏、十字光标吸附读值；通道样式面板（颜色 / 线宽 / 线型 / 数据点）；内嵌采样启停；信号发生器（Linear / Cosine / Sawtooth，参数下发由 STM32 执行）；寄存器侧面板（单读写 + 周期循环写入）；全速率采样 CSV 记录 |
| **FlashStorage** | STM32 自身 FLASH 文件存储（协议 0x39~0x3F）：把任意本地文件上传到 STM32 内置 Flash（Sector 5-11，896 KB）暂存，另一台 PC 再下载，1:1 无损（改扩展名即复原）；支持容量查询与一键清空（含元数据擦除） |
| **SerialDebug**（浮动） | 串口调试模拟器，不依赖真实硬件即可端到端跑通 I2C 扫描、寄存器读写、PMIC、采样流帧、发生器命令等 |

**支持 IC**：AW86008 / AW86100 / DW9786 / DW9788(HL9788N)。

公共基础设施：

- **串口管理**：独立线程、心跳保活 / 超时重试、烧录链路 fast-path 同步 API（`sendAndWaitResponse`）
- **协议帧编解码**：控制帧 `0xAA55`（CRC16）+ 流帧 `0xBB`（XOR），与固件严格对齐
- **命令分发器**：优先级队列 + seq 匹配，串行化命令、避免响应错配
- **统一配置文件存取**：各页功能参数 → 单个 JSON，全手动 Read/Write（`AppConfigService`）
- **多语言（i18n）**：中 / 英运行时即时切换，记忆上次选择
- **日志面板**：全局单例、跨线程安全、level / category / 文件落盘
- Splash screen、QSS 主题、自定义 logo / icon、关于对话框、DPI 锁屏

---

## Tech Stack

- C++17
- Qt 6 Widgets（Core / Widgets / SerialPort / Svg / SvgWidgets / OpenGLWidgets）
- CMake 3.16+
- MinGW 13.1.0 64-bit
- Dongwoon DW9786 / HL9788N 原厂 vendor SDK（第三方源码，见 `src/services/flashstrategies/vendor/`）

---

## Repository Layout

```text
MotorDev/
├─ CMakeLists.txt
├─ build.bat                       # 一键 mingw32-make 增量构建脚本
├─ README.md
├─ CLAUDE.md                       # AI 协作规则与架构约束
├─ codex.md                        # Codex 工程师协作约束
├─ design_spec.md                  # UI 设计规格（唯一权威）
├─ protocol.md                     # 通信协议（唯一权威）
├─ docs/
│  ├─ vision.md                    # 项目愿景与设计原则
│  ├─ tech_overview.md             # 技术栈、目录结构、开发优先级
│  └─ file_index.md                # src 模块功能索引
├─ resources/                      # qrc / 图标 / QSS 主题
├─ translations/                   # i18n（motordev_en.ts / .qm）
├─ src/
│  ├─ main.cpp                     # 入口，安装全局 Qt 消息处理器
│  ├─ mainwindow.{h,cpp}           # 主窗口 Shell
│  ├─ devicecontext.{h,cpp}        # 当前 IC 类型 / 从机地址
│  ├─ frameparser.{h,cpp}          # 帧解析状态机 + 帧编码 + CRC16
│  ├─ serialmanager.{h,cpp}        # 串口管理（独立线程、心跳、fast-path）
│  ├─ models/                      # ScopeChannelModel / ChannelBuffer
│  ├─ protocol/                    # motor_protocol / register_utils /
│  │                                sampling_config / firmware_parser /
│  │                                batch_register_file
│  ├─ services/                    # AppConfigService / CommandDispatcher /
│  │                                ConfigService / RegisterService /
│  │                                BatchRegisterService / BlockReadService /
│  │                                ScopeService / ScopeRecordService /
│  │                                CyclicWriteService / GeneratorService /
│  │                                FwFlashService / FlashStoreService /
│  │                                Dw9786OisResetService / Simulator* /
│  │                                flashstrategies/（含 vendor/ 原厂 SDK）
│  ├─ tabs/                        # configtab / registerrwtab / fwflashtab /
│  │                                oscilloscoptab / flashstoragetab /
│  │                                serialdebugtab
│  ├─ ui/                          # style_constants.h / repolish.h / scopeviewmode.h
│  └─ widgets/                     # activitybar / topbar / sidebar / logpanel /
│                                    aboutdialog / registertable /
│                                    fwflash* / scope* 系列
├─ build_make_qt/                  # 默认构建输出目录（含 MotorDev.exe）
├─ TRACKING/                       # 用户可读问题追踪笔记（HTML）
└─ tmp_handoff/                    # 阶段计划 / Review / 实现回执交接（本地，未入仓）
```

---

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

---

## Build

### Quick Build（首次成功配置后）

`build.bat` 是一键增量构建脚本，前提是 `build_make_qt/` 已通过下面 "First-time CMake Configure" 完成 cmake 配置：

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

构建产物：`build_make_qt\MotorDev.exe`

### Why MinGW Makefiles, Not Ninja

仓库默认推荐 `MinGW Makefiles`。最初尝试过 Ninja，但在当前项目环境里更容易遇到 `Detecting CXX compiler ABI info` 卡住、脏构建目录残留 `.ninja_lock`、难以区分生成器问题与 Qt 配置问题。若你已在自己机器稳定验证过 Ninja，可自行切换。

### Common Problems

1. **`Qt6` 找不到** —— 检查 `-DCMAKE_PREFIX_PATH` 与 `-DQt6_DIR`，确认都指向同一套 `6.11.0\mingw_64`。
2. **`g++` 不是 Qt 配套版本** —— 不要依赖裸 `g++`，直接传绝对路径 `D:\Qt\Tools\mingw1310_64\bin\g++.exe`。
3. **`MotorDev.exe` 链接失败、提示文件被占用** —— 先关闭正在运行的实例，再重新 `mingw32-make` 或 `build.bat`。
4. **`AutoMoc` 或子进程创建失败** —— 受限终端 / 沙箱环境的常见问题，换到正常本地终端重试。

---

## Deploy

打包 Qt 运行时：

```powershell
& "D:\Qt\6.11.0\mingw_64\bin\windeployqt.exe" .\build_make_qt\MotorDev.exe
```

---

## Documentation

| 文件 | 用途 |
|------|------|
| `docs/vision.md` | 项目愿景与设计原则 |
| `docs/tech_overview.md` | 技术栈、目录结构、开发优先级 |
| `docs/file_index.md` | `src/` 模块功能索引 |
| `protocol.md` | 串口通信协议（**唯一权威**，generator_v1 / v2.11） |
| `design_spec.md` | UI 设计与交互规格（**唯一权威**） |
| `CLAUDE.md` | Claude Code 协作规则与架构约束 |
| `codex.md` | Codex 工程师实现侧约束 |
| `tmp_handoff/` | 阶段计划 / Review / 实现回执（本地，未入仓） |
| `TRACKING/` | 用户可读问题追踪笔记（HTML） |

---

## Current Status

v1.0 核心场景与配套能力均已落地并联调通过：

- 串口扫描 / 连接 / 断开 / 心跳 / 重连；I2C 扫描、IC 型号与从机地址选择
- PMIC 三路电压配置（DRVDD / VCMVDD / IOVDD）与使能 / 禁用
- 寄存器单读 / 单写 / 全部读写 + 批量读写浮窗 + 块读取浮窗（CSV）
- 固件烧录：AW86008 / AW86100（本地 ISP + 0x38 真实进度）、DW9786 / DW9788（vendor SDK + I2C 透传）
- 示波器 8 通道 60 fps、视图切换、拖拽缩放、十字光标读值、信号发生器、寄存器侧面板（周期写）、CSV 记录
- STM32 FLASH 文件存储（上传 / 下载 / 容量 / 清空）
- 统一配置文件读写、多语言中英运行时切换、关于对话框
- 协议帧 CRC16 与固件对齐、采样间隔表与协议对齐、命令分发串行化、流帧 backpressure
- 串口调试模拟器（端到端验证不依赖真实硬件）

进行中：

- 打包发布（windeployqt 流程完善）

---

## License

项目代码采用 **Apache-2.0**，见 [`LICENSE`](LICENSE)。

> ⚠️ **第三方代码例外**：`src/services/flashstrategies/vendor/`（Dongwoon DW9786 / HL9788N 原厂 SDK）为第三方厂商源码，受其各自授权条款约束，**不在本项目 Apache-2.0 范围内**。对外分发 / 开源 / 公开仓库前，请先确认这些 vendor 代码的授权是否允许。
