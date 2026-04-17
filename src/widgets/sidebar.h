#pragma once

#include <memory>
#include <QWidget>

class QPropertyAnimation;

namespace Ui {
class Sidebar;
}

class Sidebar : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(const QString &title, QWidget *contentWidget, QWidget *parent = nullptr);
    ~Sidebar() override;

    bool isCollapsed() const;
    QWidget *contentWidget() const;
    void setBodyWidth(int width);

public slots:
    void toggleCollapsed();
    void setCollapsed(bool collapsed);

signals:
    void collapseStateChanged(bool collapsed);

private:
    void applyExpandedWidth();

    std::unique_ptr<Ui::Sidebar> ui;
    QWidget *m_contentWidget = nullptr;
    QPropertyAnimation *m_animation = nullptr;
    bool m_collapsed = false;
    int m_bodyWidth = 0;
};
