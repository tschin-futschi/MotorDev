# MotorDev UI 设计规格文档

> 版本：v0.2 | 日期：2026-05-06
> 本文档定义所有 UI 组件的精确规格，供开发实现参考。

---

## 色彩系统

### 主色调

| 名称 | 色值 | 用途 |
|------|------|------|
| 主绿色 | `#3B6D11` | 状态栏背景、连接按钮边框、激活图标文字 |
| 浅绿色 | `#EAF3DE` | 连接按钮背景、添加行按钮背景 |
| 边框绿 | `#c0dd97` | 激活图标边框 |
| 文字绿 | `#97C459` | Logo 文字 |
| 状态栏文字 | `#c0dd97` | 状态栏文字颜色 |

### 背景色

| 名称 | 色值 | 用途 |
|------|------|------|
| 窗口背景 | `#f5f5f3` | 整体窗口背景 |
| 标题栏 | `#e0ddd8` | 窗口标题栏 |
| 活动栏 | `#e8e5e0` | 左侧活动栏背景 |
| 侧边栏 | `#f0ede8` | 侧边配置栏背景 |
| 顶部栏 | `#f8f7f5` | 顶部信息栏背景 |
| 表头 | `#f0ede8` | 表格表头背景 |
| 偶数行 | `#fafaf8` | 表格偶数行背景 |
| Hover 行 | `#f0f7e8` | 表格鼠标悬停行 |

### 功能色

| 名称 | 色值 | 用途 |
|------|------|------|
| 读按钮背景 | `#E6F1FB` | 读操作按钮背景 |
| 读按钮边框/文字 | `#185FA5` | 读操作按钮前景 |
| 写按钮背景 | `#FAEEDA` | 写操作按钮背景 |
| 写按钮边框/文字 | `#854F0B` | 写操作按钮前景 |
| 值列文字 | `#185FA5` | 寄存器值显示颜色 |
| 错误值文字 | `#E24B4A` | 异常寄存器值颜色 |
| 未连接指示灯 | `#E24B4A` | 连接状态红灯 |
| 已连接指示灯 | `#639922` | 连接状态绿灯 |

### 边框色

| 名称 | 色值 | 用途 |
|------|------|------|
| 默认边框 | `#ccc` | 通用边框 |
| 输入框边框 | `#ddd` | 表单输入框边框 |
| 表格行边框 | `#eee` | 表格单元格边框 |
| 组分隔线 | `#aaa` | 4组之间的分隔竖线，2px |
| 表头边框 | `#ccc` | 表头单元格边框 |

---

## 字体规格

| 用途 | 字号 | 字重 | 颜色 |
|------|------|------|------|
| 应用名称 | 15px | 500 | `#2c2c2a` |
| 侧边栏标题 | 10px | 500 | `#888`，大写 |
| 侧边栏标签 | 11px | 400 | `#666` |
| 侧边栏分组标题 | 10px | 500 | `#aaa` |
| 顶部栏标签 | 11px | 400 | `#aaa` |
| 顶部栏数值 | 11px | 400 | `#555` |
| 活动栏图标文字 | 9px | 400 | `#888`（激活时 `#3B6D11`） |
| 表头文字 | 12px | 500 | `#555` |
| 表格数据 | 12px | 400 | `#333`（值列 monospace） |
| 按钮文字 | 10～11px | 400 | 各按钮专属色 |
| 状态栏文字 | 10px | 400 | `#c0dd97` |

---

## 尺寸规格

### 主窗口

| 区域 | 尺寸 |
|------|------|
| 默认窗口 | 1280×800px |
| 最小窗口 | 1024×600px |
| 顶部栏高度 | 36px |
| 状态栏高度 | 22px |
| 活动栏宽度 | 44px（固定） |
| 侧边栏默认宽度 | 150px（可收缩至 0） |

### 活动栏图标

| 属性 | 值 |
|------|-----|
| 按钮尺寸 | 34×34px |
| 圆角 | 6px |
| 图标尺寸 | 16×16px |
| 文字大小 | 9px |
| 激活边框 | 0.5px solid `#c0dd97` |

### 连接指示灯

| 属性 | 值 |
|------|-----|
| 直径 | 7px |
| 形状 | 圆形 |
| 未连接色 | `#E24B4A` |
| 已连接色 | `#639922` |

---

## 组件规格

### 顶部栏（TopBar）

```
高度: 36px
背景: #f8f7f5
边框: 底部 0.5px solid #eee
内边距: 0 12px

从左到右:
[Logo 22×22] [间距8px] [MotorDev 13px/500] [分隔线]
[串口 label] [串口值] [连接指示灯7px] [连接状态文字]
[MCU 指示灯7px] [MCU 状态文字]
[语言下拉框 → 右对齐]
```

**MCU 启动状态徽章**（响应协议 `0x0B BOOT_STATUS` 帧）：

| 状态 | 触发 | 指示灯色 | 文字 | tooltip |
|------|------|--------|------|--------|
| 未知 | 启动默认 / 串口断开后 | `#888888`（MutedText） | `MCU: 未知` | （空） |
| 已就绪 | 收到 `0x0B 0x00` (BOOT_OK) | `#639922`（ConnectedIndicator） | `MCU: 已就绪` | `STM32 就绪：全部模块初始化完成` |
| 初始化失败 | 收到 `0x0B 0x01~0x05` 或保留段 | `#E24B4A`（DisconnectedIndicator） | `MCU: 初始化失败` | 对应 `bootStatusDescription`（如"I2C2（电机 IC 总线）初始化失败：请检查 PB10/PB11"） |

收到 `INIT_FAIL_*` 时除徽章变红外，**同一会话弹一次 `QMessageBox::warning`** 提示用户处理硬件；标志位在串口断开时重置，使重连后若再次失败可再次弹窗。

### 侧边栏（Sidebar）

```
宽度: 150px（收缩时 0px，动画 200ms）
背景: #f0ede8
边框: 右侧 0.5px solid #ccc

Header:
  高度: 30px
  字体: 10px 大写
  收缩按钮: ‹ 符号，右对齐

内容区:
  内边距: 8px 10px
  各 select 宽度: 72px

底部连接按钮:
  高度: 32px
  背景: #EAF3DE
  边框: 0.5px solid #3B6D11
  圆角: 5px
  字体: 11px #3B6D11
```

### 寄存器读写页（RegisterRwTab）

#### 整体布局

