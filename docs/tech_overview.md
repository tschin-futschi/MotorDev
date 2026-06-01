# MotorDev — 技术概览

> 版本：v0.5 | 日期：2026-06-01

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
│   ├── serialmanager.cpp / .h       # 串口管理，独立线程，含心跳与超时重试；fast-path 同步 API `sendAndWaitResponse`
│   ├── frameparser.cpp / .h         # 二进制帧解析状态机 + 控制帧编码 + CRC16
│   ├── devicecontext.cpp / .h       # 当前 IC 类型和从机地址持久状态
│   ├── protocol/
│   │   ├── motor_protocol.cpp / .h  # 命令字节常量 + 请求载荷编码 + 响应载荷解码（含 AW ISP 0x32~0x38、FLASH 文件存储 0x39~0x3F）
│   │   ├── register_utils.cpp / .h  # 寄存器地址/值文本解析（支持 0x 前缀）
│   │   ├── sampling_config.cpp / .h # 采样间隔/显示窗口 UI 文本 ↔ 协议索引映射
│   │   ├── firmware_parser.cpp / .h # 固件解析：`.bin` 直读 + Intel `.hex` 合并 + HL9788N 自定义 hex + CRC32（IEEE 802.3）
│   │   └── batch_register_file.cpp / .h # 批量读写配置文件解析与回写：`<addr>, <value>` + `//` 注释；解析严格全或无；回写保留原结构仅替换值列
│   ├── models/
│   │   ├── scopechannelmodel.cpp / .h # 示波器 8 通道配置数据模型（启用/描述/地址/样式）
│   │   └── channelbuffer.cpp / .h     # 单通道双层环形缓冲（原始环 + 降采样后 UI 环）
│   ├── services/
│   │   ├── commanddispatcher.cpp / .h # 命令分发器（优先级队列 + seq 匹配 + 错误回退）
│   │   ├── appconfigservice.cpp / .h  # 统一配置文件存取（各页面功能参数 → 单个 JSON，全手动 Read/Write）
│   │   ├── configservice.cpp / .h     # 串口连接、I2C 扫描、IC 地址、PMIC 配置/复位
│   │   ├── registerservice.cpp / .h   # 单行/批量寄存器读写 + 500ms 超时
│   │   ├── batchregisterservice.cpp / .h # 批量读写浮窗业务层：文件解析 + 状态机 + 顺序读写 + 进度信号
│   │   ├── blockreadservice.cpp / .h    # 块读取浮窗业务层：连续地址段 dump → CSV + 协作式取消 + 失败即停（已成功条目仍写文件）
│   │   ├── scopeservice.cpp / .h      # 4 步采样启动序列 + 流帧批量 + 5s 看门狗
│   │   ├── scoperecordservice.cpp / .h # 示波器采样全速率原始数据持续写 CSV（采样启停即录制启停）
│   │   ├── cyclicwriteservice.cpp / .h # 循环写入服务（轮询 + 连续错误自停）
│   │   ├── generatorservice.cpp / .h  # 波形发生器（Linear/Cosine/Sawtooth + 3s ACK 超时）
│   │   ├── simulatorservice.cpp / .h  # 调试模拟器命令分发与正弦波形生成（独立 std::thread）
│   │   ├── simulatorserial.cpp / .h   # 模拟器串口驱动（独立线程，供 SimulatorService 使用）
│   │   ├── fwflashservice.cpp / .h    # 固件烧录服务：状态机 + 前置序列协调 + fast-path worker + 心跳暂停/恢复 + 0x38 进度帧消费
│   │   ├── flashstoreservice.cpp / .h # STM32 FLASH 文件存储服务（上传/下载/容量查询/WIPE）
│   │   ├── dw9786oisresetservice.cpp / .h # DW9786 上电 OISReset 9 步序列（连接成功且 Select IC=DW9786 时触发）
│   │   ├── flashstrategy.cpp / .h     # 烧录策略抽象基类
│   │   ├── flashstrategyregistry.cpp / .h # 烧录策略注册中心（按 IC 型号枚举/查找）
│   │   └── flashstrategies/
│   │       ├── aw_local_isp_strategy.cpp / .h # AW 本地 ISP 策略基类（0x32~0x37 序列）
│   │       ├── aw86008_strategy.cpp / .h      # AW86008 策略（继承 AW base）
│   │       ├── aw86100_strategy.cpp / .h      # AW86100 策略（继承 AW base）
│   │       ├── dw9786_strategy.cpp / .h       # DW9786 真实策略：vendor SDK + I2C 透传
│   │       ├── dw9786_bridge.cpp / .h         # DW9786 vendor 库与 SerialManager 的桥接层
│   │       ├── dw9786_vendor_include.h        # DW9786 vendor 包装头
│   │       ├── dw9788_strategy.cpp / .h       # DW9788 (HL9788N) 真实策略：vendor SDK + I2C 透传
│   │       ├── hl9788n_bridge.cpp / .h        # HL9788N vendor 库与 SerialManager 的桥接层
│   │       ├── hl9788n_vendor_include.h       # vendor 包装头（先展开 Qt/std 头再 #undef 污染宏）
│   │       └── vendor/{hl9788n,dw9786}/       # Dongwoon 原厂 vendor 源码（Func、api_ref、stdafx；dw9786 另含 symbol_rename.h 隔离同名符号）
│   ├── ui/
│   │   ├── repolish.h               # QSS 重新 polish 工具函数
│   │   ├── style_constants.h        # 所有颜色、尺寸、间距常量
│   │   └── scopeviewmode.h          # ScopeViewMode 枚举（Overlay/Stacked，跨 Tab/Widget 共用）
│   ├── tabs/
│   │   ├── configtab.cpp / .h       # Tab0 配置（串口/IC/PMIC，三卡片布局）
│   │   ├── registerrwtab.cpp / .h   # Tab1 寄存器读写（含 Hex/Dec 切换 + 配置持久化）
│   │   ├── fwflashtab.cpp / .h      # Tab2 FW 烧录（IC 选择 + 文件加载 + 控制 + 日志，QSplitter 两栏）
│   │   ├── oscilloscoptab.cpp / .h  # Tab3 示波器（8ch 60fps，全屏、十字光标、寄存器辅助、发生器）
│   │   ├── flashstoragetab.cpp / .h # Tab4 STM32 FLASH 文件存储（上传/下载/容量/WIPE）
│   │   └── serialdebugtab.cpp / .h  # 串口调试模拟器（独立窗口，模拟 STM32 全栈响应）
│   └── widgets/
│       ├── activitybar.cpp / .h     # 左侧活动栏（六个页面 + 调试浮窗按钮 + 底部「关于」按钮）
│       ├── aboutdialog.cpp / .h     # 关于对话框（模态：元信息 + 德语题词 + Logo 全幅背景）
│       ├── topbar.cpp / .h          # 顶栏（Logo、连接状态、MCU 启动状态徽章、Overlay/Style/Crosshair、语言切换 combo 运行时中英即时切换）
│       ├── sidebar.cpp / .h         # 可收缩侧边栏（configtab/registerrwtab/oscilloscoptab 共用）
│       ├── logpanel.cpp / .h        # 底部日志面板（全局单例，跨线程安全）
│       ├── registertable.cpp / .h   # 寄存器表格组件（4 组×30 行）
│       ├── scopeplotwidget.cpp / .h # 示波器波形绘制画布（QOpenGLWidget + QPainter，含内嵌采样按钮）
│       ├── scopepreviewwidget.cpp / .h # 示波器独立预览控件（自带正弦波数据源，UI 演示用）
│       ├── scopestylepanel.cpp / .h    # 示波器通道样式面板（颜色/线宽/线型/数据点）
│       ├── scopebottompanel.cpp / .h   # 示波器底部面板容器（通道条 + 采样配置 + 浮窗切换）
│       ├── scopechannelstrip.cpp / .h  # 示波器单通道配置条（启用/描述/地址）
│       ├── scoperegisterpanel.cpp / .h # 示波器侧寄存器读写面板（8 行 + 循环写入控制）
│       ├── scopegeneratorpanel.cpp / .h # 信号发生器配置面板（Linear/Cosine/Sawtooth）
│       ├── scopemarqueelabel.cpp / .h   # 跑马灯状态标签（文字溢出时自动滚动）
│       ├── fwfileinfopanel.cpp / .h     # 固件文件信息面板（QStackedWidget 空/合法/错误三页）
│       ├── fwflashcontrolpanel.cpp / .h # 烧录控制面板（开始/取消按钮 + 阶段标签 + 进度条）
│       └── fwflashlogpanel.cpp / .h     # 烧录操作日志面板（4 级颜色、时间戳、底部追随滚动）
└── tmp_handoff/                     # agent 协作临时文件
```

### 架构层划分

| 层 | 职责 | 主要文件 |
|----|------|---------|
| 应用入口 | QApplication 启动、日志安装、主窗口创建 | `main.cpp` |
| UI Shell | 顶层窗口、页面切换、日志面板 | `mainwindow`、`widgets/{topbar,activitybar,logpanel,sidebar}` |
| UI Tab | 业务页面 | `tabs/{configtab,registerrwtab,fwflashtab,oscilloscoptab,flashstoragetab,serialdebugtab}` |
| UI Widget | 复用控件 | `widgets/{registertable,scopeplotwidget,scopebottompanel,scopechannelstrip,scoperegisterpanel,scopegeneratorpanel,scopestylepanel,scopepreviewwidget,scopemarqueelabel,fwfileinfopanel,fwflashcontrolpanel,fwflashlogpanel,aboutdialog}` |
| 服务层 | 业务逻辑封装，UI/通信解耦 | `services/{appconfigservice,commanddispatcher,configservice,registerservice,batchregisterservice,blockreadservice,scopeservice,scoperecordservice,cyclicwriteservice,generatorservice,simulatorservice,fwflashservice,flashstoreservice,dw9786oisresetservice}` |
| 烧录策略层 | 按 IC 型号封装烧录算法（策略模式） | `services/{flashstrategy,flashstrategyregistry}`、`services/flashstrategies/{aw_local_isp_strategy,aw86008_strategy,aw86100_strategy,dw9786_strategy,dw9786_bridge,dw9788_strategy,hl9788n_bridge}` |
| 第三方 vendor | Dongwoon HL9788N / DW9786 原厂 SDK 源码（最小改动适配 MotorDev 工程；dw9786 用 symbol_rename.h 隔离同名符号） | `services/flashstrategies/vendor/{hl9788n,dw9786}/{Func,*_api_ref,stdafx.h}` |
| 数据模型层 | 状态与缓冲 | `models/{scopechannelmodel,channelbuffer}`、`devicecontext` |
| 协议层 | 指令编解码、采样参数映射、固件文件解析、批量读写配置文件 | `protocol/{motor_protocol,register_utils,sampling_config,firmware_parser,batch_register_file}` |
| 通信层 | 串口管理 + 跨线程消息 + fast-path 同步 API | `serialmanager` |
| 传输层 | 帧解析与编码 | `frameparser` |
| 开发工具传输层 | 模拟器串口 | `services/simulatorserial` |
| 共享枚举/样式 | 跨模块常量 | `ui/{scopeviewmode.h,style_constants.h,repolish.h}` |

---

## 开发优先级

> 状态截至 2026-06-01。"✓"=已落地，"⌛"=部分实现 / stub，"○"=未实现。

1. CMakeLists.txt 基础构建配置 ✓
2. 主窗口框架（TopBar + ActivityBar + Sidebar + ContentArea + StatusBar）✓
3. SerialManager + FrameParser（核心通信）✓
4. Tab 0 配置（串口/IC/PMIC）✓
5. Tab 1 寄存器读写（RegisterTable + Hex/Dec + 批量读写/块读取；参数经统一配置文件手动存取）✓
6. Tab 2 FW 烧录 ✓
   - AW86008 / AW86100：STM32 本地 ISP（协议 0x32~0x37）+ 0x38 真实进度
   - DW9788 (HL9788N)：vendor SDK + I2C 透传桥接（OTA 模式默认保留校准）
   - DW9786：vendor SDK + I2C 透传桥接（id_check + fw_download_with_buffer + chip_enable，OTA 模式默认保留校准）✓
7. Tab 3 示波器（QPainter + QOpenGLWidget 自研渲染，8ch 60fps）✓
8. Tab 4 STM32 FLASH 文件存储（v2.11 / 0x39~0x3F）✓
9. 串口调试模拟器（SerialDebugTab，独立浮窗）✓
10. 信号发生器（Linear / Cosine / Sawtooth）✓
11. 统一配置文件存取（各页参数手动 Read/Write 单个 JSON；原 registers.json 自动持久化已移除）✓
12. 启动画面 + 应用图标 + DPI 锁屏 ✓
13. MCU 启动状态徽章（0x0B BOOT_STATUS）✓
14. 多语言支持（i18n 运行时中英即时切换，TopBar combo 驱动 + QSettings 记忆）✓
15. 关于对话框（侧边栏底部「关于」按钮，元信息 + 德语题词 + Logo 背景；原「设置」占位按钮已移除）✓
16. 打包发布（windeployqt）⌛

---

## 注意事项

- 所有 UI 组件必须在主线程操作，串口数据通过信号槽跨线程传递
- Qt6 中 `QSerialPort` 在子线程使用时需注意线程亲和性
- 表格缩放建议用 `QGraphicsProxyWidget` 或自定义 delegate 实现，避免直接使用 CSS transform
- 多语言字符串统一用 `tr()` 包裹
- 所有硬编码颜色提取为常量，方便后续主题切换
