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

public slots:
    void toggleCollapsed();

signals:
    void collapseStateChanged(bool collapsed);

private:
    QWidget *m_bodyWidget = nullptr;
    QWidget *m_contentWidget = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QPropertyAnimation *m_animation = nullptr;
    bool m_collapsed = false;
};
