#pragma once

#include <QWidget>

class QPushButton;

class ActivityBar : public QWidget {
    Q_OBJECT

public:
    enum Page {
        ConfigPage = 0,
        RegisterPage = 1,
        FlashPage = 2,
        ScopePage = 3,
    };

    explicit ActivityBar(QWidget *parent = nullptr);

signals:
    void pageSelected(int index);

private:
    void setActivePage(int index);

    QPushButton *m_configButton = nullptr;
    QPushButton *m_registerButton = nullptr;
    QPushButton *m_flashButton = nullptr;
    QPushButton *m_scopeButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
};
