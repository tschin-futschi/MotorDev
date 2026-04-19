#pragma once

#include <QLabel>
#include <QWidget>

class ScopeGeneratorPanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeGeneratorPanel(QWidget *parent = nullptr);
    ~ScopeGeneratorPanel() override;

private:
    void setupUi();

    QLabel *m_titleLabel = nullptr;
    QLabel *m_placeholderLabel = nullptr;
};
