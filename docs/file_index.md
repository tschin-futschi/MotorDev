# src 模块索引

> 面向 AI 辅助开发的 `src` 模块快速定位索引。按功能场景找目标文件，避免无差别扫描源码目录。
> 粒度：文件级（`.h/.cpp` 成对，条目省略后缀）。
> 架构层标签：`[传输]` `[通信]` `[协议]` `[数据模型]` `[UI-Shell]` `[UI-Tab]` `[UI-Widget]` `[样式]`

---

## 功能场景索引

### 串口连接与断开
- `src/tabs/configtab` `[UI-Tab]` — 串口扫描/连接/断开操作入口（仅控件，业务下沉到 ConfigService）
- `src/services/configservice` `[通信]` — 串口连接/断开业务逻辑、心跳启停、状态转发
- `src/widgets/topbar` `[UI-Shell]` — 顶栏连接状态显示（指示灯、端口名）
- `src/serialmanager` `[通信]` — 串口打开/关闭、重试、心跳管理
- `src/frameparser` `[传输]` — 字节流解析为控制帧/流帧，并负责帧编码

### IC 扫描与连接
- `src/tabs/configtab` `[UI-Tab]` — I2C 扫描触发、IC 类型/从机地址选择（仅控件）
- `src/services/configservice` `[通信]` — I2C 扫描命令下发、响应解析、IC 地址写入
- `src/services/commanddispatcher` `[通信]` — 命令优先级队列与 seq 匹配
- `src/devicecontext` `[数据模型]` — 当前 IC 类型和从机地址的持久状态
- `src/protocol/motor_protocol` `[协议]` — I2C 扫描指令编码、响应解码

### PMIC 配置
- `src/tabs/configtab` `[UI-Tab]` — PMIC 电压配置 UI（DRVDD/VCMVDD/IOVDD 输入 + 配置 / 关闭按钮）
- `src/services/configservice` `[通信]` — PMIC 两步序列（SetVoltage → Enable）+ 5s 整体超时 + 关闭命令

### IC 配置文件写入/读出
- `src/tabs/configtab` `[UI-Tab]` — Config File 行（文件选择 combo、Browse/Write/Read 按钮；**当前为 UI stub，按钮未连接信号**）

### 寄存器读写
- `src/tabs/registerrwtab` `[UI-Tab]` — 表格事件转发、Hex/Dec 切换、批量按钮（仅控件）
- `src/services/registerservice` `[通信]` — 单行/批量读写队列、500ms 超时、sessionId 防过期响应
- `src/widgets/registertable` `[UI-Widget]` — 寄存器表格显示与编辑（4 组 × 20 行）
- `src/protocol/motor_protocol` `[协议]` — 读写寄存器指令编码/响应解码
- `src/protocol/register_utils` `[协议]` — 地址/值文本统一解析（含 0x 前缀）

### 寄存器配置文件自动保存/加载
- `src/tabs/registerrwtab` `[UI-Tab]` — 表格数据自动保存至 `AppDataLocation/registers.json`，启动时自动加载；底部"批量读写"面板的文件选择按钮**当前为 UI stub，未连接信号**
- `src/widgets/registertable` `[UI-Widget]` — `loadConfig`/`saveConfig` 实现，读写 JSON 格式的地址-描述-值配置

### 示波器/波形观测
> 示波器数据流已接入，支持 8 通道 60fps 实时渲染、拖拽缩放（X/Y 轴）、双击全屏、十字光标吸附读数、调试模拟器端到端数据路径。
> 示波器工具栏控件（视图模式切换、Style、Crosshair）位于 TopBar 仅示波器页面可见；采样启停按钮内嵌在绘图区。

