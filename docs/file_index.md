# src 模块索引

> 面向 AI 辅助开发的 `src` 模块快速定位索引。按功能场景找目标文件，避免无差别扫描源码目录。
> 粒度：文件级（`.h/.cpp` 成对，条目省略后缀）。
> 架构层标签：`[传输]` `[通信]` `[协议]` `[数据模型]` `[UI-Shell]` `[UI-Tab]` `[UI-Widget]` `[样式]`

---

## 功能场景索引

### 串口连接与断开
- `src/tabs/configtab` `[UI-Tab]` — 串口扫描/连接/断开操作入口
- `src/widgets/topbar` `[UI-Shell]` — 顶栏连接状态显示（指示灯、端口名）
- `src/serialmanager` `[通信]` — 串口打开/关闭、重试、心跳管理
- `src/frameparser` `[传输]` — 字节流解析为控制帧/流帧，并负责帧编码

### IC 扫描与连接
- `src/tabs/configtab` `[UI-Tab]` — I2C 扫描触发、IC 类型/从机地址选择
- `src/devicecontext` `[数据模型]` — 当前 IC 类型和从机地址的持久状态
- `src/protocol/motor_protocol` `[协议]` — I2C 扫描指令编码、响应解码
- `src/serialmanager` `[通信]` — 指令发送与响应接收

### PMIC 配置
- `src/tabs/configtab` `[UI-Tab]` — PMIC 电压配置 UI（DRVDD/VCMVDD/IOVDD 输入框与"配置 PMIC"按钮；**当前为 UI stub，按钮未连接信号**）

### IC 配置文件写入/读出
- `src/tabs/configtab` `[UI-Tab]` — Config File 行（文件选择 combo、Browse/Write/Read 按钮；**当前为 UI stub，按钮未连接信号**）

### 寄存器读写
- `src/tabs/registerrwtab` `[UI-Tab]` — 读写操作队列调度、批量操作、按钮事件处理
- `src/widgets/registertable` `[UI-Widget]` — 寄存器表格显示与编辑（4 组 × 20 行）
- `src/protocol/motor_protocol` `[协议]` — 读写寄存器指令编码/响应解码
- `src/serialmanager` `[通信]` — 指令发送与帧接收

### 寄存器配置文件自动保存/加载
- `src/tabs/registerrwtab` `[UI-Tab]` — 表格数据自动保存至 `AppDataLocation/registers.json`，启动时自动加载；底部"批量读写"面板的文件选择按钮**当前为 UI stub，未连接信号**
- `src/widgets/registertable` `[UI-Widget]` — `loadConfig`/`saveConfig` 实现，读写 JSON 格式的地址-描述-值配置

### 示波器/波形观测
> 示波器数据流已接入，支持 8 通道 60fps 实时渲染、拖拽缩放（X/Y 轴）、调试模拟器端到端数据路径。寄存器面板后端处理、导出/截图等为 stub。

- `src/tabs/oscilloscoptab` `[UI-Tab]` — 示波器 Tab 容器，组合各子组件，管理 running 状态、viewMode、PendingCommand 采样序列状态机、ScopeStreamBatcher 批量数据接收
- `src/widgets/scopetoolbar` `[UI-Widget]` — 工具栏（Overlay/Stacked 视图切换、采样启停按钮、运行状态标签）
- `src/widgets/scopeplotwidget` `[UI-Widget]` — 波形绘制画布（QOpenGLWidget + QPainter GL 后端，overlay/stacked 模式，拖拽缩放 X/Y 轴，双击重置视图）；16ms UI 定时器驱动 + 通道快照展平 + 零堆分配 paintEvent + cosmetic 画笔 + 4x MSAA，8 通道稳定 60fps
- `src/widgets/scopebottompanel` `[UI-Widget]` — 底部面板容器；通道条内嵌显示，寄存器/发生器面板以独立浮动窗口（`Qt::Tool`）弹出
- `src/widgets/scopechannelstrip` `[UI-Widget]` — 单通道配置条（启用开关、描述、寄存器地址）
- `src/widgets/scoperegisterpanel` `[UI-Widget]` — 示波器侧寄存器读写面板（8 行 R/W + 启动/停止/清除/录入参数，UI 完整，信号已连接；后端处理在 oscilloscoptab 中为 stub）
- `src/widgets/scopegeneratorpanel` `[UI-Widget]` — 信号发生器配置面板（UI stub）

### 固件烧录
- `src/tabs/fwflashtab` `[UI-Tab]` — 固件烧录 Tab（功能待实现）

### 串口调试模拟器
- `src/tabs/serialdebugtab` `[UI-Tab]` — 串口调试模拟器浮动窗口，可模拟 STM32 侧响应（I2C 扫描、寄存器读写、采样启停、流帧上报）；点击 ActivityBar "调试" 按钮弹出，不占用 ContentStack 页面
- `src/services/simulatorserial` `[传输]` — 模拟器专用串口驱动，接口与 SerialManager 对称（openPort / closePort / sendRawFrame）；独立线程运行，仅供 SerialDebugTab 使用

