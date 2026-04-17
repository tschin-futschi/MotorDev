#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ScopeGeneratorPanel;
}

class ScopeGeneratorPanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeGeneratorPanel(QWidget *parent = nullptr);
    ~ScopeGeneratorPanel() override;

private:
    std::unique_ptr<Ui::ScopeGeneratorPanel> ui;
};