```
[Sidebar 168px] | [主内容区]

主内容区（垂直 QVBoxLayout，无 QSplitter）:
  · RegisterTable（主表格，stretch=1，占满主内容区）
  · 常驻工具条（一行紧凑高度，右对齐 toggle 按钮「批量读写」）

批量读写面板：**独立 Qt::Tool 浮动窗口**（与示波器 ShowRegister / ShowGenerator 同款机制），
  · 默认隐藏，点击工具条按钮弹出；
  · 浮窗可拖到主窗口外、独立缩放；
  · 关闭浮窗（窗口右上 × 或再点按钮）后按钮状态自动同步。

Sidebar 内容:
  - "全部读取" 按钮（高32px，绿色系）
  - "全部写入" 按钮（高32px，橙色系）
  - DEC / HEX 切换按钮组（互斥，切换值列显示模式，默认 HEX）

连接状态不影响 RegisterRwTab：所有控件始终可用（产品决策 2026-05-21，便于 UI 浏览与离线调试；按钮/单元格在串口未连接时点击操作会经 RegisterService → CommandDispatcher 自动落到本地错误回调，UI 显示 "--" / 写入失败）。
```

#### RegisterTable 表格组件

```
表格类型: QTableWidget
布局: 4 组并排，每组 5 列，共 20 列；每组 30 行
整体缩放: 紧凑 CSS 样式（小字号 + 小行高），不使用 QGraphicsProxyWidget

列结构（每组，重复 4 次）:
  描述列: ~110px，左对齐，可编辑文字，11px
  地址列: ~52px，居中，可编辑，monospace，格式 "0000"（4位十六进制，无前缀）
  值列:   ~56px，居中，可编辑（用于写入）/ 显示读取结果，monospace，蓝色 #185FA5
  读列:   ~26px，居中，QPushButton "读"，蓝色系
  写列:   ~23px，居中，QPushButton "写"，橙色系

行高: 19px
表头高度: 24px
表头背景: #f0ede8（SidebarBackground）
组分隔线: 列 5、10、15 左侧绘制 2px solid #aaa（通过自定义 ItemDelegate 实现）
单元格内边距: 0
```

#### 值列显示模式

| 模式 | 格式 | 示例 |
|------|------|------|
| HEX（默认） | "0x" + 4位大写十六进制 | "0x1A2B" |
| DEC | 有符号十进制整数 | "-1234" |

切换 DEC/HEX 时，已有值立即重新格式化显示，不改变底层数值。

#### 值列显示规则

| 状态 | 显示内容 | 颜色 |
|------|---------|------|
| 初始 / 地址为空 | 空白 | — |
| 读取成功（HEX）| "0xXXXX"（4位大写） | #185FA5 |
| 读取成功（DEC）| 有符号十进制 | #185FA5 |
| 读取失败 / 超时 | "--" | #E24B4A |
| 写入后 | 保持当前值显示 | #185FA5 |

#### 持久化

```
格式: JSON
路径: QStandardPaths::AppDataLocation / "registers.json"
触发: 描述、地址或值单元格内容变更后立即保存；批量读取完成后自动保存
内容: 4 组 × 30 行共 120 条记录，每条包含 {desc: string, addr: string, val: string}
启动时自动加载
```

#### 批量操作（全部读取 / 全部写入）

```
全部读取: 对所有地址非空的行，逐行顺序发送读命令（单次发出，等待响应后发下一条）
全部写入: 对地址和值均非空的行，逐行顺序发送写命令
顺序: globalRow 0 → 1 → ... → 79（组0行0 → 组0行19 → 组1行0 → ... → 组3行19）
错误处理: 单行失败（超时/错误响应）时标记 "--"，继续处理队列中下一行
```

#### 批量读写浮动窗口（独立 Qt::Tool 浮窗）

整体布局调整（2026-05-28 起，参考示波器 ShowRegister / ShowGenerator 同款机制）：
- 删除原 QSplitter 上下分屏 5:3:3（含两个空占位 GroupBox），不再保留占位框
- RegisterTable 占主内容区 stretch=1，始终占满主内容区
- RegisterTable 下方新增一行常驻工具条（紧凑高度 = `Style::Size::SidebarComboMinHeight`），右对齐放 toggle 按钮「批量读写」
- 批量读写面板**不再内嵌在主窗口**，改为独立 `Qt::Tool` 顶级浮动窗口弹出

常驻工具条 toggle 按钮：

```
控件: QToolButton (objectName: registerRwBatchToggleBtn)
属性: checkable=true，**默认 checked=false（浮窗默认隐藏）**
文字: 固定 "批量读写"（不随状态切换；checked 视觉由 QToolButton 自带样式表达）
高度: Style::Size::SidebarComboMinHeight (28)
位置: 工具条右对齐（左侧 addStretch）
```

点击行为：
- check → `m_batchPanel->show()` + `raise()` + `activateWindow()`
- uncheck → `m_batchPanel->hide()`
- 用户点浮窗右上角关闭按钮（×）→ 触发 `QEvent::Close`，RegisterRwTab 的 `eventFilter` 同步把按钮 `setChecked(false)`

批量读写浮窗：

```
容器: QWidget (objectName: registerRwBatchPanel)，parent=nullptr，windowFlags=Qt::Tool
windowTitle: "批量读写"
属性: Qt::WA_DeleteOnClose=false（手动管理生命周期；RegisterRwTab 析构时 delete）
初始尺寸: 600 × 220 px
默认状态: hide()，由 toggle 按钮控制显隐
事件过滤: RegisterRwTab::eventFilter 监听 QEvent::Close 同步按钮
内边距: Style::Size::ContentSpacing (16)
行间距: 10px
```

浮窗内含 4 行，每行 **3 列**：

| 列 | 控件 | 类型 | 说明 |
|---|------|------|------|
| 1 | 批量写入 / 批量读出 | QPushButton (96×32) | 点击触发该行的批量写入或读出操作 |
| 2 | 路径框 | QLineEdit (readOnly, stretch=1) | 显示选中的配置文件路径，由浏览对话框填入 |
| 3 | 浏览 | QPushButton (96×32) | 打开文件选择对话框，过滤器 `配置文件 (*.txt);;所有文件 (*.*)` |

4 个独立操作槽（数据流：配置文件 ↔ 芯片寄存器，同一配置文件既是写入源也是读出目标）：
- 第 1、2 行：「批量写入」配置文件 → 芯片（解析文件中的「地址+值」列表，逐条写入芯片寄存器）
- 第 3、4 行：「批量读出」芯片 → 配置文件（读取文件中地址列表对应的寄存器值，回写到该文件）

浮窗**底部**额外有一行状态文字 `QLabel`（`objectName=registerRwBatchStatusLabel`，字号 11px，颜色 `FwFlashStageLabelFg`）：实时显示当前任务进度、完成总结或错误原因；空闲时保持最后一次任务的结果文字（不主动清空）。

