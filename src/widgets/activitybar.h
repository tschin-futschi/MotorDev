#pragma once

#include <QWidget>

class QPushButton;

class ActivityBar : public QWidget {
    Q_OBJECT

public:
    enum Page {
        RegisterPage = 0,
        FlashPage = 1,
        ScopePage = 2,
    };

    explicit ActivityBar(QWidget *parent = nullptr);

signals:
    void pageSelected(int index);

private:
    void setActivePage(int index);

    QPushButton *m_registerButton = nullptr;
    QPushButton *m_flashButton = nullptr;
    QPushButton *m_scopeButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
};
