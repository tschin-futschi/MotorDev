# MotorDev — 技术概览

> 版本：v0.1 | 日期：2026-03-31

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
- Qt6::PrintSupport（用于 QCustomPlot）
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
│   └── tech_overview.md             # 技术概览（本文件）
├── resources/
│   ├── motordev_logo.svg
│   ├── icons/
│   └── resources.qrc
├── src/
│   ├── main.cpp
│   ├── mainwindow.cpp / .h
│   ├── serialmanager.cpp / .h       # 串口管理，独立线程
│   ├── frameparser.cpp / .h         # 二进制帧解析状态机
│   ├── datalogger.cpp / .h          # 日志与 CSV 导出
│   ├── tabs/
│   │   ├── registerrwtab.cpp / .h   # Tab1 寄存器读写
│   │   ├── fwflashtab.cpp / .h      # Tab2 FW 烧录
│   │   └── oscilloscoptab.cpp / .h  # Tab3 示波器
│   ├── widgets/
│   │   ├── registertable.cpp / .h   # 寄存器表格组件
│   │   ├── activitybar.cpp / .h     # 左侧活动栏
│   │   └── sidebar.cpp / .h         # 可收缩侧边栏
│   └── qcustomplot/
│       ├── qcustomplot.cpp
│       └── qcustomplot.h
├── include/
├── tmp_handoff/                     # agent 协作临时文件
└── translations/
    ├── motordev_zh.ts
    ├── motordev_en.ts
    ├── motordev_ru.ts
    └── motordev_ja.ts
```

---

## 开发优先级

1. CMakeLists.txt 基础构建配置
2. 主窗口框架（TopBar + ActivityBar + Sidebar + ContentArea + StatusBar）
3. SerialManager + FrameParser（核心通信）
4. Tab 1 寄存器读写（RegisterTable）
5. Tab 2 FW 烧录
6. Tab 3 示波器（QCustomPlot 集成）
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