状态不持久化：每次启动应用，浮窗均处于隐藏状态、4 个路径框为空、状态文字为空；位置 / 尺寸不记忆。

#### 配置文件格式（写入 / 读出 共用）

参考 `register_read.txt` 同款格式：

```
//如下寄存器是AF方面的设置
0xB500, 0x1234  // 电流Machine
0xB408, 0x5678

//OIS
//0xB567, 0x4444
```

解析规则：
- 每条数据行：`<addr>, <value>`（逗号分隔，前后可有空白；地址 16-bit、值 16-bit，均必须为 `0x` 前缀的十六进制）
- `//` 起到行末视为注释；行首 `//` 整行注释、行末 `// xxx` 同行尾注释都跳过
- 空行解析时跳过、回写时一律删除
- 整行注释（包括被注释掉的数据行 `//0xB567, 0x4444`）在批量读出回写时**原样保留**
- 同一地址在文件中出现两次视为格式错误

#### 「批量写入」流程（文件 → 芯片）

1. 点「浏览」选择配置文件 → 路径填入路径框；状态文字保持空
2. 点「批量写入」按钮：
   - 若路径为空：状态文字 `"批量写入 #i：请先浏览选择配置文件"`，不发任何命令
   - 解析文件（**严格全或无**：任一异常立即拒绝整批）
   - 若解析失败：状态文字给出具体错误（见下表），不发任何命令
   - 解析成功：按文件顺序逐条调 `RegisterService::writeSingleRow`（0x21 协议）
3. **遇错即停**：单条写入失败 → 立即中止剩余条目，状态文字提示出错位置（已写入到芯片的部分无法回滚）
4. 全部成功 → 状态文字 `"批量写入 #i 完成：N/N 成功"`

#### 「批量读出」流程（芯片 → 文件）

1. 同上选文件
2. 点「批量读出」按钮：
   - 同上做文件解析校验
   - 解析成功：按文件顺序逐条调 `RegisterService::readSingleRow`（0x20 协议）
3. **遇错即停 + 全或无回写**（决策 #7）：单条读取失败 → 立即中止剩余条目；**原文件完全不修改**（即使前面部分已读到新值，也不回写）；状态文字提示出错位置
4. 全部成功 → 用新读到的值回写文件：
   - **保留原结构**：注释、整行注释、行序、行末注释、地址列文本完全不变
   - 仅替换数据行的「值」列为新读到的 `0x%04X` 大写格式
   - 输出时**统一删除所有空行**
5. 状态文字 `"批量读出 #i 完成：N/N 成功，已写回 <文件名>"`

#### 状态文字格式样例

| 场景 | 状态文字 |
|---|---|
| 启动 | `批量写入 #1 启动：12 条数据行` |
| 进行中 | `批量写入 #1：5/12 0xB500 = 0x1234 OK` |
| 完成 | `批量写入 #1 完成：12/12 成功` |
| 读出完成 | `批量读出 #3 完成：8/8 成功，已写回 register_read.txt` |
| 写入中止 | `批量写入 #1 中止：第 5/12 条 0xB500 写入失败` |
| 读出中止 | `批量读出 #3 中止：第 5/8 条 0xB500 读取失败（原文件未修改）` |
| 路径未选 | `批量写入 #1：请先浏览选择配置文件` |
| 解析错（格式）| `批量写入 #1：第 5 行格式错误（应为 \`<addr>, <value>\`）：xyz` |
| 解析错（地址越界）| `批量写入 #1：第 3 行地址越界（应在 0x0000~0xFFFF）：0x12345` |
| 解析错（值越界）| `批量写入 #1：第 7 行值越界（应在 0x0000~0xFFFF）：0x12345` |
| 解析错（重复地址）| `批量写入 #1：第 9 行与第 4 行地址重复：0xB500` |
| 解析错（空文件）| `批量写入 #1：文件无有效数据行（仅含注释或空行）` |
| 解析错（文件读不到）| `批量写入 #1：文件读取失败：<系统原因>` |

#### 互斥与按钮可用性

- **全局互斥**（决策 #8）：任意时刻最多 1 个批量/全量任务在跑
- 槽 #i 运行中 → 其他 3 个槽的按钮 + 4 个浏览按钮 + Sidebar 全部读取/全部写入 按钮**全部 disable**
- Sidebar 全部读取/全部写入 运行中 → 4 个槽的按钮 + 4 个浏览按钮**全部 disable**
- 任务结束（成功 / 失败 / 中止）后所有按钮恢复 enable
- **不提供取消按钮**（决策 #12）：任务一旦启动必须跑完或自然中止

#### 与 `RegisterTable` 主表格的关系

完全独立（决策 #11）：批量读写浮窗操作的对象是配置文件 + 芯片寄存器，**不读、不写 `RegisterTable`**；表格里的内容也不受批量操作影响。用户如需在主表格查看刚写入的值，需手动点 Sidebar「全部读取」回读。

#### 块读取浮动窗口（独立 Qt::Tool 浮窗，2026-05-28 新增）

入口：RegisterRwTab 主区域工具栏「批量读写」按钮**右侧**新增「块读取」QToolButton（checkable，再点关闭，与批量读写按钮同款风格）；两个按钮之间 10px 间距。

```
容器: QWidget (objectName: registerRwBlockReadPanel)，parent=nullptr，windowFlags=Qt::Tool
windowTitle: "块读取"
属性: Qt::WA_DeleteOnClose=false（手动管理生命周期；RegisterRwTab 析构时 delete）
初始尺寸: 520 × 240 px
默认状态: hide()，由 toggle 按钮控制显隐
事件过滤: RegisterRwTab::eventFilter 监听 QEvent::Close 同步按钮
内边距: Style::Size::ContentSpacing (16)
行间距: 10px
```

浮窗结构（垂直）：

```
┌── 块读取 ──────────────────────────────────────────┐
│ 起始地址：    [_____________]  (placeholder "0xB002 或 B002")
│ 寄存器个数：  [_____________]  (placeholder "十进制，例如 100")
│ 保存目录：    [_____________________________]  [浏览...]
│
│ [████████████░░░░░░░░░░░░] 50/100               (QProgressBar)
│ 状态: 块读取：50/100 0xB05A = 0xABCD OK            (QLabel)
│
│                          [开始读取]    [取消]
└────────────────────────────────────────────────────┘
```

字段说明：

