#pragma once

#include <QWidget>

class QPropertyAnimation;
class QPushButton;
class QWidget;

class Sidebar : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(QWidget *parent = nullptr);

    bool isCollapsed() const;

public slots:
    void toggleCollapsed();

signals:
    void collapseStateChanged(bool collapsed);

private:
    QWidget *m_contentWidget = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPropertyAnimation *m_animation = nullptr;
    bool m_collapsed = false;
};
