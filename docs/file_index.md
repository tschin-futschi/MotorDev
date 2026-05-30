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
- `src/tabs/registerrwtab` `[UI-Tab]` — 表格事件转发、Hex/Dec 切换、Sidebar 全部读/写、批量读写浮窗（4 槽独立操作，全局互斥）、块读取浮窗（独立通道，不互斥）
- `src/services/registerservice` `[通信]` — 单行/批量读写队列、500ms 超时、sessionId 防过期响应
- `src/services/batchregisterservice` `[通信]` — 批量读写浮窗的业务层：内部持有独立 RegisterService 实例（与 RegisterTable 隔离）+ 状态机（Idle/Parsing/Writing/Reading/Completed/Failed）+ 调用 `BatchRegisterFile` 解析与回写；通过 `stageMessage` / `logMessage` / `finished` 信号上报，UI 只渲染不掺业务
- `src/services/blockreadservice` `[通信]` — 块读取浮窗的业务层：独立 RegisterService 实例（与 RegisterTable / BatchRegisterService 完全隔离）+ 状态机（Idle/Reading/WritingFile/Completed/Failed/Cancelled）+ 协作式取消（cancelFlag）+ 失败即停（已成功条目仍写文件）；通过 `stateChanged` / `progress` / `stageMessage` / `logMessage` / `finished` 信号上报；CSV 输出按标准格式（首行表头 `addr,value` + 4 位大写 hex 无 `0x` + LF 行尾）；文件命名 `Bulkread_HHMMSS.csv`（PC 本地时间，同秒冲突自动 `_N`）
- `src/widgets/registertable` `[UI-Widget]` — 寄存器表格显示与编辑（4 组 × 30 行）
- `src/protocol/motor_protocol` `[协议]` — 读写寄存器指令编码/响应解码
- `src/protocol/register_utils` `[协议]` — 地址/值文本解析与格式化唯一来源（`parseNumber` 无符号地址；`parseSignedValue` 有符号值 0x→HEX/否则 DEC；`formatValue` 按 HEX/DEC 格式化）；被 RegisterTable 与 ScopeRegisterPanel 共用

### 寄存器配置文件自动保存/加载
- `src/tabs/registerrwtab` `[UI-Tab]` — 表格数据自动保存至 `AppDataLocation/registers.json`，启动时自动加载；批量读写浮窗（弹出式 Qt::Tool）4 槽独立支持「配置文件 ↔ 芯片寄存器」往返，浮窗状态 / 路径不持久化（每次启动空白）；块读取浮窗（独立 Qt::Tool）按起始地址 + 个数（步进 +2）连续 dump 到目录下的 `Bulkread_HHMMSS.csv`，浮窗仅记忆「上次保存目录」其余字段每次空白
- `src/widgets/registertable` `[UI-Widget]` — `loadConfig`/`saveConfig` 实现，读写 JSON 格式的地址-描述-值配置
- `src/protocol/batch_register_file` `[协议]` — 批量读写浮窗的配置文件解析与回写：`<addr>, <value>` + `//` 注释 + 空行 三态行处理；解析严格全或无（含格式错 / 重复地址 / 越界 等异常分类）；回写时保留原文件结构（注释 / 行序 / 行末注释）仅替换数据行「值」列，输出统一删空行

### 示波器/波形观测
> 示波器数据流已接入，支持 8 通道 60fps 实时渲染、拖拽缩放（X/Y 轴）、双击全屏、十字光标吸附读数、调试模拟器端到端数据路径。
> 数据记录：采样期间将全速率原始采样持续写入 CSV（`ScopeRecordService`），采样启停即录制启停，目录由顶部行 Record Dir 控件指定。
> 示波器工具栏控件（视图模式切换、Style、Crosshair）位于 TopBar 仅示波器页面可见；采样启停按钮内嵌在绘图区。