| 字段 | 控件 | 说明 |
|---|---|---|
| 起始地址 | QLineEdit | 接受 `0xB002` / `B002` 两种 hex 格式 |
| 寄存器个数 | QLineEdit | 十进制 ≥ 1；地址按 +2 步进（与现有批量读写一致） |
| 保存目录 | QLineEdit（只读）+ QPushButton「浏览...」 | 浏览按钮调 `QFileDialog::getExistingDirectory`；初始值 = `Documents` |
| 进度条 | QProgressBar (0~N) | 实时显示已成功条目 / 总条目 |
| 状态文字 | QLabel (11px, FwFlashStageLabelFg) | 进度 / 完成 / 错误反馈 |
| 开始读取 | QPushButton (96×32) | 校验通过后启动任务；运行时禁用 |
| 取消 | QPushButton (96×32) | 协作式取消；空闲时禁用，仅 Reading 阶段启用 |

字段校验（决策 D9）：

| 错误 | 状态文字 |
|---|---|
| 起始地址格式错 | `起始地址格式错误（应为 0xXXXX 或 XXXX）` |
| 个数非正整数 | `寄存器个数必须为正整数` |
| 地址越界 | `地址范围越界：起始 0xFFF0 + 100 个寄存器超过 0xFFFE 上限` |
| 目录未选 | `请先选择保存目录` |

读取流程：
1. 用户填入起始地址 + 个数 + 目录 → 点「开始读取」
2. 校验通过后 `BlockReadService::start(startAddr, count, dir)` 启动
3. Service 内部按地址 +2 步进逐条调内部 `RegisterService::readSingleRow`（独立通道，与 RegisterTable / BatchRegisterService 隔离）
4. 成功每条 → 累积到 `m_results`，emit progress + stageMessage
5. **失败即停**（决策 D5）→ 立即收口：已读到的成功条目仍写入 CSV
6. **协作式取消**（决策 D6）→ 用户点取消 → 置 cancelFlag → 当前 in-flight 命令响应后立即收口；已读条目仍写入 CSV
7. 所有路径（完成 / 失败 / 取消）均走 `finishAndWriteCsv()`：状态 → WritingFile → QSaveFile 原子写 → emit finished

CSV 文件格式（决策 D4）：

```
addr,value\n
B002,1002\n
B004,ABCD\n
B006,0000\n
...
```

- 首行表头 `addr,value`（小写）
- 数据行 `XXXX,XXXX` — 4 位大写 hex，**无 `0x` 前缀**，逗号后**无空格**
- 行尾 LF（不是 CRLF）
- 文件命名 `Bulkread_HHMMSS.csv`（PC 本地时间，QTime 格式 `HHmmss`）
- 同秒冲突自动追加 `_N`（如 `Bulkread_172345_1.csv`），最多尝试 999 次

互斥规则（决策 D7）：

- **独立通道**：不与 Sidebar「全部读取/全部写入」、批量读写浮窗共享互斥位
- 块读取运行中，Sidebar 与批量读写按钮**仍可点击**；底层 SerialManager 命令队列串行执行
- 块读取自身也互斥：service 同时只能跑一个任务；同 service 重入会被 `isBusy()` 拒绝

持久化（决策 D10）：

- **仅记忆「上次保存目录」**（`m_blockReadLastDir`，会话内成员变量），关闭/打开浮窗保留
- 起始地址、个数字段每次空白
- 不持久化到磁盘

### 状态栏（StatusBar）

```
高度: 22px
背景: #3B6D11
文字: 11px #c0dd97

从左到右:
[软件信息 "MotorDev · MIT License"] [stretch]
[固件版本 "固件 v0.0.0 · 编译日期 2026-01-01"，居中] [stretch]
[日志面板开关按钮 "▼ 输出" / "▲ 输出"，点击切换 LogPanel 可见性]

日志面板开关按钮:
  高度: 18px
  背景: transparent
  文字: 11px #c0dd97
  hover: #EAF3DE
```

---

## 交互规范

### 串口连接流程

1. 用户在侧边栏选择串口参数
2. 点击"连接串口"按钮
3. 按钮变为"断开连接"，指示灯变绿，状态栏更新
4. 连接失败：弹出 QMessageBox 显示错误信息

### 寄存器读操作

1. 用户点击某行"读"按钮
2. 发送读指令帧
3. 等待响应（500ms 超时）
4. 收到数据：更新值列，正常值蓝色显示
5. 超时：值列显示"--"，状态栏提示超时

### 寄存器写操作

1. 用户在值列输入数值
2. 点击"写"按钮
3. 发送写指令帧
4. 等待 ACK（500ms 超时）
5. ACK OK：值列短暂高亮绿色
6. ACK ERR 或超时：值列高亮红色，状态栏提示

### 侧边栏收缩

1. 点击 ‹ 按钮
2. 侧边栏宽度从 150px 动画过渡至 0（200ms，QPropertyAnimation）
3. ContentArea 同步扩展填满
4. ‹ 变为 › 显示在活动栏边缘
5. 再次点击展开

---

## FW 烧录页面规格

```
整体布局（水平，无 Sidebar）:
[主内容区横向 QSplitter 两栏]

[左栏 stretch=1 (minWidth=280)：参数区]  | [右栏 stretch=1 (minWidth=280)：执行 / 输出区]

左栏（垂直）:
  ┌── 顶部紧凑 IC 选择行 ─────┐
  │  [目标 IC：] [下拉框 200px] [IC 描述文字]
  └────────────────────────────┘
  ┌── GroupBox "固件文件" ─────┐
  │  路径行 [QLineEdit][浏览][清空] │
  │  ── HLine 分隔线 ──            │
  │  内嵌 FwFileInfoPanel（QWidget）│
  │    · 空: "请先选择固件文件"    │
  │    · 合法: 字段网格            │
  │    · 错误: 红色错误信息        │
  └────────────────────────────┘
  (剩余空间留白)

右栏（垂直）:
  ┌── GroupBox "烧录控制" ─────┐
  │  [开始烧录][取消烧录] 阶段标签  │
  │  ▓▓▓▓▓░░░░░░░░░░░ 进度条        │
  └────────────────────────────┘
  ┌── GroupBox "操作日志" stretch=1 ┐
  │  [HH:mm:ss.zzz] [LEVEL] 消息    │
  │  ...                            │
  │  [清空]（右下）                 │
  └────────────────────────────────┘

QSplitter 把手宽度: Style::Size::SplitterHandleWidth (8px)
QSplitter 方向: 水平；首次显示比例 1:1（用户可拖动手柄调整后比例保持）；childrenCollapsible=false；左右栏 minWidth=280
连接状态不影响 FwFlashTab：所有控件始终可用（产品决策 2026-05-21）。"开始烧录"按钮的可用性由 IC 已选择 + 固件文件有效 + 当前不忙 共同决定，与串口连接状态解耦
```

### 卡片视觉