- `src/tabs/oscilloscoptab` `[UI-Tab]` — 示波器 Tab 容器，协调 ScopeService / RegisterService / GeneratorService / CyclicWriteService 四个服务；通过 MainWindow 桥接与 TopBar 上的示波器控件交互
- `src/widgets/scopeplotwidget` `[UI-Widget]` — 波形绘制画布（QOpenGLWidget + QPainter GL 后端，overlay/stacked 模式，拖拽缩放 X/Y 轴，双击全屏，内嵌采样按钮，十字光标）；16ms UI 定时器驱动 + 通道快照展平 + 零堆分配 paintEvent + cosmetic 画笔，8 通道稳定 60fps
- `src/widgets/scopestylepanel` `[UI-Widget]` — 示波器通道样式面板（颜色选择、线宽、线型、数据点开关）；由 TopBar 的 Style 按钮触发显隐
- `src/widgets/scopebottompanel` `[UI-Widget]` — 底部面板容器；通道条内嵌显示，寄存器/发生器面板以独立浮动窗口（`Qt::Tool`）弹出
- `src/widgets/scopechannelstrip` `[UI-Widget]` — 单通道配置条（启用开关、描述、寄存器地址）
- `src/widgets/scopemarqueelabel` `[UI-Widget]` — 跑马灯状态标签（采样/循环写入/发生器活跃状态汇总）
- `src/widgets/scoperegisterpanel` `[UI-Widget]` — 示波器侧寄存器读写面板（8 行 R/W + 循环写入间隔/启动/停止/清除/录入参数）
- `src/widgets/scopegeneratorpanel` `[UI-Widget]` — 信号发生器配置面板（Linear/Cosine/Sawtooth 模式切换 + 参数校验）
- `src/widgets/scopepreviewwidget` `[UI-Widget]` — 独立预览控件，自带正弦数据源用于 UI 演示与样式验证
- `src/models/scopechannelmodel` `[数据模型]` — 8 通道配置数据模型（启用/描述/地址/颜色/线宽/线型/数据点 + 协议参数生成）
- `src/models/channelbuffer` `[数据模型]` — 单通道双层环形缓冲（原始环 + UI 降采样环）
- `src/services/scopeservice` `[通信]` — 4 步采样启动序列（Interval/Channels/Map/Start）+ ScopeStreamBatcher 流帧批量接收 + 5s 看门狗 + 背压（QAtomicInt）
- `src/services/registerservice` `[通信]` — 寄存器面板单行/批量读写
- `src/services/cyclicwriteservice` `[通信]` — 寄存器面板循环写入（轮询 + 连续错误 5 次自停）
- `src/services/generatorservice` `[通信]` — 发生器协议命令（0x55/0x56/0x57/0x58）与运行状态管理 + 3s ACK 超时
- `src/protocol/sampling_config` `[协议]` — 采样间隔/显示窗口的 UI 文本 ↔ 协议索引映射

### 固件烧录
> 2026-05-19 起改为 STM32 本地 ISP 烧录方案（协议 v2.7 §4.3.5，命令 0x32~0x37）。废除原 AW SDK DLL 链路。
> 2026-05-24 新增协议级真实进度：协议 v2.9 §4.3.5 加 `0x38 FLASH_EXEC_PROGRESS` 事件帧（STM32→PC 主动上报，SEQ=0xFF，载荷 `[phase(1)][done(4 LE)][total(4 LE)]`，phase 0=ERASE / 1=WRITE）。STM32 在 EXEC 期间于 `aw_flash_download_check` 内 erase 前/后 + write 循环每次后主动上报，上位机据此驱动 EXEC 阶段进度条真实推进。4 阶段权重映射 DATA 20% / ERASE 5% / WRITE 70% / TAIL 5%，常量集中在 `FwFlashProgress` 命名空间。
> 框架：UI 5 区（IC 选择 / 文件选择 / 文件信息 / 烧录控制 / 操作日志）+ 文件解析（`.bin` / Intel `.hex`）+ 策略模式 + 前置序列（停采样 → 停发生器 → 停循环写入）+ **fast-path 烧录线程**（任务通过 `invokeMethod(QueuedConnection)` 投递到 SerialManager 工作线程同步执行；strategy 直接调 `SerialManager::sendAndWaitResponse` 发协议 0x32~0x37 命令）+ 协作式取消（`cancelFlag`）+ **心跳暂停 / 恢复**（EXEC 阻塞 5-10s 期间 STM32 不响应任何帧，FwFlashService 在烧录任务前 stopHeartbeat、任务完成回主线程后 startHeartbeat）。**PMIC 不在前置序列中关闭：烧录期间 IC 必须保持正常供电**。AW86006 / AW86100 共用 `AwLocalIspStrategy` 基类（BEGIN → DATA 循环 → EXEC,失败时 RESET_CHIP + CANCEL 收尾）；DW9786 / DW9788 仍为 stub。UI 加载固件后即校验 ≤ 64 KB 且 4 字节对齐（STM32 端 SRAM1 单缓冲上限）。