- `src/tabs/oscilloscoptab` `[UI-Tab]` — 示波器 Tab 容器，协调 ScopeService / RegisterService / GeneratorService / CyclicWriteService / ScopeRecordService 五个服务；通过 MainWindow 桥接与 TopBar 上的示波器控件交互；记录目录经 QSettings(`scope/recordDir`) 跨会话持久化
- `src/widgets/scopeplotwidget` `[UI-Widget]` — 波形绘制画布（QOpenGLWidget + QPainter GL 后端，overlay/stacked 模式，拖拽缩放 X/Y 轴，双击全屏，内嵌采样按钮，十字光标）；16ms UI 定时器驱动 + 通道快照展平 + 零堆分配 paintEvent + cosmetic 画笔，8 通道稳定 60fps
- `src/widgets/scopestylepanel` `[UI-Widget]` — 示波器通道样式面板（颜色选择、线宽、线型、数据点开关）；由 TopBar 的 Style 按钮触发显隐
- `src/widgets/scopebottompanel` `[UI-Widget]` — 底部面板容器；通道条内嵌显示，寄存器/发生器面板以独立浮动窗口（`Qt::Tool`）弹出
- `src/widgets/scopechannelstrip` `[UI-Widget]` — 单通道配置条（启用开关、描述、寄存器地址）
- `src/widgets/scopemarqueelabel` `[UI-Widget]` — 跑马灯状态标签（采样/循环写入/发生器活跃状态汇总）
- `src/widgets/scoperegisterpanel` `[UI-Widget]` — 示波器侧寄存器读写面板（8 行 R/W + 循环写入间隔/启动/停止/清除 + HEX/DEC 切换按钮）；值列输入按 0x 前缀判进制（有符号 DEC -32768~32767），非法值红框（editingFinished/写入/切换时校验），切换按当前模式重排合法值；解析/格式化复用 `register_utils`
- `src/widgets/scopegeneratorpanel` `[UI-Widget]` — 信号发生器配置面板（Linear/Cosine/Sawtooth 模式切换 + 参数校验）
- `src/widgets/scopepreviewwidget` `[UI-Widget]` — 独立预览控件，自带正弦数据源用于 UI 演示与样式验证
- `src/models/scopechannelmodel` `[数据模型]` — 8 通道配置数据模型（启用/描述/地址/颜色/线宽/线型/数据点 + 协议参数生成）
- `src/models/channelbuffer` `[数据模型]` — 单通道双层环形缓冲（原始环 + UI 降采样环）
- `src/services/scopeservice` `[通信]` — 4 步采样启动序列（Interval/Channels/Map/Start）+ ScopeStreamBatcher 流帧批量接收 + 5s 看门狗 + 背压（QAtomicInt）
- `src/services/scoperecordservice` `[通信]` — 示波器数据持续记录：监听 ScopeService 的 `acquisitionConfigured`/`runningChanged`/`samplesReceived`，采样启动即开录、停止即停录；全速率原始采样写入 `Scope_YYYYMMDD_HHMMSS.csv`（首行注释元信息 + `time_us,CHx,...`，有符号十进制，每帧一行）；列 = 会话开始时 `currentChannelMask` 通道；目录由 UI/QSettings 提供，未设/无效则不录并经 `recordError` 提示；主线程缓冲写 + 1s flush
- `src/services/registerservice` `[通信]` — 寄存器面板单行/批量读写
- `src/services/cyclicwriteservice` `[通信]` — 寄存器面板循环写入（轮询 + 连续错误 5 次自停）
- `src/services/generatorservice` `[通信]` — 发生器协议命令（0x55/0x56/0x57/0x58）与运行状态管理 + 3s ACK 超时
- `src/protocol/sampling_config` `[协议]` — 采样间隔/显示窗口的 UI 文本 ↔ 协议索引映射