4 张 GroupBox 均使用与 ConfigTab 一致的卡片风格：
- `QGraphicsDropShadowEffect`：offset (0,1)、blur 6、color `Style::Color::PanelShadow`
- `WA_Hover` + `GroupHoverFilter` → 悬停时设置 `hovered` 动态属性，QSS 可重涂边框/阴影

### 无 Sidebar

不同于其他 Tab，FwFlashTab 直接以左右两栏占满主内容区，不挂 `Sidebar` 容器。
"操作步骤说明"功能由文件信息卡片下方的提示文字 + 阶段标签自然替代。

### 顶部紧凑 IC 选择行（取消 GroupBox 卡片，节省垂直空间）

单行横向布局，不带 GroupBox：

```
[ 目标 IC： ] [QComboBox 200px] [描述文字（MutedText 11px，stretch=1）]
```

- 标签 "目标 IC：" 11px 常规字体
- IC 下拉框宽度 `Style::Size::FwFlashIcComboW = 200`，默认显示 `请选择目标 IC...`
- 后续显示选中策略的 `icDescription()`（`Style::Color::MutedText`，11px），吃掉行内剩余空间
- 下拉框列表按 `FlashStrategyRegistry` 注册顺序：`AW86006` / `AW86100` / `DW9786` / `DW9788`

### 卡片"固件文件"

两段式：

```
[QLineEdit 只读 stretch] [浏览...] [清空]
─────── HLine 分隔线 ───────
FwFileInfoPanel（内嵌 QWidget，QStackedWidget 三页）
```

文件过滤器：`Firmware Files (*.bin *.hex);;Binary (*.bin);;Intel HEX (*.hex)`
解析触发：点击「浏览...」选定后立即调用 `FirmwareParser::parseFile()`

`FwFileInfoPanel` 三页：

| 状态 | 内容 |
|------|------|
| 空 | 单行 `请先选择固件文件`（`Style::Color::MutedText`） |
| 合法 | 网格 2 列：标签（11px `#666`）/ 值（11px monospace `#333`），可选中复制 |
| 错误 | 单行 `Style::Color::FwFlashFileInfoErrorFg` 文字 `解析失败：<原因>` |

合法状态字段（`.hex` 时多 3 行，`.bin` 时隐藏）：

| 字段 | 来源 |
|---|---|
| 文件名 | `FirmwareInfo::fileName` |
| 文件大小 | `<bytes> 字节 (<KB> KB)` |
| 文件格式 | `Binary` / `Intel HEX` |
| CRC32 | `0xXXXXXXXX`（基于合并后 `data`） |
| 段数（仅 .hex） | `segments.size()` |
| 地址范围（仅 .hex） | `0xXXXXXXXX - 0xYYYYYYYY` |
| 有效字节（仅 .hex） | `effectiveBytes` |

### 卡片"烧录控制"（FwFlashControlPanel）

紧凑两行布局：

```
[ 开始烧录（120×32） ] [ 取消烧录（100×32） ]  阶段标签 11px stretch
[ QProgressBar 高 18px，0~100，textVisible，主绿色 ]
```

阶段标签与按钮同行，靠左与按钮间留 8px 间距，`stretch=1` 吃掉剩余空间。

阶段标签文本由 FwFlashService 推送：
- `空闲`
- `准备中...`
- `停止采样...` / `停止信号发生器...` / `停止循环写入...` / `关闭 PMIC...`
- `烧录中 12.3 KB / 60.0 KB (20.5%)`
- `烧录完成` / `失败：<原因>` / `已取消`

### 卡片"操作日志"（FwFlashLogPanel，右栏 stretch=1）

```
[ QPlainTextEdit 只读，monospace 11px，maximumBlockCount=2000 ]
[ 右下角「清空」按钮 ]
```

行格式：`[HH:mm:ss.zzz] [LEVEL] <内容>`，按级别上色：

| LEVEL | 颜色常量 |
|---|---|
| `INFO ` | `Style::Color::FwFlashLogInfoFg` |
| `WARN ` | `Style::Color::FwFlashLogWarnFg` |
| `ERROR` | `Style::Color::FwFlashLogErrorFg` |
| `OK   ` | `Style::Color::FwFlashLogOkFg` |

仅当滚动条已在底部时新日志才自动滚到底部，避免打断用户阅读历史（与全局 `LogPanel` 行为一致）。

### 视觉常量（已加入 `style_constants.h`）

```
Style::Color:: FwFlashFileInfoLabelFg / FwFlashFileInfoValueFg / FwFlashFileInfoErrorFg
                FwFlashStageLabelFg
                FwFlashLogInfoFg / FwFlashLogWarnFg / FwFlashLogErrorFg / FwFlashLogOkFg
Style::Size::  FwFlashIcComboW=200
                FwFlashStartButtonW=120 / FwFlashStartButtonH=32
                FwFlashCancelButtonW=100 / FwFlashCancelButtonH=32
                FwFlashProgressH=18
```

### 烧录前置序列（产品决策）

点击「开始烧录」后，FwFlashService 在主线程依次触发：

1. 停止采样（fire-and-forget）
2. 停止信号发生器（fire-and-forget）
3. 停止循环写入（fire-and-forget）
4. 关闭 PMIC（fire-and-forget）

任一步失败仅写 `WARN` 日志，**不阻断**烧录调用。烧录完成 / 失败 / 取消后**不**自动恢复采样、发生器、循环写入或 PMIC，由用户决定是否手动重启。

不弹二次确认弹窗。

### 烧录函数留空架构

每款 IC 一个 `FlashStrategy` 子类，`flash()` 函数体留空（当前为 1 KB / 100 ms 的联调用模拟实现），由用户后续填入真实烧录算法。AW86100 与 AW86006 烧录算法等同，因此 `AW86100Strategy` 直接继承 `AW86006Strategy`，仅 override `icModel()` / `icDescription()`。

新增 IC 流程：在 `src/services/flashstrategies/` 下新增子类 + 在 `FlashStrategyRegistry::registerBuiltins()` 加一行 `add(...)` 即可，UI 自动列出。

---

## 示波器页面规格

> 数据流已接入，支持 8 通道 60fps 实时渲染。示波器工具栏控件已迁移至 TopBar（仅示波器页面可见），底部面板信号部分为 stub。

```
整体布局（水平）:
[Sidebar 224px（默认收起）] | [主内容区]

主内容区（垂直）:
1. ScopePlotWidget（波形绘制画布，stretch=1）
2. ScopeBottomPanel（底部面板）

注：原 ScopeToolBar 已移除。视图模式切换、Style、采样启停按钮现位于 TopBar，
仅在示波器页面可见，由 MainWindow 在页面切换时控制显隐。
```

### TopBar 示波器控件

