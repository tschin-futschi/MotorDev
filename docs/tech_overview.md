# MotorDev — 技术概览

> 版本：v0.3 | 日期：2026-05-06

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
- CMake 3.16+，启用 CMAKE_AUTOMOC / CMAKE_AUTORCC（AUTOUIC 保留但项目已无 .ui 文件）

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
│   ├── main.cpp                     # 应用入口（QApplication、启动画面、全局日志路由）
│   ├── mainwindow.cpp / .h          # 主窗口，组合所有顶层组件，管理连接状态与页面联动
│   ├── serialmanager.cpp / .h       # 串口管理，独立线程，含心跳与超时重试
│   ├── frameparser.cpp / .h         # 二进制帧解析状态机 + 控制帧编码 + CRC16
│   ├── devicecontext.cpp / .h       # 当前 IC 类型和从机地址持久状态
│   ├── protocol/
│   │   ├── motor_protocol.cpp / .h  # 命令字节常量 + 请求载荷编码 + 响应载荷解码
│   │   ├── register_utils.cpp / .h  # 寄存器地址/值文本解析（支持 0x 前缀）
│   │   └── sampling_config.cpp / .h # 采样间隔/显示窗口 UI 文本 ↔ 协议索引映射
│   ├── models/
│   │   ├── scopechannelmodel.cpp / .h # 示波器 8 通道配置数据模型（启用/描述/地址/样式）
│   │   └── channelbuffer.cpp / .h     # 单通道双层环形缓冲（原始环 + 降采样后 UI 环）
│   ├── services/
│   │   ├── commanddispatcher.cpp / .h # 命令分发器（优先级队列 + seq 匹配 + 错误回退）
│   │   ├── configservice.cpp / .h     # 串口连接、I2C 扫描、IC 地址、PMIC 配置/复位
│   │   ├── registerservice.cpp / .h   # 单行/批量寄存器读写 + 500ms 超时
│   │   ├── scopeservice.cpp / .h      # 4 步采样启动序列 + 流帧批量 + 5s 看门狗
│   │   ├── cyclicwriteservice.cpp / .h # 循环写入服务（轮询 + 连续错误自停）
│   │   ├── generatorservice.cpp / .h  # 波形发生器（Linear/Cosine/Sawtooth + 3s ACK 超时）
│   │   ├── ftdi_latency_helper.cpp / .h # FT232 D2XX Latency Timer 强设辅助（仅由 SerialManager::openPort 调用，best-effort）
│   │   ├── simulatorservice.cpp / .h  # 调试模拟器命令分发与正弦波形生成（独立 std::thread）
│   │   └── simulatorserial.cpp / .h   # 模拟器串口驱动（独立线程，供 SimulatorService 使用）
│   ├── ui/
│   │   ├── repolish.h               # QSS 重新 polish 工具函数
│   │   ├── style_constants.h        # 所有颜色、尺寸、间距常量
│   │   └── scopeviewmode.h          # ScopeViewMode 枚举（Overlay/Stacked，跨 Tab/Widget 共用）
│   ├── tabs/
│   │   ├── configtab.cpp / .h       # Tab0 配置（串口/IC/PMIC，三卡片布局）
│   │   ├── registerrwtab.cpp / .h   # Tab1 寄存器读写（含 Hex/Dec 切换 + 配置持久化）
│   │   ├── fwflashtab.cpp / .h      # Tab2 FW 烧录（占位，待实现）
│   │   ├── oscilloscoptab.cpp / .h  # Tab3 示波器（8ch 60fps，全屏、十字光标、寄存器辅助、发生器）
│   │   └── serialdebugtab.cpp / .h  # 串口调试模拟器（独立窗口，模拟 STM32 全栈响应）
│   └── widgets/
│       ├── activitybar.cpp / .h     # 左侧活动栏（页面切换）
│       ├── topbar.cpp / .h          # 顶栏（Logo、连接状态、Overlay/Style/Crosshair；语言切换占位）
│       ├── sidebar.cpp / .h         # 可收缩侧边栏（四个 Tab 共用）
│       ├── logpanel.cpp / .h        # 底部日志面板（全局单例，跨线程安全）
│       ├── registertable.cpp / .h   # 寄存器表格组件（4 组×20 行）
│       ├── scopeplotwidget.cpp / .h # 示波器波形绘制画布（QOpenGLWidget + QPainter，含内嵌采样按钮）
│       ├── scopepreviewwidget.cpp / .h # 示波器独立预览控件（自带正弦波数据源，UI 演示用）
│       ├── scopestylepanel.cpp / .h    # 示波器通道样式面板（颜色/线宽/线型/数据点）
│       ├── scopebottompanel.cpp / .h   # 示波器底部面板容器（通道条 + 采样配置 + 浮窗切换）
│       ├── scopechannelstrip.cpp / .h  # 示波器单通道配置条（启用/描述/地址）
│       ├── scoperegisterpanel.cpp / .h # 示波器侧寄存器读写面板（8 行 + 循环写入控制）
│       ├── scopegeneratorpanel.cpp / .h # 信号发生器配置面板（Linear/Cosine/Sawtooth）
│       └── scopemarqueelabel.cpp / .h   # 跑马灯状态标签（文字溢出时自动滚动）
└── tmp_handoff/                     # agent 协作临时文件
```

### 架构层划分

| 层 | 职责 | 主要文件 |
|----|------|---------|
| 应用入口 | QApplication 启动、日志安装、主窗口创建 | `main.cpp` |
| UI Shell | 顶层窗口、页面切换、日志面板 | `mainwindow`、`widgets/{topbar,activitybar,logpanel,sidebar}` |
| UI Tab | 业务页面 | `tabs/{configtab,registerrwtab,fwflashtab,oscilloscoptab,serialdebugtab}` |
| UI Widget | 复用控件 | `widgets/{registertable,scopeplotwidget,scopebottompanel,scopechannelstrip,scoperegisterpanel,scopegeneratorpanel,scopestylepanel,scopepreviewwidget,scopemarqueelabel}` |
| 服务层 | 业务逻辑封装，UI/通信解耦 | `services/{commanddispatcher,configservice,registerservice,scopeservice,cyclicwriteservice,generatorservice,simulatorservice}` |
| 通信辅助 | FT232 芯片端 Latency Timer 强设（D2XX） | `services/ftdi_latency_helper` |
| 数据模型层 | 状态与缓冲 | `models/{scopechannelmodel,channelbuffer}`、`devicecontext` |
| 协议层 | 指令编解码与采样参数映射 | `protocol/{motor_protocol,register_utils,sampling_config}` |
| 通信层 | 串口管理 + 跨线程消息 | `serialmanager` |
| 传输层 | 帧解析与编码 | `frameparser` |
| 开发工具传输层 | 模拟器串口 | `services/simulatorserial` |
| 共享枚举/样式 | 跨模块常量 | `ui/{scopeviewmode.h,style_constants.h,repolish.h}` |

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
