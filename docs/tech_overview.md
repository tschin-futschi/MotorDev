# MotorDev — 技术概览

> 版本：v0.2 | 日期：2026-04-06

---

## 项目概览

| 项目 | 内容 |
|------|------|
| 软件名称 | MotorDev |
| 用途 | 电机驱动模组调试专用上位机工具 |
| 开发语言 | C++17 |
| UI 框架 | Qt6 Widgets |
| 构建系统 | CMake 3.16+ |
| 目标平台 | Windows 10/11 x64 |
| 目标分辨率 | 1280×800 基准，自适应至 1920×1080 |

---

## 技术栈

- Qt6::Core
- Qt6::Widgets
- Qt6::SerialPort
- Qt6::Svg
- Qt6::SvgWidgets
- Qt6::OpenGLWidgets（用于 ScopePlotWidget 的 QOpenGLWidget + QPainter 渲染）
- CMake 3.16+，启用 CMAKE_AUTOMOC / CMAKE_AUTORCC / CMAKE_AUTOUIC

---

## 项目目录结构

```
MotorDev/
├── CMakeLists.txt
├── CLAUDE.md                        # 协作规则、架构约束、开发流程
├── protocol.md                      # 通信协议（唯一权威）
├── design_spec.md                   # UI 设计规格（唯一权威）
├── docs/
│   ├── vision.md                    # 项目愿景
│   ├── tech_overview.md             # 技术概览（本文件）
│   └── file_index.md               # src 模块索引（AI 辅助定位）
├── resources/
│   ├── motordev_logo.svg
│   ├── icons/
│   └── resources.qrc
├── src/
│   ├── main.cpp
│   ├── mainwindow.cpp / .h
│   ├── serialmanager.cpp / .h       # 串口管理，独立线程
│   ├── frameparser.cpp / .h         # 二进制帧解析状态机
│   ├── devicecontext.cpp / .h       # 当前 IC 类型和从机地址持久状态
│   ├── protocol/
│   │   └── motor_protocol.cpp / .h  # 寄存器读写/I2C 扫描等指令编解码
│   ├── ui/
│   │   └── style_constants.h        # 所有颜色、尺寸、间距常量
│   ├── tabs/
│   │   ├── configtab.cpp / .h       # Tab0 配置（串口/IC/PMIC）
│   │   ├── registerrwtab.cpp / .h   # Tab1 寄存器读写
│   │   ├── fwflashtab.cpp / .h      # Tab2 FW 烧录（占位）
│   │   ├── oscilloscoptab.cpp / .h  # Tab3 示波器（数据流已接入，8ch 60fps 渲染）
│   │   └── serialdebugtab.cpp / .h  # 串口调试模拟器（浮动窗口，开发工具）
│   ├── services/
│   │   └── simulatorserial.cpp / .h # 模拟器串口驱动（独立线程，供 SerialDebugTab 使用）
│   └── widgets/
│       ├── activitybar.cpp / .h     # 左侧活动栏（页面切换）
│       ├── topbar.cpp / .h          # 顶栏（Logo、连接状态、语言切换占位）
│       ├── sidebar.cpp / .h         # 可收缩侧边栏（四个 Tab 共用）
│       ├── logpanel.cpp / .h        # 底部日志面板（全局单例）
│       ├── registertable.cpp / .h   # 寄存器表格组件（4组×20行）
│       ├── scopetoolbar.cpp / .h    # 示波器工具栏
│       ├── scopeplotwidget.cpp / .h # 示波器波形绘制画布（自绘制）
│       ├── scopebottompanel.cpp / .h  # 示波器底部面板容器
│       ├── scopechannelstrip.cpp / .h # 示波器单通道配置条
│       ├── scoperegisterpanel.cpp / .h # 示波器侧寄存器读写面板
│       └── scopegeneratorpanel.cpp / .h # 信号发生器配置面板（占位）
└── tmp_handoff/                     # agent 协作临时文件
```

---

## 开发优先级

1. CMakeLists.txt 基础构建配置
2. 主窗口框架（TopBar + ActivityBar + Sidebar + ContentArea + StatusBar）
3. SerialManager + FrameParser（核心通信）
4. Tab 1 寄存器读写（RegisterTable）
5. Tab 2 FW 烧录
6. Tab 3 示波器（QPainter + QOpenGLWidget 自研渲染）
7. 多语言支持
8. 配置持久化
9. 打包发布（windeployqt）

---

## 注意事项

- 所有 UI 组件必须在主线程操作，串口数据通过信号槽跨线程传递
- Qt6 中 `QSerialPort` 在子线程使用时需注意线程亲和性
- 表格缩放建议用 `QGraphicsProxyWidget` 或自定义 delegate 实现，避免直接使用 CSS transform
- 多语言字符串统一用 `tr()` 包裹
- 所有硬编码颜色提取为常量，方便后续主题切换