```
位置: TopBar 中 connectionLabel 之后、弹性空白之前
仅在示波器页面可见（其他页面隐藏）

控件（从左到右）:
[Overlay/Stacked] [Style] [Crosshair toggle]

Overlay/Stacked: QToolButton，单按钮点击切换，文字显示当前模式
Style: QToolButton，checkable，切换通道样式面板
Crosshair toggle: QToolButton，checkable；文字在「使用十字光标：开启/关闭」之间切换；
                 启用后绘图区显示垂直虚线 + 吸附最近样本的圆点 + 数值标签框

注：采样启停按钮已迁至 ScopePlotWidget 绘图区右上角内嵌位置（见下文），不再位于 TopBar
注：原 STOPPED/RUNNING 状态标签已移除，采样状态通过内嵌采样按钮的文字与样式区分
```

### ScopeStylePanel（通道样式面板）

```
触发方式: TopBar 中 Style 按钮（checkable），点击弹出/关闭
位置: 锚定在 ScopePlotWidget 右上角下方，与 Style 按钮对齐
宽度: 200px
背景: Style::Color::ScopePanelBackground

内容:
  标题行: "Channel Styles" (10px bold)
  每通道一行 (CH1~CH8):
    [颜色按钮 20×20px] [CHn 标签 30px] [线宽 QDoubleSpinBox 92px] [线型 QToolButton 52px] [数据点 QToolButton 52px]
  底部: [Default Settings 按钮]

线型选项: Solid / Dash / Dot / SolidDot
数据点选项: On / Off
线宽范围: 0.5 ~ 8.0, 步进 0.5
```

### ScopePreviewWidget（通道波形预览）

```
位置: ScopeStylePanel 中每个通道行的波形预览区域
用途: 展示当前通道的线型/线宽/颜色效果预览
绘制: QPainter 自绘制，绘制一段正弦波示例
```

### ScopePlotWidget（波形绘制画布）

```
最小尺寸: 720×320px
绘制方式: QOpenGLWidget + QPainter（GL 后端，cosmetic 画笔，零堆分配 paintEvent）
背景: #f4f2ed
绘图区: 圆角矩形，内部网格 10×8

视图模式:
  Overlay — 所有通道波形叠加在同一绘图区，右上角显示图例
  Stacked — 每通道独立 lane，lane 间隔 10px

交互:
  鼠标左键拖拽 — 水平方向选区缩放 X 轴 / 垂直方向选区缩放 Y 轴
  双击 — 切换全屏（独立无边框窗口；Esc 退出）
  右键 — 弹出菜单（含重置视图）
  十字光标（由 TopBar Crosshair 按钮启用）— 鼠标移动时显示垂直虚线、吸附最近样本圆点、数值标签

内嵌采样启停按钮:
  位置: 绘图区右上角
  控件: QPushButton，文字随状态在「开始采样 / 停止采样」之间切换
  样式: 由 Style::Color::ScopeStatusRunning* / ScopeStatusStopped* 系列驱动
  尺寸常量: ScopeSamplingButtonMinW=92, ScopeSamplingButtonMinH=24

默认通道颜色（定义在 style_constants.h）:
  CH1 Speed   rgb(255, 90, 90)    // bright red — ScopeWaveCh1
  CH2 Torque  rgb(90, 220, 90)    // bright green — ScopeWaveCh2
  CH3 Temp    rgb(90, 170, 255)   // bright blue — ScopeWaveCh3
  CH4 Current rgb(255, 220, 80)   // bright yellow — ScopeWaveCh4
  CH5         rgb(255, 90, 230)   // magenta — ScopeWaveCh5
  CH6         rgb(90, 230, 230)   // cyan — ScopeWaveCh6
  CH7         rgb(255, 160, 60)   // orange — ScopeWaveCh7
  CH8         rgb(220, 220, 220)  // light grey — ScopeWaveCh8
```

### ScopeBottomPanel（底部面板）

```
背景: #f1eee8
边框: 顶部 1px solid #d8d1c7

控制行（顶部，横向排列）:
  [Sample Interval 标签] [QComboBox: 150 us / 250 us / 300 us / 400 us / 500 us / 900 us / 1000 us]
    （选项与 protocol.md 4.4 节 0x52 索引表 0x00~0x06 严格对应，由 SamplingConfig::intervalLabels() 提供）
  [Y Axis QToolButton（下拉菜单: Auto / Manual...）]
  [Display Window 标签] [QComboBox: 50 ms / 200 ms / 500 ms / 1000 ms / 2000 ms / 4000 ms]
  [Record Dir 标签] [QLineEdit 路径框（占满右侧）] [浏览… 按钮] [打开 按钮]
    （数据记录目录，替代原 Capture Note；QFileDialog 选目录，QSettings 跨会话持久化；
     采样启动即把全速率原始采样写入 `<dir>/Scope_YYYYMMDD_HHMMSS.csv`，停止即停；
     目录未设/无效则不录制，仅在日志面板提示；
     "打开"按钮：用 Excel（Windows `start excel`）打开记录目录下最新的 Scope_*.csv，无文件则日志提示）
  注：采样启停按钮位于 ScopePlotWidget 绘图区内嵌位置，不在底部面板亦不在 TopBar
  底部按钮行（右对齐）: [Hide/Show Channels] [Hide/Show Register] [Hide/Show Generator] 三个切换按钮

通道配置条区域（可通过 Hide/Show Channels 按钮折叠）:
  ScopeChannelStrip × 8，水平排列
  每条包含: CheckBox "CHn" + 描述 QLineEdit + 寄存器地址 QLineEdit
  最小宽度: 104px
  CH1~CH4 默认勾选

寄存器面板（ScopeRegisterPanel）:
  以独立浮动窗口弹出（Qt::Tool），500×400px
  8 行 R/W：每行 [描述] [地址] [值] [R] [W]
  值列输入：进制由 0x 前缀决定（0x→HEX，否则有符号 DEC，范围 -32768~32767）；
    非法值在该值框显示红色边框（editingFinished / 写入 / 切换时校验）
  底部: 下发时间间隔 + 启动/停止/清除面板/[HEX/DEC] 切换按钮
    - HEX/DEC 按钮（替代原"录入参数"）：切换值列显示进制，默认 HEX，按钮文案显示当前模式；
      切换时合法值立即重格式化（与主寄存器页口径一致），非法值保留原文并标红
    - 清除面板：清空 8 行的描述/地址/值及错误状态
  点击 Hide/Show Register 按钮切换显示

信号发生器面板（ScopeGeneratorPanel）:
  以独立浮动窗口弹出（Qt::Tool），最小 420×340px（Style::Size::GeneratorPanelMinW / MinH）
  通过 QStackedWidget + QButtonGroup 切换三种模式：
    - Linear   — 参数: 目标地址、min、max、step、间隔(ms)；对应协议 0x55
    - Cosine   — 参数: 振幅、偏移、频率(Hz)、最多 3 通道（地址 + 相位°）；对应协议 0x56
    - Sawtooth — 参数: 目标地址、min、max、step（每 tick 写一次，无 interval 分频）；对应协议 0x58
                 用于链路完整性测试：配合采样读取同一寄存器，肉眼可验证示波器是否完整还原信号
  统一启动/停止按钮；启动前对所有字段做格式校验，错误字段显示红色边框
  波形由 STM32 本地执行，上位机仅下发参数与启停命令
  点击 Hide/Show Generator 按钮切换显示
```

