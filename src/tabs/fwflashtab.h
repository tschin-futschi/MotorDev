#pragma once

#include <QWidget>

class FwFlashTab : public QWidget {
    Q_OBJECT

public:
    explicit FwFlashTab(QWidget *parent = nullptr);

private:
    void setupUi();
    void connectSignals();
};
