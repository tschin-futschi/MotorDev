#pragma once

#include <QWidget>

class QPropertyAnimation;
class QPushButton;
class QWidget;

class Sidebar : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(const QString &title, QWidget *contentWidget, QWidget *parent = nullptr);

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

    QWidget *m_bodyWidget = nullptr;
    QWidget *m_contentWidget = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QPropertyAnimation *m_animation = nullptr;
    bool m_collapsed = false;
    int m_bodyWidth = 0;
};
