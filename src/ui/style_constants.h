// =============================================================================
// @file    style_constants.h
// @brief   全局样式常量定义（颜色、尺寸、字体、串口参数）
//
// 本文件集中管理所有 UI 组件的颜色值、尺寸值、字体名称和串口配置参数。
// 禁止在其他文件中硬编码颜色或尺寸，所有设计值必须引用本文件中的具名常量。
// 修改本文件会影响全局外观，需评估全局影响。
// =============================================================================

#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

namespace MotorDev::Style {

// ---------------------------------------------------------------------------
// 颜色常量
// ---------------------------------------------------------------------------
namespace Color {

// --- 品牌色 / 主题色 ---
inline const QColor PrimaryGreen("#3B6D11");         ///< 主色调：深绿色，用于品牌标识和主要按钮
inline const QColor LightGreen("#EAF3DE");           ///< 浅绿色背景，用于高亮区域
inline const QColor BorderGreen("#c0dd97");           ///< 绿色边框，用于已选中/激活状态
inline const QColor TextGreen("#97C459");             ///< 绿色文字，用于状态栏等信息展示
inline const QColor StatusBarText("#c0dd97");          ///< 状态栏文字颜色

// --- 窗口与面板背景 ---
inline const QColor WindowBackground("#f5f5f3");      ///< 主窗口背景色
inline const QColor ActivityBarBackground("#e8e5e0");  ///< 左侧活动栏背景色
inline const QColor SidebarBackground("#f0ede8");      ///< 侧边栏背景色
inline const QColor TopBarBackground("#f8f7f5");       ///< 顶栏背景色
inline const QColor PanelBackground("#ffffff");        ///< 通用面板/卡片背景色
inline const QColor White("#ffffff");                  ///< 纯白色
inline const QColor Transparent("transparent");        ///< 透明色

// --- 连接状态指示 ---
inline const QColor DisconnectedIndicator("#E24B4A");  ///< 串口断开状态：红色指示灯
inline const QColor ConnectedIndicator("#639922");     ///< 串口已连接状态：绿色指示灯

// --- 边框 ---
inline const QColor DefaultBorder("#cccccc");          ///< 默认边框色
inline const QColor InputBorder("#dddddd");            ///< 输入框边框色
inline const QColor TableRowBorder("#eeeeee");         ///< 表格行分隔线颜色

// --- 文字颜色 ---
inline const QColor AppText("#2c2c2a");                ///< 主文字颜色（深灰）
inline const QColor MutedText("#888888");              ///< 次要文字颜色（灰色）
inline const QColor SidebarLabelText("#666666");       ///< 侧边栏标签文字
inline const QColor TopBarLabelText("#aaaaaa");        ///< 顶栏标签文字（浅灰）
inline const QColor TopBarValueText("#555555");        ///< 顶栏数值文字

// --- 寄存器读写按钮 ---
inline const QColor ReadButtonBackground("#E6F1FB");   ///< 读按钮背景色（浅蓝）
inline const QColor ReadButtonForeground("#185FA5");   ///< 读按钮前景色（蓝色）
inline const QColor WriteButtonBackground("#FAEEDA");  ///< 写按钮背景色（浅黄）
inline const QColor WriteButtonForeground("#854F0B");  ///< 写按钮前景色（棕色）
inline const QColor RegisterValueText("#185FA5");      ///< 寄存器值文字颜色（蓝色）
inline const QColor RegisterErrorText("#E24B4A");      ///< 寄存器错误文字颜色（红色）

// --- 表格样式 ---
inline const QColor TableEvenRowBackground("#fafaf8"); ///< 表格偶数行背景色（斑马纹）
inline const QColor TableHoverRowBackground("#f0f7e8"); ///< 表格行悬浮高亮色
inline const QColor GroupDivider("#aaaaaa");            ///< 寄存器组分隔线颜色
inline const QColor TableHeaderBackground("#f0ede8");  ///< 表头背景色
inline const QColor TableHeaderText("#555555");        ///< 表头文字颜色

// --- 按钮状态 ---
inline const QColor PrimaryButtonHover("#D5E8C4");     ///< 主按钮悬浮色
inline const QColor DisabledBackground("#E6E6E6");     ///< 禁用状态背景色
inline const QColor DisabledBorder("#C9C9C9");         ///< 禁用状态边框色
inline const QColor DisabledText("#9A9A9A");           ///< 禁用状态文字色
inline const QColor PanelHoverBackground("#f5f5f2");   ///< 面板悬浮背景色
inline const QColor SecondaryButtonHover("#f0f0f0");   ///< 次要按钮悬浮色
inline const QColor SecondaryButtonPressed("#e0e0e0"); ///< 次要按钮按下色
inline const QColor PanelShadow(44, 44, 42, 38);      ///< 面板阴影色（RGBA，低透明度）

// --- 示波器：背景与边框 ---
inline const QColor ScopeBackground("#f4f2ed");        ///< 示波器整体背景色
inline const QColor ScopeFrameBorder("#d9d2c8");       ///< 示波器外框边框色
inline const QColor ScopeFrameBackground("#fbfaf8");   ///< 示波器绘图区框架背景色
inline const QColor ScopePanelBackground("#f8f6f1");   ///< 示波器面板背景色
inline const QColor ScopePanelBorder("#ddd6cb");       ///< 示波器面板边框色
inline const QColor ScopeSectionBackground("#fcfbf8"); ///< 示波器区段背景色
inline const QColor ScopeSectionBorder("#ddd6cb");     ///< 示波器区段边框色
inline const QColor ScopeOverlayBorder("#cfc7bb");     ///< 示波器叠加模式边框色
inline const QColor ScopeDivider("#d8d1c7");           ///< 示波器分隔线颜色
inline const QColor ScopeBottomBackground("#f1eee8");  ///< 示波器底部面板背景色

// --- 示波器：文字颜色 ---
inline const QColor ScopeText("#4d4942");              ///< 示波器主文字
inline const QColor ScopeTextMuted("#7d776f");         ///< 示波器次要文字
inline const QColor ScopeTextSecondary("#555049");     ///< 示波器辅助文字
inline const QColor ScopeTextDim("#8a867e");           ///< 示波器暗淡文字
inline const QColor ScopeTextLabel("#6d6a63");         ///< 示波器标签文字
inline const QColor ScopeTextSubtle("#7a736a");        ///< 示波器微弱文字
inline const QColor ScopeTextDark("#544f48");          ///< 示波器深色文字
inline const QColor ScopeTextChannel("#56514a");       ///< 示波器通道名文字
inline const QColor ScopeTextHeader("#57524b");        ///< 示波器标题文字
inline const QColor ScopeTextAlt("#45423c");           ///< 示波器备选文字

// --- 示波器：输入框 ---
inline const QColor ScopeInputBorder("#d5cec4");       ///< 示波器输入框边框色
inline const QColor ScopeInputBorderAlt("#d2ccc3");    ///< 示波器输入框备选边框色

// --- 示波器：工具按钮 ---
inline const QColor ScopeToolButtonBackground("#fbfaf8");       ///< 工具按钮默认背景
inline const QColor ScopeToolButtonBorder("#d5cec4");           ///< 工具按钮默认边框
inline const QColor ScopeToolButtonHover("#f2eee8");            ///< 工具按钮悬浮背景
inline const QColor ScopeToolButtonHoverBorder("#bbb2a6");      ///< 工具按钮悬浮边框
inline const QColor ScopeToolButtonPressed("#ebe5dc");          ///< 工具按钮按下背景
inline const QColor ScopeToolButtonChecked("#e5f0df");          ///< 工具按钮选中背景
inline const QColor ScopeToolButtonCheckedBorder("#9ab781");    ///< 工具按钮选中边框
inline const QColor ScopeToolButtonCheckedText("#35541c");      ///< 工具按钮选中文字
inline const QColor ScopeToolButtonDisabled("#f0ede7");         ///< 工具按钮禁用背景
inline const QColor ScopeToolButtonDisabledBorder("#ddd7ce");   ///< 工具按钮禁用边框
inline const QColor ScopeToolButtonDisabledText("#a7a097");     ///< 工具按钮禁用文字

// --- 示波器：采样状态标签 ---
inline const QColor ScopeStatusRunningBackground("#edf6e7");    ///< 运行中状态背景（绿色调）
inline const QColor ScopeStatusRunningText("#46702a");          ///< 运行中状态文字
inline const QColor ScopeStatusRunningBorder("#b8d2a6");        ///< 运行中状态边框
inline const QColor ScopeStatusStoppedBackground("#f7ece9");    ///< 已停止状态背景（红色调）
inline const QColor ScopeStatusStoppedText("#995548");          ///< 已停止状态文字
inline const QColor ScopeStatusStoppedBorder("#d8b0a5");        ///< 已停止状态边框

// --- 示波器：绘图区网格与图例 ---
inline const QColor ScopeGridMinor("#ebe4d9");         ///< 次网格线颜色
inline const QColor ScopeGridMajor("#d5ccbf");         ///< 主网格线颜色
inline const QColor ScopeLegendBackground("#f1ede6");  ///< 图例背景色
inline const QColor ScopeSelectionBorder("#7aa75a");   ///< 选区边框颜色（拖拽缩放）
inline const QColor ScopeSelectionFill(154, 183, 129, 48);  ///< 选区填充色（半透明绿）
inline const QColor ScopeStackedLaneBorder("#ddd6cb");  ///< 分离模式通道条带边框
inline const QColor ScopeCrosshairBoxBackground(30, 30, 30, 220);  ///< 十字光标读数 box 背景（半透明深灰）

// --- 示波器：通道配置条 ---
inline const QColor ScopeChannelBackground("#fbfaf8");        ///< 通道条背景色
inline const QColor ScopeChannelBorder("#d8d1c7");            ///< 通道条边框色
inline const QColor ScopeChannelInputBorder("#d5cec4");       ///< 通道条输入框边框
inline const QColor ScopeChannelInputFocus("#9ab781");        ///< 通道条输入框聚焦边框

// --- 示波器：复选框 ---
inline const QColor ScopeCheckboxUncheckedBorder("#bfb7ab");       ///< 复选框未选中边框
inline const QColor ScopeCheckboxCheckedBackground("#dcead0");     ///< 复选框选中背景
inline const QColor ScopeCheckboxCheckedBorder("#9ab781");         ///< 复选框选中边框

// --- 示波器：寄存器面板按钮 ---
inline const QColor ScopeRegReadBackground("#e8f1fa");    ///< 示波器侧读按钮背景
inline const QColor ScopeRegReadBorder("#bdd1e5");        ///< 示波器侧读按钮边框
inline const QColor ScopeRegWriteBackground("#f7eddd");   ///< 示波器侧写按钮背景
inline const QColor ScopeRegWriteBorder("#e1c89c");       ///< 示波器侧写按钮边框
inline const QColor ScopeRegClearBackground("#f2f0eb");   ///< 示波器侧清除按钮背景
inline const QColor ScopeRegClearBorder("#d5cec4");       ///< 示波器侧清除按钮边框
inline const QColor ScopeRegLoadBackground("#edf6e7");    ///< 示波器侧加载按钮背景
inline const QColor ScopeRegLoadBorder("#b8d2a6");        ///< 示波器侧加载按钮边框

// --- 示波器：切换按钮 ---
inline const QColor ScopeToggleBackground("#fbfaf8");     ///< 切换按钮默认背景
inline const QColor ScopeToggleText("#5a554d");           ///< 切换按钮文字
inline const QColor ScopeToggleBorder("#d5cec4");         ///< 切换按钮边框
inline const QColor ScopeToggleHover("#f2eee8");          ///< 切换按钮悬浮背景
inline const QColor ScopeToggleHoverBorder("#bbb2a6");    ///< 切换按钮悬浮边框

// --- 示波器：8 通道波形颜色 ---
inline const QColor ScopeWaveCh1(255,  90,  90);   ///< 通道 1：亮红色
inline const QColor ScopeWaveCh2( 90, 220,  90);   ///< 通道 2：亮绿色
inline const QColor ScopeWaveCh3( 90, 170, 255);   ///< 通道 3：亮蓝色
inline const QColor ScopeWaveCh4(255, 220,  80);   ///< 通道 4：亮黄色
inline const QColor ScopeWaveCh5(255,  90, 230);   ///< 通道 5：品红色
inline const QColor ScopeWaveCh6( 90, 230, 230);   ///< 通道 6：青色
inline const QColor ScopeWaveCh7(255, 160,  60);   ///< 通道 7：橙色
inline const QColor ScopeWaveCh8(220, 220, 220);   ///< 通道 8：浅灰色

// --- 日志面板文字颜色 ---
inline const QColor LogWarning("#B85C00");           ///< 警告日志文字颜色（橙色）
inline const QColor LogError("#C0392B");             ///< 错误日志文字颜色（红色）
inline const QColor LogInfo("#2E7D32");              ///< 信息日志文字颜色（绿色）

// --- 固件烧录页面 ---
inline const QColor FwFlashFileInfoLabelFg("#666666"); ///< 文件信息面板标签文字
inline const QColor FwFlashFileInfoValueFg("#333333"); ///< 文件信息面板值文字
inline const QColor FwFlashFileInfoErrorFg("#E24B4A"); ///< 文件信息面板错误文字
inline const QColor FwFlashStageLabelFg("#555555");    ///< 烧录阶段标签文字
inline const QColor FwFlashLogInfoFg("#333333");       ///< 烧录日志 INFO 文字
inline const QColor FwFlashLogWarnFg("#854F0B");       ///< 烧录日志 WARN 文字
inline const QColor FwFlashLogErrorFg("#E24B4A");      ///< 烧录日志 ERROR 文字
inline const QColor FwFlashLogOkFg("#639922");         ///< 烧录日志 OK 文字
inline const QColor FwFlashProgressChunkStart("#5BA02E"); ///< 烧录进度条 chunk 渐变起色（深绿）
inline const QColor FwFlashProgressChunkEnd("#97C459");   ///< 烧录进度条 chunk 渐变止色（浅绿）
}

// ---------------------------------------------------------------------------
// 尺寸常量（单位：像素，除非另有说明）
// ---------------------------------------------------------------------------
namespace Size {

// --- 窗口尺寸 ---
inline constexpr int WindowWidth = 1280;              ///< 默认窗口宽度
inline constexpr int WindowHeight = 800;              ///< 默认窗口高度
inline constexpr int MinWindowWidth = 1024;            ///< 最小窗口宽度
inline constexpr int MinWindowHeight = 600;            ///< 最小窗口高度

// --- 顶栏与状态栏 ---
inline constexpr int TopBarHeight = 36;               ///< 顶栏高度
inline constexpr int StatusBarHeight = 22;            ///< 状态栏高度

// --- 活动栏（左侧页面切换栏） ---
inline constexpr int ActivityBarWidth = 44;           ///< 活动栏宽度
inline constexpr int ActivityButtonSize = 34;         ///< 活动栏按钮尺寸（正方形）
inline constexpr int ActivityButtonRadius = 6;        ///< 活动栏按钮圆角半径
inline constexpr int ActivityBarTopPadding = 8;       ///< 活动栏顶部内边距
inline constexpr int ActivityBarBottomPadding = 8;    ///< 活动栏底部内边距
inline constexpr int ActivityBarSpacing = 6;          ///< 活动栏按钮间距
inline constexpr int ActivityButtonPadding = 4;       ///< 活动栏按钮内边距

// --- 侧边栏 ---
inline constexpr int SidebarWidth = 150;              ///< 侧边栏展开宽度
inline constexpr int SidebarCollapsedWidth = 0;       ///< 侧边栏收起宽度
inline constexpr int SidebarHandleWidth = 18;         ///< 侧边栏拖拽手柄宽度
inline constexpr int SidebarTotalWidth = SidebarWidth + SidebarHandleWidth;  ///< 侧边栏总宽度（含手柄）
inline constexpr int SidebarHeaderHeight = 30;        ///< 侧边栏标题栏高度
inline constexpr int SidebarButtonHeight = 32;        ///< 侧边栏按钮高度
inline constexpr int SidebarAnimationMs = 200;        ///< 侧边栏展开/收起动画时长（毫秒）
inline constexpr int SidebarContentHorizontalPadding = 10; ///< 侧边栏内容水平内边距
inline constexpr int SidebarContentVerticalPadding = 8;    ///< 侧边栏内容垂直内边距
inline constexpr int SidebarSectionSpacing = 8;       ///< 侧边栏区段间距
inline constexpr int SidebarComboMinHeight = 28;      ///< 侧边栏下拉框最小高度

// --- Logo 与指示灯 ---
inline constexpr int LogoSize = 22;                   ///< 顶栏 Logo 图标尺寸
inline constexpr int IndicatorSize = 7;               ///< 连接状态指示灯直径

// --- 顶栏示波器控件 ---
inline constexpr int TopBarScopeButtonMinH = 22;      ///< 顶栏示波器控件最小高度
inline constexpr int TopBarScopeButtonMaxH = 24;      ///< 顶栏示波器控件最大高度
inline constexpr int TopBarViewModeButtonMinW = 68;   ///< 顶栏视图模式按钮最小宽度
inline constexpr int TopBarStyleButtonMinW = 52;      ///< 顶栏样式按钮最小宽度
inline constexpr int TopBarCrosshairButtonMinW = 132; ///< 顶栏十字光标按钮最小宽度

// --- 通用布局 ---
inline constexpr int BorderThin = 1;                  ///< 细边框宽度
inline constexpr int OuterMargin = 0;                 ///< 外边距
inline constexpr int MainSpacing = 0;                 ///< 主布局间距
inline constexpr int HeaderHorizontalPadding = 12;    ///< 标题栏水平内边距
inline constexpr int LanguageComboWidth = 96;         ///< 语言切换下拉框宽度
inline constexpr int ContentPadding = 24;             ///< 内容区域内边距
inline constexpr int ContentSpacing = 16;             ///< 内容区域元素间距
inline constexpr int FormSpacing = 10;                ///< 表单元素间距
inline constexpr int SplitterHandleWidth = 8;         ///< QSplitter 拖拽手柄宽度
inline constexpr int GroupBoxTopMargin = 8;           ///< 分组框顶部外边距

// --- 模拟器 ---
inline constexpr int SimulatorLeftPanelWidth = 260;   ///< 模拟器左侧面板宽度

// --- 状态栏 ---
inline constexpr int StatusBarHorizontalPadding = 10; ///< 状态栏水平内边距
inline constexpr int StatusBarSpacing = 10;           ///< 状态栏元素间距

// --- 寄存器表格 ---
inline constexpr int TableRowHeight = 19;             ///< 表格行高
inline constexpr int TableHeaderHeight = 24;          ///< 表头高度
inline constexpr int TableGroupCount = 4;             ///< 寄存器组数量
inline constexpr int TableRowCount = 30;              ///< 每组寄存器行数（固定）
inline constexpr int TableGroupDividerWidth = 2;      ///< 组分隔线宽度
inline constexpr int ColDescWidth = 76;               ///< "描述"列宽度
inline constexpr int ColAddrWidth = 67;               ///< "地址"列宽度（与 ColValueWidth 一致，便于阅读 0xXXXX 地址）
inline constexpr int ColValueWidth = 67;              ///< "值"列宽度
inline constexpr int ColReadWidth = 26;               ///< "读"按钮列宽度
inline constexpr int ColWriteWidth = 23;              ///< "写"按钮列宽度

// --- 示波器样式面板 ---
inline constexpr int ScopeStylePanelWidth = 200;          ///< 样式面板宽度
inline constexpr int ScopeStylePanelMargin = 8;            ///< 样式面板外边距
inline constexpr int ScopeStylePanelRowSpacing = 2;        ///< 样式面板行间距
inline constexpr int ScopeStyleColorButtonSize = 20;       ///< 颜色选择按钮尺寸
inline constexpr int ScopeStyleChannelLabelWidth = 30;     ///< 通道标签宽度
inline constexpr int ScopeStyleWidthSpinWidth = 92;        ///< 线宽调节框宽度
inline constexpr int ScopeStyleToolButtonMinWidth = 52;    ///< 工具按钮最小宽度
inline constexpr int ScopeStyleInputMinHeight = 24;        ///< 输入控件最小高度
inline constexpr int ScopeStyleButtonRadius = 3;           ///< 样式面板按钮圆角
inline constexpr int ScopeStyleTitleFontPx = 10;           ///< 样式面板标题字号（px）
inline constexpr int ScopeStyleRowFontPx = 9;              ///< 样式面板行字号（px）

// --- 示波器底部面板 ---
inline constexpr int ScopeBottomPanelMinCollapsed = 40;    ///< 底部面板收起最小高度
inline constexpr int ScopeBottomPanelMinExpanded = 132;    ///< 底部面板展开最小高度
inline constexpr int ScopeBottomPanelMaxExpanded = 240;    ///< 底部面板展开最大高度
inline constexpr int ScopeIntervalComboMinW = 110;         ///< 底部面板采样间隔下拉框最小宽度
inline constexpr int ScopeYAxisButtonMinW = 132;           ///< 底部面板 Y 轴按钮最小宽度

// --- 示波器通道条 ---
inline constexpr int ScopeChannelStripMinW = 104;          ///< 单通道配置条最小宽度
inline constexpr int ScopeChannelStripRowH = 22;           ///< 通道条行高（min=max=22）

// --- 日志面板 ---
inline constexpr int LogPanelHeight = 200;            ///< 日志面板默认高度

// --- 示波器绘图 ---
inline constexpr int ScopePlotFontSmall = 8;          ///< 绘图区小号字体（px）
inline constexpr int ScopePlotFontNormal = 9;         ///< 绘图区常规字体（px）
inline constexpr int ScopePlotMinHeight = 320;        ///< 绘图区最小高度
inline constexpr int ScopePlotFramePadding = 8;       ///< 绘图区外框内边距
inline constexpr int ScopePlotAxisLeft = 58;          ///< Y 轴标签区域宽度（左边距）
inline constexpr int ScopePlotAxisTop = 14;           ///< 绘图区顶部边距
inline constexpr int ScopePlotAxisRight = 14;         ///< 绘图区右侧边距
inline constexpr int ScopePlotAxisBottom = 32;        ///< X 轴标签区域高度（底部边距）
inline constexpr int ScopePlotStackedGap = 10;        ///< 分离模式通道条带间距

// --- 跑马灯标签 ---
inline constexpr int MarqueeMinWidth = 140;           ///< 跑马灯标签最小宽度
inline constexpr int MarqueeMinHeight = 26;           ///< 跑马灯标签最小高度
inline constexpr int MarqueeFontSize = 10;            ///< 跑马灯字号（px）

// --- 寄存器表格字号 ---
inline constexpr int RegisterTableFontSize = 11;      ///< 寄存器表格字号（px）

// --- 示波器采样按钮 ---
inline constexpr int ScopeSamplingButtonMinW = 92;    ///< 采样启停按钮最小宽度
inline constexpr int ScopeSamplingButtonMinH = 24;    ///< 采样启停按钮最小高度

// --- 信号发生器面板 ---
inline constexpr int GeneratorPanelMinW = 420;        ///< 发生器面板最小宽度
inline constexpr int GeneratorPanelMinH = 340;        ///< 发生器面板最小高度

// --- 固件烧录页面 ---
inline constexpr int FwFlashIcComboW = 200;            ///< IC 型号下拉框最小宽度
inline constexpr int FwFlashStartButtonW = 120;        ///< 开始烧录按钮宽度
inline constexpr int FwFlashStartButtonH = 32;         ///< 开始烧录按钮高度
inline constexpr int FwFlashCancelButtonW = 100;       ///< 取消烧录按钮宽度
inline constexpr int FwFlashCancelButtonH = 32;        ///< 取消烧录按钮高度
inline constexpr int FwFlashProgressH = 18;            ///< 烧录进度条高度
}

// ---------------------------------------------------------------------------
// 字体常量
// ---------------------------------------------------------------------------
namespace Font {
inline const char *SansSerif = "Segoe UI";   ///< 无衬线字体（用于通用 UI 文字）
inline const char *Monospace = "Consolas";    ///< 等宽字体（用于寄存器值、日志等）
}

// ---------------------------------------------------------------------------
// 串口配置常量
// ---------------------------------------------------------------------------
namespace Serial {

/// @brief 获取支持的波特率选项列表
/// @return 波特率字符串列表，用于填充 UI 下拉框
inline QStringList baudRateLabels() {
    return {
        QStringLiteral("9600"),   QStringLiteral("19200"),
        QStringLiteral("38400"),  QStringLiteral("57600"),
        QStringLiteral("115200"), QStringLiteral("230400"),
        QStringLiteral("460800"), QStringLiteral("921600")
    };
}

inline const char *DefaultBaudRate = "460800";  ///< 默认波特率
}

// ---------------------------------------------------------------------------
// 资源路径常量
// ---------------------------------------------------------------------------
namespace Text {
inline const QString LogoResource = QStringLiteral(":/motordev_logo.svg");  ///< Logo SVG 资源路径
}

}
