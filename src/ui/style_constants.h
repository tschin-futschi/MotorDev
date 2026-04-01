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
inline constexpr int GroupBoxTopMargin = 10;
inline constexpr int SplitterHandleWidth = 8;
inline constexpr int StatusBarHorizontalPadding = 10;
inline constexpr int StatusBarSpacing = 10;
}

namespace Text {
inline const QString LogoResource = QStringLiteral(":/motordev_logo.svg");
}

}