### 固件烧录
> 2026-05-19 起改为 STM32 本地 ISP 烧录方案（协议 v2.7 §4.3.5，命令 0x32~0x37）。废除原 AW SDK DLL 链路。
> 2026-05-24 新增协议级真实进度：协议 v2.9 §4.3.5 加 `0x38 FLASH_EXEC_PROGRESS` 事件帧（STM32→PC 主动上报，SEQ=0xFF，载荷 `[phase(1)][done(4 LE)][total(4 LE)]`，phase 0=ERASE / 1=WRITE）。STM32 在 EXEC 期间于 `aw_flash_download_check` 内 erase 前/后 + write 循环每次后主动上报，上位机据此驱动 EXEC 阶段进度条真实推进。4 阶段权重映射 DATA 20% / ERASE 5% / WRITE 70% / TAIL 5%，常量集中在 `FwFlashProgress` 命名空间。
> 2026-05-27 新增 DW9788 (Dongwoon HL9788N) 真实烧录：通过 `Hl9788nBridge` 把 vendor SDK 与 SerialManager 桥接，vendor 内部 I2C 调用翻译为协议 0x30 / 0x31 透传命令；DW9788 路径**不走** 0x32~0x37 本地 ISP，而是 vendor SDK 一次性完成 `id_check → fw_update_dma → ois_reset → fw_info → verify`，**OTA 模式默认保留模组校准数据**。FirmwareParser 同时支持 HL9788N 自定义 hex 文本（每行 1 个 32-bit hex 字，共 16384 行 = 64 KB），解析为 65536 字节小端二进制后直接 reinterpret_cast 给 vendor SDK。
> 框架：UI 5 区（IC 选择 / 文件选择 / 文件信息 / 烧录控制 / 操作日志）+ 文件解析（`.bin` / Intel `.hex` / HL9788N hex）+ 策略模式 + 前置序列（停采样 → 停发生器 → 停循环写入）+ **fast-path 烧录线程**（任务通过 `invokeMethod(QueuedConnection)` 投递到 SerialManager 工作线程同步执行；AW strategy 直接调 `SerialManager::sendAndWaitResponse` 发协议 0x32~0x37 命令，DW9788 strategy 走 vendor SDK + 0x30/0x31 透传）+ 协作式取消（`cancelFlag`）+ **心跳暂停 / 恢复**（EXEC 阻塞 5-10s 期间 STM32 不响应任何帧，FwFlashService 在烧录任务前 stopHeartbeat、任务完成回主线程后 startHeartbeat）。**PMIC 不在前置序列中关闭：烧录期间 IC 必须保持正常供电**。AW86008 / AW86100 共用 `AwLocalIspStrategy` 基类（BEGIN → DATA 循环 → EXEC,失败时 RESET_CHIP + CANCEL 收尾）；DW9788 通过 `Hl9788nBridge` + vendor SDK 完成（见上）；DW9786 仍为 stub。UI 加载固件后即校验：AW 路径 ≤ 64 KB 且 4 字节对齐（STM32 端 SRAM1 单缓冲上限）；HL9788N 路径固定 64 KB。

