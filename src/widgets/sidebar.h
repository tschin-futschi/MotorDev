#pragma once

#include <QWidget>

class QPropertyAnimation;
class QLabel;
class QPushButton;

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
    void setupUi(const QString &title);

    QWidget *m_bodyWidget = nullptr;
    QLabel *m_headerLabel = nullptr;
    QWidget *m_contentPlaceholder = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QWidget *m_contentWidget = nullptr;
    QPropertyAnimation *m_animation = nullptr;
    bool m_collapsed = false;
    int m_bodyWidth = 0;
};
