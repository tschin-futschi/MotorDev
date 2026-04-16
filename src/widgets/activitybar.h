#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ActivityBar;
}

class ActivityBar : public QWidget {
    Q_OBJECT

public:
    enum Page {
        ConfigPage = 0,
        RegisterPage = 1,
        FlashPage = 2,
        ScopePage = 3,
        DebugPage = 4,
    };

    explicit ActivityBar(QWidget *parent = nullptr);
    ~ActivityBar() override;
    void setPageEnabled(int page, bool enabled);

signals:
    void pageSelected(int index);

private:
    void setActivePage(int index);

    std::unique_ptr<Ui::ActivityBar> ui;
};