- `src/tabs/fwflashtab` `[UI-Tab]` — 固件烧录 Tab 容器；持有 `FlashStrategyRegistry` 与 `FwFlashService`，组织 5 区主内容布局；构造接收 `SerialManager *` 与 `DeviceContext *`（目标 IC 下拉**只读单向跟随**配置页 Select IC，监听 `DeviceContext::icTypeChanged` 经 `syncIcFromContext` 按 icModel 字符串选中，无占位项）；构造 `LogSink` lambda 注入 registry，让 strategy 日志转发到 `FwFlashLogPanel`；`parseAndShowFile` 加载固件后校验 ≤ 64 KB / 4 字节对齐；HL9788N hex 触发补齐时由 `saveHl9788nPaddedHex` 把实际烧入的 16384 行 hex 文本另存到原始 hex 同目录，文件名 `<stem>--00000000--<HHMMSS>.hex`（同秒冲突追加 `_N`），失败仅 warn 不阻塞烧录
- `src/widgets/fwfileinfopanel` `[UI-Widget]` — 固件文件信息面板（QStackedWidget：空 / 合法 / 错误三页切换）
- `src/widgets/fwflashcontrolpanel` `[UI-Widget]` — 烧录控制面板(开始/取消按钮、进度条、阶段标签)
- `src/widgets/fwflashlogpanel` `[UI-Widget]` — 烧录操作日志面板（4 级颜色、时间戳、滚动到底自动跟随）
- `src/protocol/firmware_parser` `[协议]` — `.bin` 直读 + Intel `.hex` 解析与段合并 + **HL9788N / DW9786 自定义 hex 解析**（每行 1 个 32-bit hex 字；HL9788N: 16384 行 = 32768 words = 65536 字节，拆字 low 先；DW9786: 10240 行 = 20480 words = 40960 字节，拆字 high 先；两种格式行内字符相同无法靠内容嗅探区分，由 `IcHint` 参数（Auto / Hl9788 / Dw9786）强制路由，调用方按 IC 选择传 hint；**N<期望行数自动补齐**：填 0 + 倒数第二行写 footer CRC32(原始 hex 全部字节, IEEE 802.3) + 末行全 0，N==期望保持向后兼容不写 footer，N>期望仍报错；两种格式共用 `parseDwHexGeneric` 通用 helper 参数化 expectedWords + highFirst）；CRC32（IEEE 802.3）；1024 KB 通用上限（AW 本地 ISP 实际可用上限 64 KB 在 UI/strategy 层另行校验）；`FirmwareInfo` 含 `paddingApplied / originalLines / footerCrc32` 三个 DW 自定义 hex 专属字段
- `src/services/fwflashservice` `[通信]` — 状态机 + 前置序列协调 + 通过 `invokeMethod(QueuedConnection)` 把烧录任务投递到 SerialManager 工作线程（fast-path）+ 烧录前后 stopHeartbeat / startHeartbeat + 进度/日志/状态信号 + `onFlashExecProgress` slot 消费 STM32 端 0x38 进度帧（按 DATA/ERASE/WRITE/TAIL 权重折算总进度 + emitProgressPct helper 统一上报）
- `src/services/flashstrategy` `[通信]` — 烧录策略抽象基类（接口定义）
- `src/services/flashstrategyregistry` `[通信]` — 策略注册中心（构造接收 `SerialManager *` 与 `AwLocalIspStrategy::LogSink`，按 IC 型号枚举/查找）
- `src/services/flashstrategies/aw_local_isp_strategy` `[通信]` — AW 本地 ISP 烧录策略共用基类：BEGIN(0x32) → DATA 循环(0x33,252 B/帧,严格递增 pktSeq,响应校验 nextSeq) → EXEC(0x34,15 s 超时) → 失败收尾(0x37 RESET_CHIP + 0x36 CANCEL)；fast-path 同步调用 `SerialManager::sendAndWaitResponse`,Q_ASSERT_X 同线程；DATA 末尾无条件强推 100% 跳过节流，确保 UI 精确收口
- `src/services/flashstrategies/aw86008_strategy` `[通信]` — AW86008 烧录策略（继承 `AwLocalIspStrategy`，仅声明型号 / 描述）
- `src/services/flashstrategies/aw86100_strategy` `[通信]` — AW86100 烧录策略（继承 `AwLocalIspStrategy`，与 AW86008 共用 STM32 端 ISP 驱动）
- `src/services/flashstrategies/dw9786_strategy` `[通信]` — DW9786 真实烧录策略：通过 `Dw9786Bridge` 调 Dongwoon DW9786 vendor SDK；fast-path 同线程；OTA 模式（默认）严禁 `dw9786_module_cal_erase`；固件 `FirmwareParser::parseDw9786Hex` 出来的 40960 字节小端二进制直接 reinterpret_cast 给 SDK；序列 `id_check(0x9786) → fw_download_with_buffer (erase + sequential write + checksum) → chip_enable + mcu_active → fw_info + chip_info`
- `src/services/flashstrategies/dw9786_bridge` `[通信]` — DW9786 vendor 库与 SerialManager 的桥接层：通过 3 个 vendor 全局函数指针（WriteI2CDev / ReadI2CDev / OutputLog）把 vendor I2C 调用翻译为 0x30 / 0x31 透传命令；progress / cancel hook 注入；attach / detach 单例语义
- `src/services/flashstrategies/dw9786_vendor_include.h` `[通信]` — vendor 包装头：先让 Qt/std 头展开，再引入 vendor 并 `#undef` 污染宏（Wait / round）；所有需引用 vendor 的 .cpp 必须 `#include` 它而非直接 include vendor 头
- `src/services/flashstrategies/vendor/dw9786/Func`、`dw9786_api_ref`、`stdafx.h` `[通信-第三方]` — Dongwoon 原厂 vendor 源码（最小改动以适配 MotorDev 工程：`#define UNICODE off` / `_T(x) x` 覆盖 / `Wait` undef / `round` undef / vendor cpp 关闭 UNICODE / vendor warning 抑制 / vendor 业务代码不动；MOTORDEV PATCH 仅在 `dw9786_api_ref.{h,cpp}` 加 `_with_buffer` sibling + 进度/取消 hook + 原 `dw9786_fw_download` 改 forward）
- `src/services/flashstrategies/vendor/dw9786/dw9786_symbol_rename.h` `[通信-第三方]` — 预处理器级 `#define` 把 dw9786 vendor 所有冲突的内部符号 rename 到 `dw9786_v_*` 前缀（15 个数据全局 + 17 个函数），让 HL9788N / DW9786 两 vendor 在 link 阶段完全独立，无需 `--allow-multiple-definition` link flag；由 `stdafx.h`（vendor TU）和 `dw9786_vendor_include.h`（bridge/strategy TU）顶部 include 进来。**grep 调试时注意：dw9786 vendor 内部的 `WriteI2CDev/RamWriteA/m_delay/_SLV_OIS_/...` 实际链接符号是 `dw9786_v_*` 前缀。** vendor 升级时若引入新冲突符号需同步更新本头
- `src/services/flashstrategies/dw9788_strategy` `[通信]` — DW9788 (HL9788N) 真实烧录策略：通过 `Hl9788nBridge` 调 vendor SDK；fast-path 同线程；OTA 模式（默认）严禁调用 `module_cal_erase`；量产模式可显式开启；固件 `FirmwareParser::parseHl9788Hex` 出来的 65536 字节小端二进制直接 reinterpret_cast 给 SDK
- `src/services/flashstrategies/hl9788n_bridge` `[通信]` — HL9788N vendor 库与 SerialManager 的桥接层：通过 3 个 vendor 全局函数指针（WriteI2CDev / ReadI2CDev / OutputLog）把 vendor I2C 调用翻译为 0x30 / 0x31 透传命令；progress / cancel hook 注入；`describeRvStatus` 把 `hl9788n_rv_status()` 位掩码翻成中文；attach / detach 单例语义（同时刻仅 1 个 DW IC 烧录）
- `src/services/flashstrategies/hl9788n_vendor_include.h` `[通信]` — vendor 包装头：先让 Qt/std 头展开，再引入 vendor 并 `#undef` 污染宏（Wait / round）；所有需引用 vendor 的 .cpp 必须 `#include` 它而非直接 include vendor 头
- `src/services/flashstrategies/vendor/hl9788n/Func`、`hl9788n_api_ref`、`stdafx.h` `[通信-第三方]` — Dongwoon 原厂 vendor 源码（最小改动以适配 MotorDev 工程：`#define UNICODE off` / CMake 关闭 vendor TU 的 UNICODE / 静音特定警告；vendor 业务代码不动）
- `src/protocol/motor_protocol` `[协议]` — AW 本地 ISP 命令 `0x32`~`0x37` 编解码（小端载荷,与 STM32 端 `pFrame->data[i]` 解码方式对齐）+ `AwIspStatus` 枚举与名称表 + `0x38 FLASH_EXEC_PROGRESS` 事件帧 `decodeFlashExecProgress` + `FlashExecPhase` 枚举 + 保留通用 I2C 透传 `0x30`(写) / `0x31`(读) 编解码（业务侧已不再使用,作为通用 debug 工具保留）
- `src/mainwindow.cpp` 路由：`unsolicitedFrameReceived` 分支识别 0x38 进度帧，通过 `findChild<FwFlashService>` + `QMetaObject::invokeMethod(QueuedConnection)` 转发给 service slot