- `src/tabs/fwflashtab` `[UI-Tab]` — 固件烧录 Tab 容器；持有 `FlashStrategyRegistry` 与 `FwFlashService`，组织 5 区主内容布局；构造接收 `SerialManager *`，构造 `LogSink` lambda 注入 registry，让 strategy 日志转发到 `FwFlashLogPanel`；`parseAndShowFile` 加载固件后校验 ≤ 64 KB / 4 字节对齐
- `src/widgets/fwfileinfopanel` `[UI-Widget]` — 固件文件信息面板（QStackedWidget：空 / 合法 / 错误三页切换）
- `src/widgets/fwflashcontrolpanel` `[UI-Widget]` — 烧录控制面板(开始/取消按钮、进度条、阶段标签)
- `src/widgets/fwflashlogpanel` `[UI-Widget]` — 烧录操作日志面板（4 级颜色、时间戳、滚动到底自动跟随）
- `src/protocol/firmware_parser` `[协议]` — `.bin` 直读 + Intel `.hex` 解析与段合并；CRC32（IEEE 802.3）；1024 KB 通用上限（AW 本地 ISP 实际可用上限 64 KB 在 UI/strategy 层另行校验）
- `src/services/fwflashservice` `[通信]` — 状态机 + 前置序列协调 + 通过 `invokeMethod(QueuedConnection)` 把烧录任务投递到 SerialManager 工作线程（fast-path）+ 烧录前后 stopHeartbeat / startHeartbeat + 进度/日志/状态信号 + `onFlashExecProgress` slot 消费 STM32 端 0x38 进度帧（按 DATA/ERASE/WRITE/TAIL 权重折算总进度 + emitProgressPct helper 统一上报）
- `src/services/flashstrategy` `[通信]` — 烧录策略抽象基类（接口定义）
- `src/services/flashstrategyregistry` `[通信]` — 策略注册中心（构造接收 `SerialManager *` 与 `AwLocalIspStrategy::LogSink`，按 IC 型号枚举/查找）
- `src/services/flashstrategies/aw_local_isp_strategy` `[通信]` — AW 本地 ISP 烧录策略共用基类：BEGIN(0x32) → DATA 循环(0x33,252 B/帧,严格递增 pktSeq,响应校验 nextSeq) → EXEC(0x34,15 s 超时) → 失败收尾(0x37 RESET_CHIP + 0x36 CANCEL)；fast-path 同步调用 `SerialManager::sendAndWaitResponse`,Q_ASSERT_X 同线程；DATA 末尾无条件强推 100% 跳过节流，确保 UI 精确收口
- `src/services/flashstrategies/aw86006_strategy` `[通信]` — AW86006 烧录策略（继承 `AwLocalIspStrategy`，仅声明型号 / 描述）
- `src/services/flashstrategies/aw86100_strategy` `[通信]` — AW86100 烧录策略（继承 `AwLocalIspStrategy`，与 AW86006 共用 STM32 端 ISP 驱动）
- `src/services/flashstrategies/dw9786_strategy` `[通信]` — DW9786 烧录策略（stub）
- `src/services/flashstrategies/dw9788_strategy` `[通信]` — DW9788 烧录策略（stub）
- `src/protocol/motor_protocol` `[协议]` — AW 本地 ISP 命令 `0x32`~`0x37` 编解码（小端载荷,与 STM32 端 `pFrame->data[i]` 解码方式对齐）+ `AwIspStatus` 枚举与名称表 + `0x38 FLASH_EXEC_PROGRESS` 事件帧 `decodeFlashExecProgress` + `FlashExecPhase` 枚举 + 保留通用 I2C 透传 `0x30`(写) / `0x31`(读) 编解码（业务侧已不再使用,作为通用 debug 工具保留）
- `src/mainwindow.cpp` 路由：`unsolicitedFrameReceived` 分支识别 0x38 进度帧，通过 `findChild<FwFlashService>` + `QMetaObject::invokeMethod(QueuedConnection)` 转发给 service slot

### 串口调试模拟器
- `src/tabs/serialdebugtab` `[UI-Tab]` — 串口调试模拟器独立窗口，应答配置 + 活动日志 UI；点击 ActivityBar "调试" 按钮弹出，不占用 ContentStack 页面
- `src/services/simulatorservice` `[通信]` — 模拟器命令分发与模拟响应（I2C 扫描/寄存器读写/PMIC/采样启停/发生器），独立 std::thread 生成正弦波形流（基波 1Hz + 纹波 100Hz，通道间相位偏移）
- `src/services/simulatorserial` `[传输]` — 模拟器专用串口驱动，接口与 SerialManager 对称（openPort / closePort / sendRawFrame）；独立线程运行

