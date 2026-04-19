#pragma once

#include <QLabel>
#include <QWidget>

class FwFlashTab : public QWidget {
    Q_OBJECT

public:
    explicit FwFlashTab(QWidget *parent = nullptr);
    ~FwFlashTab() override;

private:
    void setupUi();
    void connectSignals();

    QWidget *m_sidebarContent = nullptr;
    QWidget *m_mainContent = nullptr;
    QLabel *m_sidebarLabel = nullptr;
    QLabel *m_placeholderLabel = nullptr;
};