### STM32 自身 FLASH 文件存储（协议 v2.11 / 0x39~0x3F）
> 与 §"固件烧录"（AW IC ISP）**完全独立**的功能：上位机把任意本地文件上传到 STM32 内置 Flash Sector 5-11（`[0x08020000, 0x08100000)`，896 KB）暂存，另一台电脑可下载下来。下载字节与原始上传文件 1:1 无损，改扩展名即可复原。单插槽覆盖模型：每次上传整区擦除（~3-7s 阻塞），不做多文件管理。**v2.11 新增 0x3F WIPE：用户主动清空整区,消除存储痕迹（含元数据）**。
> STM32 端：`Linker/STM32F429ZGTX_FLASH.ld` `FLASH LENGTH` 已收缩 1024K → 128K 防固件膨胀覆盖文件区；`BSP/Src/bsp_flash.c` 提供 Erase/Program/Read；`App/Src/app_flashstore.c` 提供 8 个 API（Init/WriteBegin/Data/End/ReadBegin/Data/GetInfo/Wipe）+ 16B 元数据 `[magic 0xA5C3E18F][size][crc32][reserved]`；`App/Src/app_protocol.c` 加 7 个 handler（0x39~0x3F）。CRC32 算法 IEEE 802.3，两侧严格对算。