### 示波器 Sidebar

```
宽度: 224px（默认收起）
标题: "示波"

当前状态: 未实现（占位，暂无内容）
保留作为未来采样通道属性配置入口，具体内容待规格细化后补充。
```

---

## STM32 FLASH 文件存储页面规格

> 与 §"FW 烧录页面规格"（AW IC ISP 烧录）**完全独立**的功能：把任意本地文件上传到 STM32 内置 Flash Sector 5-11（`[0x08020000, 0x08100000)`，896 KB）暂存，另一台 PC 再下载。下载字节与原始上传文件 **1:1 无损**，改扩展名即可复原。单插槽覆盖模型：每次上传整区擦除（~3-7s 阻塞）。协议 v2.11 / 0x39~0x3F。

```
整体布局（垂直，无 Sidebar）:
[主内容区垂直 QVBoxLayout]

  ┌── 容量行 ────────────────────────────────────────────────┐
  │  容量：已用 N / 总 M | 剩余 K        [刷新容量]          │
  └──────────────────────────────────────────────────────────┘
  ┌── 操作行 ────────────────────────────────────────────────┐
  │  [上传文件...] [下载到本地...] [取消] [清空 FLASH]  阶段 │
  └──────────────────────────────────────────────────────────┘
  ┌── 进度条 ────────────────────────────────────────────────┐
  │  ▓▓▓▓▓░░░░░░░░░░░░░░░░░░  0~100%（绿色渐变 chunk）        │
  └──────────────────────────────────────────────────────────┘
  ┌── 操作日志面板（FwFlashLogPanel 复用，stretch=1）─────────┐
  │  [HH:mm:ss.zzz] [LEVEL] 消息                              │
  │  ...                                                      │
  │  [清空]（右下）                                           │
  └──────────────────────────────────────────────────────────┘

外边距: Style::Size::ContentPadding (24px)
行间距: Style::Size::ContentSpacing (16px)
按钮间距: 8px

连接状态不影响 FlashStorageTab：所有控件始终可用（产品决策 2026-05-21）。
按钮可用性仅由 `FlashStoreService::isBusy()` 决定：忙碌时操作按钮 disable、取消按钮 enable；空闲时反之。
```

### 容量行

```
[QLabel 容量文本 stretch=1] [QPushButton 刷新容量]

QLabel 文字格式:
  · 未刷新:        "容量：未知（点击刷新查询）"
  · 已刷新:        "容量：已用 X.XX KB / 总 Y.YY KB | 剩余 Z.ZZ KB"
  · 字段文字色:    Style::Color::FwFlashStageLabelFg
  · 单位自动选择:  < 1KB → B；< 1MB → KB；≥ 1MB → MB（QString::number(v, 'f', 2)）

QPushButton:
  · 文字: "刷新容量"
  · 触发协议命令: 0x3E INFO（无载荷，响应 8B totalCapacity + usedSize 小端）
  · 总容量固定为 917488 字节（= 896 KB - 16 B 元数据）
  · 进入任何任务时 disable，任务完成自动 enable
```

### 操作行

```
[上传文件... 120×32] [下载到本地... 120×32] [取消 100×32] [清空 FLASH 120×32]  [阶段标签 stretch]

按钮尺寸（复用 FwFlash 常量）:
  · 上传 / 下载 / 清空: Style::Size::FwFlashStartButtonW (120) × FwFlashStartButtonH (32)
  · 取消:               Style::Size::FwFlashCancelButtonW (100) × FwFlashCancelButtonH (32)

按钮行为:
  · 上传文件...: QFileDialog::getOpenFileName，过滤器 "All Files (*.*)"，起始目录 Documents；
                 选定后立即客户端校验文件大小（1 ≤ size ≤ 917488），失败仅在日志面板写 Error
                 不弹窗；通过校验则调 FlashStoreService::startWrite(path)，对应协议
                 0x39 BEGIN → 0x3A DATA × N → 0x3B END
  · 下载到本地...: QFileDialog::getExistingDirectory 选目录；保存文件名约定
                 "FLASH_<HHmmss>.txt"（PC 本地时间；同秒冲突追加 "_N"）；
                 对应协议 0x3C READ_BEGIN → 0x3D READ_DATA × N，落盘后本地 CRC32 校验
  · 取消:        调 FlashStoreService::cancel()；NOR Flash 物理擦除（0x39 / 0x3F 期间
                 ~3-7s）无法中途中断，UI 上的取消在 WIPE / 整区擦阶段为提示性
  · 清空 FLASH:  弹 QMessageBox::warning 二次确认（详见下文），通过后调 startWipe()
                 → 协议 0x3F WIPE 单帧，成功后自动 emit infoUpdated(917488, 0) 刷新容量行

阶段标签:
  · 字体: 11px，颜色 Style::Color::FwFlashStageLabelFg
  · 文本由 FlashStoreService 通过 stageMessage 信号推送，例如:
    "空闲" / "准备中..." / "整区擦除中..." / "上传 50% (456.0 KB / 896.0 KB)" /
    "下载中..." / "正在清空 FLASH..." / "完成" / "失败：<原因>" / "已取消"
```

### 进度条

```
[QProgressBar 高 18px，range 0~100，textVisible，居中对齐]

样式（与 FwFlashControlPanel 进度条完全一致）:
  · border: 1px solid Style::Color::DefaultBorder
  · border-radius: 5px
  · background:  Style::Color::WindowBackground
  · color:       Style::Color::AppText
  · chunk:       qlineargradient 从 Style::Color::FwFlashProgressChunkStart 到
                 FwFlashProgressChunkEnd，radius 4px，margin 1px

进度来源:
  · 上传: 已发字节 / totalBytes × 100
  · 下载: 已接字节 / metadataSize × 100
  · WIPE / INFO: 单帧操作，不更新进度条（保持 0 或最后值）
```

### 操作日志面板

```
复用 FwFlashLogPanel 组件（QPlainTextEdit 只读，monospace 11px，maximumBlockCount=2000）
行格式: [HH:mm:ss.zzz] [LEVEL] <内容>，按级别上色（Info / Warn / Error / Ok）
仅当滚动条已在底部时新日志才自动滚到底部
```

