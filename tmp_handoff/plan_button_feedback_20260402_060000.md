# Plan

Stage: button_feedback
Status: active
Priority: medium
Source: Claude Code
Date: 2026-04-02 06:00:00
Depends-On: review_ui_tweak_20260402_050000.md (pass)
Supersedes: none

## Goal

为所有按钮添加 hover（悬浮）和 pressed（按下）视觉反馈：颜色加深 + 1px 下沉效果。

## Scope

- 主按钮（makePrimaryButton）— 添加 hover / pressed 样式
- Browse 按钮 — 添加 hover / pressed 样式
- ActivityBar 按钮 — 补充 pressed 样式（已有 hover）

## Non-Goals

- 不修改按钮尺寸、布局、信号槽连接
- 不修改非按钮控件
- 不新增颜色常量到 style_constants.h（hover/pressed 颜色直接写在 QSS 中，保持简洁）

## Files-In-Scope

- `src/tabs/configtab.cpp`（makePrimaryButton + Browse 按钮样式）
- `src/widgets/activitybar.cpp`（activityButtonStyle）

## Files-Frozen

- `src/serialmanager.h` / `src/serialmanager.cpp`
- `src/frameparser.h` / `src/frameparser.cpp`
- `src/devicecontext.h` / `src/devicecontext.cpp`
- `src/tabs/configtab.h`
- `src/mainwindow.h` / `src/mainwindow.cpp`
- `src/main.cpp`
- `src/ui/style_constants.h`
- `CMakeLists.txt`

## Constraints

- 下沉效果通过 `padding` 偏移实现（pressed 时 padding-top +1px, padding-bottom -1px），不用 QSS margin 或 transform
- 颜色加深采用手动指定的十六进制色值
- 所有按钮的 hover/pressed 效果必须同时包含颜色加深和下沉

---

## 详细设计

### 1. makePrimaryButton — configtab.cpp:63-73

**当前样式：**
```cpp
"QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:13px; padding:0 12px; }"
```

**改为：**
```cpp
"QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:13px; padding:0 12px; }"
"QPushButton:hover { background:#D5E8C4; }"
"QPushButton:pressed { background:#C0DD97; padding:1px 12px 0 12px; }"
```

颜色说明：
- 正常：`#EAF3DE`（LightGreen）
- Hover：`#D5E8C4`（LightGreen 加深一级）
- Pressed：`#C0DD97`（BorderGreen，再深一级）
- Pressed padding：top 从 0 变为 1px，bottom 从 0 保持为 0（原始 `padding:0 12px` 等于上下都是 0，改为 `1px 12px 0 12px` 实现 1px 下沉）

注意：padding 简写 `padding:0 12px` 表示上下 0、左右 12px。pressed 改为四值写法 `padding:1px 12px 0 12px`（上1 右12 下0 左12），文字视觉下移 1px。

### 2. Browse 按钮 — configtab.cpp:502-507

**当前样式：**
```cpp
"QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:13px; padding:0 12px; }"
```

**改为：**
```cpp
"QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:13px; padding:0 12px; }"
"QPushButton:hover { background:#f0f0f0; }"
"QPushButton:pressed { background:#e0e0e0; padding:1px 12px 0 12px; }"
```

颜色说明：
- 正常：`#ffffff`（White）
- Hover：`#f0f0f0`（浅灰）
- Pressed：`#e0e0e0`（灰）

### 3. ActivityBar 按钮 — activitybar.cpp:16-33

**当前样式：**
```cpp
"QPushButton {"
"background:%1; border:...; border-radius:...; color:...; font-size:11px; padding:%6px;"
"}"
"QPushButton:hover { background:%7; }"
```

**改为，追加 pressed 规则：**
```cpp
"QPushButton {"
"background:%1; border:...; border-radius:...; color:...; font-size:11px; padding:%6px;"
"}"
"QPushButton:hover { background:%7; }"
"QPushButton:pressed { background:#dddbd6; padding:%8px %6px %9px %6px; }"
```

其中：
- `%8` = `Style::Size::ActivityButtonPadding + 1`（即 5）
- `%9` = `Style::Size::ActivityButtonPadding - 1`（即 3）
- pressed 背景色 `#dddbd6`（比 hover 的 TopBarBackground `#f8f7f5` 更深）

由于 `QString::arg` 最多支持 9 个参数，需要拆分为两次 `.arg()` 调用或改用其他方式。建议将 pressed padding 值直接计算后硬编码：

```cpp
return QStringLiteral(
    "QPushButton {"
    "background:%1;"
    "border:%2px solid %3;"
    "border-radius:%4px;"
    "color:%5;"
    "font-size:11px;"
    "padding:%6px;"
    "}"
    "QPushButton:hover { background:%7; }"
    "QPushButton:pressed { background:#dddbd6; padding:%8px %6px %9px %6px; }")
    .arg(background)
    .arg(Style::Size::BorderThin)
    .arg(border)
    .arg(Style::Size::ActivityButtonRadius)
    .arg(text)
    .arg(Style::Size::ActivityButtonPadding)
    .arg(Style::Color::TopBarBackground.name())
    .arg(Style::Size::ActivityButtonPadding + 1)
    .arg(Style::Size::ActivityButtonPadding - 1);
```

注意：`QString::arg` 的 `%1` ~ `%9` 全部用到了，刚好不超限。

---

## Acceptance

1. **编译通过** — `mingw32-make -C build_make_qt -j4` 成功
2. **主按钮 hover** — 鼠标悬浮时背景变为 `#D5E8C4`
3. **主按钮 pressed** — 鼠标按下时背景变为 `#C0DD97`，文字下沉 1px
4. **Browse 按钮 hover/pressed** — 悬浮 `#f0f0f0`，按下 `#e0e0e0` + 下沉
5. **ActivityBar 按钮 pressed** — 按下时背景加深 + 下沉
6. **冻结文件未修改**

## Notes

- 如果 ActivityBar 的 `%8`/`%9` 参数超出 `QString::arg` 限制（`%1`~`%9`），可改用 `.arg(...).arg(...)` 链式调用分组