- `src/tabs/flashstoragetab` `[UI-Tab]` — FLASH 存储 Tab 容器，单栏布局：容量行（已用/总/剩余 + 刷新）+ 操作行（上传/下载/取消/**清空 FLASH** + 阶段标签）+ 进度条 + 日志面板（复用 `FwFlashLogPanel`）。"清空 FLASH" 按钮触发 `QMessageBox::warning` 二次确认弹窗（默认按钮 No 防误触发）。构造接收 `SerialManager *`，创建 `FlashStoreService` 并 wire 7 个信号
- `src/services/flashstoreservice` `[通信]` — 状态机（Idle/WritePreparing/WriteBeginning/WriteSending/WriteEnding/ReadBeginning/ReadFetching/QueryingInfo/Completed/Failed/Cancelled；Wipe 复用 WriteBeginning 状态）+ fast-path 投递到 SerialManager 工作线程同步调 `sendAndWaitResponse` + 心跳暂停/恢复（与 FwFlashService 同款）+ 协作式取消。四种操作：startWrite（读文件 → 算 CRC32 → 0x39 → 0x3A 循环 → 0x3B）/ startRead（0x3C → 0x3D 循环 → 落盘 → 本地 CRC 校验）/ refreshInfo（0x3E 单帧）/ **startWipe（0x3F 单帧 ~3-7s，成功后自动 emit infoUpdated(917488, 0) 刷新容量行）**。下载文件名 `FLASH_HHMMSS.txt`（PC 本地时间，同秒冲突追加 `_N`）
- `src/protocol/motor_protocol` `[协议]` — 7 个命令码 `CmdFlashStoreWriteBegin / WriteData / WriteEnd / ReadBegin / ReadData / Info / Wipe`（0x39~0x3F）+ `FlashStoreStatus` 枚举与 `flashStoreStatusName` + 4 个 encode 函数 + 4 个 decode 函数（小端 LE，与 STM32 端 FsStatus 严格对齐）。Wipe 命令空载荷,响应 1B status 复用 `decodeFlashStoreSimpleStatus`
- `src/protocol/firmware_parser` `[协议]` — `computeCrc32` 被 FlashStoreService 复用，与 STM32 端 `app_flashstore.c::crc32_update` byte-by-byte 一致

### 串口调试模拟器
- `src/tabs/serialdebugtab` `[UI-Tab]` — 串口调试模拟器独立窗口，应答配置 + 活动日志 UI；点击 ActivityBar "调试" 按钮弹出，不占用 ContentStack 页面
- `src/services/simulatorservice` `[通信]` — 模拟器命令分发与模拟响应（I2C 扫描/寄存器读写/PMIC/采样启停/发生器），独立 std::thread 生成正弦波形流（基波 1Hz + 纹波 100Hz，通道间相位偏移）
- `src/services/simulatorserial` `[传输]` — 模拟器专用串口驱动，接口与 SerialManager 对称（openPort / closePort / sendRawFrame）；独立线程运行

### 主窗口框架与页面导航
- `src/main.cpp` — 程序入口，创建 QApplication 和 MainWindow，安装全局 Qt 消息处理器（将 qDebug/qWarning 路由至 LogPanel）
- `src/mainwindow` `[UI-Shell]` — 主窗口，持有并组合所有顶层组件，管理串口连接后部分页面的启用/禁用状态
- `src/widgets/activitybar` `[UI-Shell]` — 左侧活动栏（配置/读写/烧录/示波/存储 五个页面切换按钮 + 调试按钮（浮动窗口）；另有"设置"按钮为 UI 占位，未连接信号）
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
| 寄存器批量导入/导出（用户选择文件） | `src/tabs/registerrwtab` + `src/protocol/batch_register_file` | 已实现：4 槽独立操作（前 2 写、后 2 读）；配置文件格式 `<addr>, <value>` + `//` 注释（参考 `register_read.txt`）；遇错即停；读出全或无（中止时原文件不动）；全局互斥（与 Sidebar 全部读/写 互锁）；浮窗内底部状态文字反馈 |
| 寄存器块读取（连续地址段 dump） | `src/tabs/registerrwtab` + `src/services/blockreadservice` | 已实现：起始地址 + 个数（步进 +2）→ `Bulkread_HHMMSS.csv`；失败即停 / 协作式取消（已成功条目仍写文件）；进度条 + 状态文字反馈；独立通道（不与 Sidebar 全部读/写 / 批量读写互斥） |
| 示波器拖拽缩放（X/Y 轴） | `src/widgets/scopeplotwidget` | 已实现：鼠标拖拽选区缩放，双击全屏，右键重置 |
| 示波器十字光标 | `src/widgets/scopeplotwidget` + `src/widgets/topbar` | 已实现：TopBar 切换按钮 + 吸附最近样本读数 |
| 示波器数据记录（CSV） | `src/services/scoperecordservice` + `src/widgets/scopebottompanel` | 已实现：采样启停即录制启停，全速率原始采样写 `Scope_YYYYMMDD_HHMMSS.csv`；目录用户指定（Record Dir + 浏览 + QSettings 持久化），未设/无效则不录并提示；"打开"按钮用 Excel 打开最新记录文件（`latestRecordFile()` + `start excel`） |
| 示波器截图等操作 | `src/tabs/oscilloscoptab` | 未实现 |
| 示波器串口数据流接入 | `src/services/scopeservice` + `src/widgets/scopeplotwidget` | 已实现：ScopeStreamBatcher 跨线程批量 + 背压 + 看门狗 |
| 示波器寄存器面板（含循环写入） | `src/widgets/scoperegisterpanel` + `src/services/registerservice` + `src/services/cyclicwriteservice` | 已实现：8 行 R/W + 循环写入间隔/启停/清除 |
| 信号发生器 | `src/widgets/scopegeneratorpanel` + `src/services/generatorservice` | 已实现：Linear / Cosine / Sawtooth 三种模式 + 协议命令（0x55/0x56/0x57/0x58），波形由 STM32 执行 |
| 固件烧录 | `src/tabs/fwflashtab` + `src/services/fwflashservice` + `src/services/flashstrategies/*` | AW86008 / AW86100 / DW9788 / DW9786 全部已落地。AW86008/AW86100：`AwLocalIspStrategy` 走 STM32 本地 ISP 协议 0x32~0x37（BEGIN → DATA 循环 → EXEC，失败时 RESET_CHIP + CANCEL 收尾）；上位机不再依赖 PC 端 DLL。DW9788：`DW9788Strategy` + `Hl9788nBridge` 调 Dongwoon HL9788N vendor SDK；DW9786：`DW9786Strategy` + `Dw9786Bridge` 调 Dongwoon DW9786 vendor SDK，序列 id_check + fw_download_with_buffer (erase+write+checksum) + chip_enable；vendor I2C 通过 0x30 / 0x31 透传到 STM32；OTA 模式默认保留模组校准；HL9788N 与 DW9786 vendor 共享同名全局符号，link 阶段用 `--allow-multiple-definition` 合并，两桥接层调用方互斥保障运行时正确性 |
| STM32 FLASH 文件存储 | `src/tabs/flashstoragetab` + `src/services/flashstoreservice` | 已实现：单插槽覆盖（每次擦 Sector 5-11 共 896 KB）+ 上传/下载/容量查询/**清空 FLASH(0x3F WIPE)**；下载 1:1 无损（改扩展名复原）；协议 v2.11 / 0x39~0x3F |
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
| 服务层 | `src/services/commanddispatcher`, `src/services/configservice`, `src/services/registerservice`, `src/services/batchregisterservice`, `src/services/blockreadservice`, `src/services/scopeservice`, `src/services/cyclicwriteservice`, `src/services/generatorservice`, `src/services/simulatorservice`, `src/services/fwflashservice`, `src/services/flashstoreservice`, `src/services/flashstrategy`, `src/services/flashstrategyregistry`, `src/services/flashstrategies/*` |
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
- `src/devicecontext` — 当前 IC 类型和从机地址：configtab 读写（Select IC）；fwflashtab 单向只读接入（目标 IC 跟随 `icTypeChanged`）；其他 Tab 尚未接入
- `src/models/scopechannelmodel` — 示波器 8 通道配置数据模型，被 OscilloscopTab、ScopeStylePanel、ScopeBottomPanel、ScopeService 共享
- `src/ui/style_constants.h` — 所有 UI 组件的颜色和尺寸来源，变更影响全局外观
- `src/widgets/sidebar` — 可折叠侧边栏容器，被 registerrwtab、oscilloscoptab 两个 Tab 共用（configtab、fwflashtab、flashstoragetab 已不使用 Sidebar）
- `src/services/fwflashservice` — 通过 `findChild` 间接依赖 `ScopeService::requestStop()` / `GeneratorService::stop()` / `CyclicWriteService::stop()`；以 fire-and-forget 方式调用，3 个 Service 的实现签名变化会影响烧录前置序列。**烧录任务通过 invokeMethod 投递到 `SerialManager` 工作线程同步执行（fast-path）**，期间该线程的 event loop 被 strategy->flash() 同步占用，其他依赖 SerialManager 的 Service 在此期间提交命令会排队等候。**心跳暂停 / 恢复**：在投递烧录任务前 stopHeartbeat（QueuedConnection 同序入队），任务完成回主线程后 startHeartbeat（无论成功/失败/取消都恢复）。PMIC 不在前置序列中关闭，烧录期间 IC 必须保持正常供电
- `src/serialmanager` — `sendAndWaitResponse` 同步 API 用于烧录链路 fast-path（仅可在自己的工作线程调用，会同步阻塞该线程直到收到响应或超时）；常规命令仍走 `sendCommand`+`emit frameReceived` 异步路径；`startHeartbeat` / `stopHeartbeat` 供 FwFlashService 在烧录前后调用