### 主窗口框架与页面导航
- `src/main.cpp` — 程序入口，创建 QApplication 和 MainWindow，安装全局 Qt 消息处理器（将 qDebug/qWarning 路由至 LogPanel）
- `src/mainwindow` `[UI-Shell]` — 主窗口，持有并组合所有顶层组件，管理串口连接后部分页面的启用/禁用状态
- `src/widgets/activitybar` `[UI-Shell]` — 左侧活动栏（配置/读写/烧录/示波四个页面切换按钮；另有"设置"按钮为 UI 占位，未连接信号）
- `src/widgets/topbar` `[UI-Shell]` — 顶栏（Logo、串口连接状态显示；语言切换 combo 为 UI 占位，**i18n 未实现**）

### 日志面板
- `src/widgets/logpanel` `[UI-Shell]` — 底部日志面板，全局单例，接收 Qt 消息输出

---

## 辅助资源

| 文件 | 用途 |
|------|------|
| `src/ui/style_constants.h` | 所有颜色、尺寸、间距常量，禁止在其他文件硬编码 |
| `CMakeLists.txt` | 项目构建定义，新增源文件必须在此注册 |

---

## 待实现功能汇总

> 快速确认哪些已实现、哪些是 stub，避免对未实现功能做错误假设。

| 功能 | 入口文件 | 状态 |
|------|---------|------|
| PMIC 电压配置 | `src/tabs/configtab` | UI stub，按钮未连接信号 |
| IC 配置文件写入/读出 | `src/tabs/configtab` | UI stub，按钮未连接信号 |
| 寄存器批量导入/导出（用户选择文件） | `src/tabs/registerrwtab` | UI stub，按钮未连接信号 |
| 示波器拖拽缩放（X/Y 轴） | `src/widgets/scopeplotwidget` | 已实现：鼠标拖拽选区缩放，双击重置视图 |
| 示波器导出/截图等工具栏操作 | `src/tabs/oscilloscoptab` | stub，仅打 debug log |
| 示波器串口数据流接入 | `src/widgets/scopeplotwidget` | 已实现：`appendSamples` 接收外部数据，16ms 定时器驱动渲染，零堆分配 paintEvent |
| 示波器寄存器面板后端处理 | `src/tabs/oscilloscoptab` | stub，信号仅打 log |
| 信号发生器 | `src/widgets/scopegeneratorpanel` | UI stub，占位面板 |
| 固件烧录 | `src/tabs/fwflashtab` | 整体未实现 |
| 多语言切换（i18n） | `src/widgets/topbar` | UI stub，combo 未连接信号 |
| 设置页面 | `src/widgets/activitybar` | UI stub，按钮未连接信号 |

---

## 架构层速查

| 层 | 文件 |
|----|------|
| 应用入口 | `src/main.cpp` |
| 传输层 | `src/frameparser` |
| 通信层 | `src/serialmanager` |
| 协议层 | `src/protocol/motor_protocol` |
| 数据模型层 | `src/devicecontext` |
| UI Shell | `src/mainwindow`, `src/widgets/topbar`, `src/widgets/activitybar`, `src/widgets/logpanel` |
| UI Tab | `src/tabs/configtab`, `src/tabs/registerrwtab`, `src/tabs/oscilloscoptab`, `src/tabs/fwflashtab`, `src/tabs/serialdebugtab` |
| UI Widget | `src/widgets/registertable`, `src/widgets/sidebar`, `src/widgets/scopetoolbar`, `src/widgets/scopeplotwidget`, `src/widgets/scopebottompanel`, `src/widgets/scopechannelstrip`, `src/widgets/scoperegisterpanel`, `src/widgets/scopegeneratorpanel` |
| 开发工具传输层 | `src/services/simulatorserial` |
| 样式常量 | `src/ui/style_constants.h` |

---

## 跨功能共享文件

改动这些文件时需评估全局影响：

- `src/serialmanager` — 所有需要串口通信的功能都依赖它
- `src/frameparser` — serialmanager 唯一依赖的帧解析器，协议帧格式变更必须同步修改
- `src/protocol/motor_protocol` — 所有读写操作的指令编解码集中在此
- `src/devicecontext` — 当前 IC 类型和从机地址，目前仅 configtab 读写；其他 Tab 尚未接入
- `src/ui/style_constants.h` — 所有 UI 组件的颜色和尺寸来源，变更影响全局外观
- `src/widgets/sidebar` — 可折叠侧边栏容器，被 configtab、registerrwtab、fwflashtab、oscilloscoptab 四个 Tab 共用
