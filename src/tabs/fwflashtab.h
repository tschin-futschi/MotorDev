#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class FwFlashTab;
}

class FwFlashTab : public QWidget {
    Q_OBJECT

public:
    explicit FwFlashTab(QWidget *parent = nullptr);
    ~FwFlashTab() override;

private:
    void connectSignals();

    std::unique_ptr<Ui::FwFlashTab> ui;
};