### 主窗口框架与页面导航
- `src/main.cpp` — 程序入口，创建 QApplication 和 MainWindow，安装全局 Qt 消息处理器（将 qDebug/qWarning 路由至 LogPanel）
- `src/mainwindow` `[UI-Shell]` — 主窗口，持有并组合所有顶层组件，管理串口连接后部分页面的启用/禁用状态
- `src/widgets/activitybar` `[UI-Shell]` — 左侧活动栏（配置/读写/烧录/示波四个页面切换按钮；另有"设置"按钮为 UI 占位，未连接信号）
- `src/widgets/topbar` `[UI-Shell]` — 顶栏（Logo、串口连接状态显示、示波器页面专属控件：视图模式切换/Style/采样启停；语言切换 combo 为 UI 占位，**i18n 未实现**）

### 日志面板
- `src/widgets/logpanel` `[UI-Shell]` — 底部日志面板，全局单例，接收 Qt 消息输出

---

## 辅助资源

| 文件 | 用途 |
|------|------|
| `src/ui/scopeviewmode.h` | ScopeViewMode 枚举（Overlay/Stacked），供 ScopePlotWidget 和 OscilloscopTab 共用 |
| `src/ui/repolish.h` | QSS repolish 工具函数（unpolish + polish + update），供 Widget 动态样式切换使用 |
| `src/ui/style_constants.h` | 所有颜色、尺寸、间距常量，禁止在其他文件硬编码 |
| `CMakeLists.txt` | 项目构建定义，新增源文件必须在此注册 |

---

## 待实现功能汇总

> 快速确认哪些已实现、哪些是 stub，避免对未实现功能做错误假设。

| 功能 | 入口文件 | 状态 |
|------|---------|------|
| PMIC 电压配置 | `src/tabs/configtab` + `src/services/configservice` | 已实现：DRVDD/IOVDD/VCMVDD 三路电压输入 + 两步序列（SetVoltage → Enable）+ Disable + 5s 超时 |
| IC 配置文件写入/读出 | `src/tabs/configtab` | UI stub，按钮未连接信号 |
| 寄存器批量导入/导出（用户选择文件） | `src/tabs/registerrwtab` | UI stub，按钮未连接信号 |
| 示波器拖拽缩放（X/Y 轴） | `src/widgets/scopeplotwidget` | 已实现：鼠标拖拽选区缩放，双击全屏，右键重置 |
| 示波器十字光标 | `src/widgets/scopeplotwidget` + `src/widgets/topbar` | 已实现：TopBar 切换按钮 + 吸附最近样本读数 |
| 示波器导出/截图等操作 | `src/tabs/oscilloscoptab` | 未实现 |
| 示波器串口数据流接入 | `src/services/scopeservice` + `src/widgets/scopeplotwidget` | 已实现：ScopeStreamBatcher 跨线程批量 + 背压 + 看门狗 |
| 示波器寄存器面板（含循环写入） | `src/widgets/scoperegisterpanel` + `src/services/registerservice` + `src/services/cyclicwriteservice` | 已实现：8 行 R/W + 循环写入间隔/启停/清除 |
| 信号发生器 | `src/widgets/scopegeneratorpanel` + `src/services/generatorservice` | 已实现：Linear / Cosine / Sawtooth 三种模式 + 协议命令（0x55/0x56/0x57/0x58），波形由 STM32 执行 |
| 固件烧录 | `src/tabs/fwflashtab` + `src/services/fwflashservice` + `src/services/flashstrategies/*` | AW86006 / AW86100 已落地：`AwLocalIspStrategy` 走 STM32 本地 ISP 协议 0x32~0x37（BEGIN → DATA 循环 → EXEC，失败时 RESET_CHIP + CANCEL 收尾）；上位机不再依赖 PC 端 DLL。DW9786 / DW9788 仍为 stub |
| 多语言切换（i18n） | `src/widgets/topbar` | UI stub，combo 未连接信号 |
| 设置页面 | `src/widgets/activitybar` | UI stub，按钮未连接信号 |

---

## 架构层速查

