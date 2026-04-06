#pragma once

#include <QColor>
#include <QString>

namespace MotorDev::Style {

namespace Color {
inline const QColor PrimaryGreen("#3B6D11");
inline const QColor LightGreen("#EAF3DE");
inline const QColor BorderGreen("#c0dd97");
inline const QColor TextGreen("#97C459");
inline const QColor StatusBarText("#c0dd97");
inline const QColor WindowBackground("#f5f5f3");
inline const QColor ActivityBarBackground("#e8e5e0");
inline const QColor SidebarBackground("#f0ede8");
inline const QColor TopBarBackground("#f8f7f5");
inline const QColor DisconnectedIndicator("#E24B4A");
inline const QColor ConnectedIndicator("#639922");
inline const QColor DefaultBorder("#cccccc");
inline const QColor InputBorder("#dddddd");
inline const QColor TableRowBorder("#eeeeee");
inline const QColor AppText("#2c2c2a");
inline const QColor MutedText("#888888");
inline const QColor SidebarLabelText("#666666");
inline const QColor TopBarLabelText("#aaaaaa");
inline const QColor TopBarValueText("#555555");
inline const QColor PanelBackground("#ffffff");
inline const QColor White("#ffffff");
inline const QColor Transparent("transparent");
inline const QColor ReadButtonBackground("#E6F1FB");
inline const QColor ReadButtonForeground("#185FA5");
inline const QColor WriteButtonBackground("#FAEEDA");
inline const QColor WriteButtonForeground("#854F0B");
inline const QColor RegisterValueText("#185FA5");
inline const QColor RegisterErrorText("#E24B4A");
inline const QColor TableEvenRowBackground("#fafaf8");
inline const QColor TableHoverRowBackground("#f0f7e8");
inline const QColor GroupDivider("#aaaaaa");
inline const QColor TableHeaderBackground("#f0ede8");
inline const QColor TableHeaderText("#555555");
inline const QColor PrimaryButtonHover("#D5E8C4");
inline const QColor DisabledBackground("#E6E6E6");
inline const QColor DisabledBorder("#C9C9C9");
inline const QColor DisabledText("#9A9A9A");
inline const QColor PanelHoverBackground("#f5f5f2");
inline const QColor SecondaryButtonHover("#f0f0f0");
inline const QColor SecondaryButtonPressed("#e0e0e0");
inline const QColor PanelShadow(44, 44, 42, 38);
inline const QColor ScopeBackground("#f4f2ed");
inline const QColor ScopeFrameBorder("#d9d2c8");
inline const QColor ScopeFrameBackground("#fbfaf8");
inline const QColor ScopePanelBackground("#f8f6f1");
inline const QColor ScopePanelBorder("#ddd6cb");
inline const QColor ScopeSectionBackground("#fcfbf8");
inline const QColor ScopeSectionBorder("#ddd6cb");
inline const QColor ScopeOverlayBorder("#cfc7bb");
inline const QColor ScopeDivider("#d8d1c7");
inline const QColor ScopeBottomBackground("#f1eee8");
inline const QColor ScopeText("#4d4942");
inline const QColor ScopeTextMuted("#7d776f");
inline const QColor ScopeTextSecondary("#555049");
inline const QColor ScopeTextDim("#8a867e");
inline const QColor ScopeTextLabel("#6d6a63");
inline const QColor ScopeTextSubtle("#7a736a");
inline const QColor ScopeTextDark("#544f48");
inline const QColor ScopeTextChannel("#56514a");
inline const QColor ScopeTextHeader("#57524b");
inline const QColor ScopeTextAlt("#45423c");
inline const QColor ScopeInputBorder("#d5cec4");
inline const QColor ScopeInputBorderAlt("#d2ccc3");
inline const QColor ScopeToolButtonBackground("#fbfaf8");
inline const QColor ScopeToolButtonBorder("#d5cec4");
inline const QColor ScopeToolButtonHover("#f2eee8");
inline const QColor ScopeToolButtonHoverBorder("#bbb2a6");
inline const QColor ScopeToolButtonPressed("#ebe5dc");
inline const QColor ScopeToolButtonChecked("#e5f0df");
inline const QColor ScopeToolButtonCheckedBorder("#9ab781");
inline const QColor ScopeToolButtonCheckedText("#35541c");
inline const QColor ScopeToolButtonDisabled("#f0ede7");
inline const QColor ScopeToolButtonDisabledBorder("#ddd7ce");
inline const QColor ScopeToolButtonDisabledText("#a7a097");
inline const QColor ScopeToolBarBackground("#f6f4ef");
inline const QColor ScopeStatusRunningBackground("#edf6e7");
inline const QColor ScopeStatusRunningText("#46702a");
inline const QColor ScopeStatusRunningBorder("#b8d2a6");
inline const QColor ScopeStatusStoppedBackground("#f7ece9");
inline const QColor ScopeStatusStoppedText("#995548");
inline const QColor ScopeStatusStoppedBorder("#d8b0a5");
inline const QColor ScopeGridMinor("#ebe4d9");
inline const QColor ScopeGridMajor("#d5ccbf");
inline const QColor ScopeLegendBackground("#f1ede6");
inline const QColor ScopeSelectionBorder("#7aa75a");
inline const QColor ScopeSelectionFill(154, 183, 129, 48);
inline const QColor ScopeStackedLaneBorder("#ddd6cb");
inline const QColor ScopeChannelBackground("#fbfaf8");
inline const QColor ScopeChannelBorder("#d8d1c7");
inline const QColor ScopeChannelInputBorder("#d5cec4");
inline const QColor ScopeChannelInputFocus("#9ab781");
inline const QColor ScopeCheckboxUncheckedBorder("#bfb7ab");
inline const QColor ScopeCheckboxCheckedBackground("#dcead0");
inline const QColor ScopeCheckboxCheckedBorder("#9ab781");
inline const QColor ScopeRegReadBackground("#e8f1fa");
inline const QColor ScopeRegReadBorder("#bdd1e5");
inline const QColor ScopeRegWriteBackground("#f7eddd");
inline const QColor ScopeRegWriteBorder("#e1c89c");
inline const QColor ScopeRegClearBackground("#f2f0eb");
inline const QColor ScopeRegClearBorder("#d5cec4");
inline const QColor ScopeRegLoadBackground("#edf6e7");
inline const QColor ScopeRegLoadBorder("#b8d2a6");
inline const QColor ScopeToggleBackground("#fbfaf8");
inline const QColor ScopeToggleText("#5a554d");
inline const QColor ScopeToggleBorder("#d5cec4");
inline const QColor ScopeToggleHover("#f2eee8");
inline const QColor ScopeToggleHoverBorder("#bbb2a6");
inline const QColor ScopeWaveCh1("#e9c46a");
inline const QColor ScopeWaveCh2("#61afef");
inline const QColor ScopeWaveCh3("#e76f51");
inline const QColor ScopeWaveCh4("#98c379");
inline const QColor LogWarning("#B85C00");
inline const QColor LogError("#C0392B");
}

namespace Size {
inline constexpr int WindowWidth = 1280;
inline constexpr int WindowHeight = 800;
inline constexpr int MinWindowWidth = 1024;
inline constexpr int MinWindowHeight = 600;
inline constexpr int TopBarHeight = 36;
inline constexpr int StatusBarHeight = 22;
inline constexpr int ActivityBarWidth = 44;
inline constexpr int SidebarWidth = 150;
inline constexpr int SidebarCollapsedWidth = 0;
inline constexpr int SidebarHandleWidth = 18;
inline constexpr int SidebarTotalWidth = SidebarWidth + SidebarHandleWidth;
inline constexpr int SidebarHeaderHeight = 30;
inline constexpr int SidebarButtonHeight = 32;
inline constexpr int ActivityButtonSize = 34;
inline constexpr int ActivityButtonRadius = 6;
inline constexpr int LogoSize = 22;
inline constexpr int IndicatorSize = 7;
inline constexpr int SidebarAnimationMs = 200;
inline constexpr int BorderThin = 1;
inline constexpr int OuterMargin = 0;
inline constexpr int MainSpacing = 0;
inline constexpr int HeaderHorizontalPadding = 12;
inline constexpr int SidebarContentHorizontalPadding = 10;
inline constexpr int SidebarContentVerticalPadding = 8;
inline constexpr int SidebarSectionSpacing = 8;
inline constexpr int ActivityBarTopPadding = 8;
inline constexpr int ActivityBarBottomPadding = 8;
inline constexpr int ActivityBarSpacing = 6;
inline constexpr int ActivityButtonPadding = 4;
inline constexpr int LanguageComboWidth = 96;
inline constexpr int SidebarComboMinHeight = 28;
inline constexpr int ContentPadding = 24;
inline constexpr int ContentSpacing = 16;
inline constexpr int FormSpacing = 10;
inline constexpr int GroupBoxTopMargin = 8;
inline constexpr int SplitterHandleWidth = 8;
inline constexpr int StatusBarHorizontalPadding = 10;
inline constexpr int StatusBarSpacing = 10;
inline constexpr int TableRowHeight = 19;
inline constexpr int TableHeaderHeight = 24;
inline constexpr int TableGroupCount = 4;
inline constexpr int TableRowCount = 20;
inline constexpr int TableGroupDividerWidth = 2;
inline constexpr int ColDescWidth = 76;
inline constexpr int ColAddrWidth = 63;
inline constexpr int ColValueWidth = 67;
inline constexpr int ColReadWidth = 26;
inline constexpr int ColWriteWidth = 23;
}

namespace Text {
inline const QString LogoResource = QStringLiteral(":/motordev_logo.svg");
}

}