### 「清空 FLASH」二次确认弹窗

```
QMessageBox::warning(parent=this,
                     title="确认清空",
                     text="此操作将清空 STM32 上的 Flash 文件存储区（896 KB），\n
                           且**不可恢复**。\n\n
                           确定继续吗？",
                     buttons=Yes | No,
                     defaultButton=No)   ← 默认 No 防 Enter 键误触发

仅 Yes 时调用 FlashStoreService::startWipe()，对应协议 0x3F WIPE（~3-7s 阻塞，
心跳暂停/恢复由 FlashStoreService 自动处理）。
```

### 视觉常量（复用 `style_constants.h`，不引入新常量）

```
Style::Color::  FwFlashStageLabelFg (容量行 / 阶段标签文字)
                DefaultBorder / WindowBackground / AppText (进度条边框 / 背景 / 文字)
                FwFlashProgressChunkStart / End (进度条渐变 chunk 颜色)
                FwFlashLogInfoFg / WarnFg / ErrorFg / OkFg (日志 4 级颜色)
Style::Size::   ContentPadding (24) / ContentSpacing (16)
                FwFlashStartButtonW (120) / FwFlashStartButtonH (32)
                FwFlashCancelButtonW (100) / FwFlashCancelButtonH (32)
                FwFlashProgressH (18)
```

> 与 FwFlash 完全复用，避免引入并行常量；后续如果调色板分化，再拆出独立 `FlashStore*` 命名空间。

### 操作流程

#### 上传文件

1. 用户点「上传文件...」选定本地文件
2. 客户端校验 1 ≤ size ≤ 917488；超限只在日志面板写 Error
3. 通过校验后 FlashStoreService 心跳暂停 → 0x39 BEGIN（含整区擦 ~3-7s 阻塞）
4. 0x3A DATA 分包送（每包 252 字节，UI 实时刷新进度条与阶段文字）
5. 0x3B WRITE_END 提交并校验 CRC（PC 端预先对原始文件算的 CRC32 比对 STM32 累计 CRC）
6. CRC 匹配 → 成功；不匹配 → 元数据不写入，slot 仍为 EMPTY，日志写 Error
7. 任意结果 → 心跳恢复，容量行自动刷新

#### 下载到本地

1. 用户点「下载到本地...」选定目标目录
2. FlashStoreService 心跳暂停 → 0x3C READ_BEGIN 拿 size + crc32
3. 0x3D READ_DATA × N 分包读取（每包 252 字节，UI 实时刷新进度条）
4. 落盘到 `<dir>/FLASH_<HHmmss>.txt`（同秒冲突追加 `_N`）
5. 本地对落盘文件算 CRC32 比对 0x3C 拿到的 crc32；不一致 → 日志写 Warn 提示用户文件可能受损
6. 日志写 OK 提示完整路径："已保存到：<path>（改回原扩展名即可使用）"
7. 心跳恢复

#### 清空 FLASH

1. 用户点「清空 FLASH」→ QMessageBox 二次确认（默认 No）
2. 用户确认 Yes → FlashStoreService 心跳暂停 → 0x3F WIPE 单帧（~3-7s 阻塞）
3. STM32 整区擦除 Sector 5-11（含元数据 16 B），不可取消
4. 响应到达后 service 自动 emit infoUpdated(917488, 0)，容量行刷新为"已用 0 / 总 896 KB | 剩余 896 KB"
5. 心跳恢复

#### 刷新容量

1. 用户点「刷新容量」→ 0x3E INFO 单帧（微秒级）
2. 响应后容量行更新

### 无 Sidebar

不同于 RegisterRwTab / OscilloscopTab，FlashStorageTab 直接以单栏占满主内容区，不挂 `Sidebar` 容器（与 FwFlashTab / ConfigTab 一致）。

---

## Logo 规格

```
文件: motordev_logo.svg
画布: 200×200px

元素:
- 背景矩形: fill=#0f1f0f, rx=28, stroke=#639922 2px
- 红色阶跃线: stroke=#E24B4A, stroke-width=1.25
  路径: M18,115 L55,115 L55,72 L155,72
- 黄色跟随线: stroke=#f5e642, stroke-width=1.25
  路径: M18,115 C50,115 54,115 55,115 C57,110 61,72 65,61 C68,57 74,66 84,70 C100,73 130,72 155,72
- 左菊花: translate(38,162)，8片椭圆花瓣 + 花心 + 茎叶
- 右菊花: translate(162,162)，同上镜像
- 文字: MOTORDEV，x=100 y=168，fill=#97C459，font-size=13，letter-spacing=2

菊花规格:
- 花瓣: 8片 ellipse rx=2 ry=4 fill=#f5e642，每45°一片
- 花心外圈: circle r=3 fill=#c8960a
- 花心内圈: circle r=1.5 fill=#e8b020
- 茎: path Q曲线，stroke=#3B6D11 1.5px
- 叶片: 两条 path Q曲线，stroke=#3B6D11 1.2px
```

---

## 多语言对照表（核心字符串）

| Key | 中文 | English | Русский | 日本語 |
|-----|------|---------|---------|--------|
| app.title | MotorDev | MotorDev | MotorDev | MotorDev |
| sidebar.config | 配置 | Config | Настройки | 設定 |
| sidebar.port | 端口 | Port | Порт | ポート |
| sidebar.baudrate | 波特率 | Baud Rate | Скорость | ボーレート |
| sidebar.connect | 连接串口 | Connect | Подключить | 接続 |
| sidebar.disconnect | 断开连接 | Disconnect | Отключить | 切断 |
| tab.register | 读写 | R/W | Рег | 読書 |
| tab.flash | 烧录 | Flash | Прошивка | 書込 |
| tab.scope | 示波 | Scope | Осциллограф | 波形 |
| table.desc | 描述 | Desc | Описание | 説明 |
| table.addr | 地址 | Addr | Адрес | アドレス |
| table.value | 值 | Value | Значение | 値 |
| table.read | 读 | R | Чт | 読 |
| table.write | 写 | W | Зп | 書 |
| btn.readall | 全部读取 | Read All | Читать всё | 全読込 |
| btn.addrow | + 添加行 | + Add Row | + Добавить | + 追加 |
| status.ready | 就绪 | Ready | Готово | 準備完了 |
| status.connected | 已连接 | Connected | Подключено | 接続済 |
| status.disconnected | 未连接 | Disconnected | Отключено | 未接続 |

---

*本文档随开发进展持续更新。协议帧格式确认后需同步更新 claude.md 中的协议部分。*