| 层 | 文件 |
|----|------|
| 应用入口 | `src/main.cpp` |
| 传输层 | `src/frameparser` |
| 通信层 | `src/serialmanager` |
| 协议层 | `src/protocol/motor_protocol`, `src/protocol/register_utils`, `src/protocol/sampling_config`, `src/protocol/firmware_parser` |
| 数据模型层 | `src/devicecontext`, `src/models/scopechannelmodel`, `src/models/channelbuffer` |
| UI Shell | `src/mainwindow`, `src/widgets/topbar`, `src/widgets/activitybar`, `src/widgets/logpanel` |
| UI Tab | `src/tabs/configtab`, `src/tabs/registerrwtab`, `src/tabs/oscilloscoptab`, `src/tabs/fwflashtab`, `src/tabs/serialdebugtab` |
| UI Widget | `src/widgets/registertable`, `src/widgets/sidebar`, `src/widgets/scopeplotwidget`, `src/widgets/scopebottompanel`, `src/widgets/scopechannelstrip`, `src/widgets/scoperegisterpanel`, `src/widgets/scopegeneratorpanel`, `src/widgets/scopestylepanel`, `src/widgets/scopepreviewwidget`, `src/widgets/scopemarqueelabel`, `src/widgets/fwfileinfopanel`, `src/widgets/fwflashcontrolpanel`, `src/widgets/fwflashlogpanel` |
| 服务层 | `src/services/commanddispatcher`, `src/services/configservice`, `src/services/registerservice`, `src/services/scopeservice`, `src/services/cyclicwriteservice`, `src/services/generatorservice`, `src/services/simulatorservice`, `src/services/fwflashservice`, `src/services/flashstrategy`, `src/services/flashstrategyregistry`, `src/services/flashstrategies/*` |
| 开发工具传输层 | `src/services/simulatorserial` |
| 共享枚举 | `src/ui/scopeviewmode.h` |
| 样式常量 | `src/ui/style_constants.h` |

---

## 跨功能共享文件

改动这些文件时需评估全局影响：

- `src/serialmanager` — 所有需要串口通信的功能都依赖它
- `src/frameparser` — serialmanager 唯一依赖的帧解析器，协议帧格式变更必须同步修改
- `src/services/commanddispatcher` — 所有应用层 Service 的统一入口，优先级队列与响应匹配集中在此
- `src/protocol/motor_protocol` — 所有读写操作的指令编解码集中在此
- `src/protocol/sampling_config` — 采样间隔/显示窗口下拉选项的唯一来源，变更影响 UI 与协议两侧
- `src/devicecontext` — 当前 IC 类型和从机地址，目前仅 configtab 读写；其他 Tab 尚未接入
- `src/models/scopechannelmodel` — 示波器 8 通道配置数据模型，被 OscilloscopTab、ScopeStylePanel、ScopeBottomPanel、ScopeService 共享
- `src/ui/style_constants.h` — 所有 UI 组件的颜色和尺寸来源，变更影响全局外观
- `src/widgets/sidebar` — 可折叠侧边栏容器，被 configtab、registerrwtab、oscilloscoptab 三个 Tab 共用（fwflashtab 已不使用 Sidebar）
- `src/services/fwflashservice` — 通过 `findChild` 间接依赖 `ScopeService::requestStop()` / `GeneratorService::stop()` / `CyclicWriteService::stop()`；以 fire-and-forget 方式调用，3 个 Service 的实现签名变化会影响烧录前置序列。**烧录任务通过 invokeMethod 投递到 `SerialManager` 工作线程同步执行（fast-path）**，期间该线程的 event loop 被 strategy->flash() 同步占用，其他依赖 SerialManager 的 Service 在此期间提交命令会排队等候。**心跳暂停 / 恢复**：在投递烧录任务前 stopHeartbeat（QueuedConnection 同序入队），任务完成回主线程后 startHeartbeat（无论成功/失败/取消都恢复）。PMIC 不在前置序列中关闭，烧录期间 IC 必须保持正常供电
- `src/serialmanager` — `sendAndWaitResponse` 同步 API 用于烧录链路 fast-path（仅可在自己的工作线程调用，会同步阻塞该线程直到收到响应或超时）；常规命令仍走 `sendCommand`+`emit frameReceived` 异步路径；`startHeartbeat` / `stopHeartbeat` 供 FwFlashService 在烧录前后调用
