# Plan

Stage: group_hover_fix
Status: active
Priority: high
Source: Claude Code
Date: 2026-04-02 07:40:00
Depends-On: review_group_hover_and_configfile_20260402_073000.md (pass)
Supersedes: none

## Goal

两项 UI 修复：1) GroupBox hover 效果不生效，改用 eventFilter 方案；2) GroupBox 内按钮移至底部对齐。

## Scope

- 修改 `makePanelGroup` 函数：移除 QSS `:hover`，改用动态属性 `hovered` + eventFilter
- 修改 `createIcGroup` / `createSerialGroup` / `createPmicGroup`：按钮行沉底

## Non-Goals

- 不修改按钮 hover/pressed 效果
- 不修改 Config File 行布局
- 不修改信号槽连接

## Files-In-Scope

- `src/tabs/configtab.cpp`

## Files-Frozen

- 其他所有文件

## Constraints

- eventFilter 逻辑放在 configtab.cpp 的匿名命名空间中，不新增独立文件
- 按钮沉底通过 `addStretch()` 实现，不使用固定定位

---

## 详细设计

### 1. GroupBox hover 修复

#### 1.1 新增 include

在 `configtab.cpp` 顶部添加（如尚未包含）：
```cpp
#include <QEvent>
#include <QStyle>
```

#### 1.2 匿名命名空间内新增 GroupHoverFilter

```cpp
class GroupHoverFilter : public QObject {
public:
    explicit GroupHoverFilter(QObject *parent = nullptr)
        : QObject(parent) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::HoverEnter) {
            auto *widget = qobject_cast<QWidget *>(watched);
            if (widget != nullptr) {
                widget->setProperty("hovered", true);
                widget->style()->polish(widget);
            }
        } else if (event->type() == QEvent::HoverLeave) {
            auto *widget = qobject_cast<QWidget *>(watched);
            if (widget != nullptr) {
                widget->setProperty("hovered", false);
                widget->style()->polish(widget);
            }
        }
        return QObject::eventFilter(watched, event);
    }
};
```

优先使用 `HoverEnter`/`HoverLeave`（配合 `WA_Hover`），比 `Enter`/`Leave` 更可靠——鼠标在子控件上时不会丢失父容器的 hover。

#### 1.3 修改 makePanelGroup

QSS 中将 `QGroupBox:hover` 改为动态属性选择器：
```cpp
"QGroupBox { background:%1; border:%2px solid %3; padding-top:28px; color:%4; font-size:14px; font-weight:500; }"
"QGroupBox[hovered=\"true\"] { background:#f5f5f2; }"
"QGroupBox::title { subcontrol-origin: padding; subcontrol-position: top center; padding:8px 0 4px 0; }"
```

函数末尾、`return group;` 之前添加：
```cpp
group->setAttribute(Qt::WA_Hover, true);
group->installEventFilter(new GroupHoverFilter(group));
```

### 2. 按钮沉底

三个 GroupBox 的按钮行需要始终在底部。方法：移除 `layout->setAlignment(Qt::AlignTop)`，在表单和按钮之间添加 `addStretch()`。

#### 2.1 createIcGroup

**当前：**
```cpp
layout->setAlignment(Qt::AlignTop);
// ... formLayout ...
layout->addLayout(formLayout);
// buttonRow
layout->addLayout(buttonRow);
```

**改为：**
```cpp
// 移除 layout->setAlignment(Qt::AlignTop);
// ... formLayout ...
layout->addLayout(formLayout);
layout->addStretch();
layout->addLayout(buttonRow);
```

#### 2.2 createSerialGroup

同上：移除 `layout->setAlignment(Qt::AlignTop)`，在 `layout->addLayout(formLayout)` 和 `layout->addLayout(buttonRow)` 之间添加 `layout->addStretch()`。

#### 2.3 createPmicGroup

**当前：**
```cpp
layout->setAlignment(Qt::AlignTop);
// ... formLayout ...
layout->addLayout(formLayout);
m_pmicConfigButton = makePrimaryButton(tr("配置 PMIC"), group);
layout->addWidget(m_pmicConfigButton, 0, Qt::AlignLeft);
```

**改为：**
```cpp
// 移除 layout->setAlignment(Qt::AlignTop);
// ... formLayout ...
layout->addLayout(formLayout);
layout->addStretch();
m_pmicConfigButton = makePrimaryButton(tr("配置 PMIC"), group);
layout->addWidget(m_pmicConfigButton, 0, Qt::AlignLeft);
```

---

## Acceptance

1. **编译通过** — `mingw32-make -C build_make_qt -j4` 成功
2. **GroupBox hover 生效** — 鼠标悬浮在 IC / Serial / PMIC GroupBox 上时，背景色变为 `#f5f5f2`
3. **子控件上 hover 不消失** — 鼠标在 GroupBox 内的 ComboBox、按钮上时，GroupBox 背景仍保持 hover 状态
4. **鼠标离开恢复** — 鼠标完全离开 GroupBox 后，背景色恢复白色
5. **按钮沉底** — 三个 GroupBox 的按钮行始终位于 GroupBox 底部，表单内容在顶部
6. **冻结文件未修改**

## Notes

- GroupHoverFilter 放在匿名命名空间中，父对象设为 group，生命周期由 GroupBox 管理
- 如果编译时因匿名命名空间中的 QObject 子类需要 MOC 而报错，可将 GroupHoverFilter 移到 configtab.h 中作为内部类，或移到 configtab.cpp 的文件作用域并 `#include "moc_configtab.cpp"` 之前
