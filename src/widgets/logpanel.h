#pragma once

#include <memory>
#include <QWidget>
#include <QtGlobal>

namespace Ui {
class LogPanel;
}

class LogPanel : public QWidget {
    Q_OBJECT

public:
    explicit LogPanel(QWidget *parent = nullptr);
    ~LogPanel() override;

    static LogPanel *instance() { return s_instance; }

public slots:
    void appendMessage(QtMsgType type, const QString &msg);

private:
    std::unique_ptr<Ui::LogPanel> ui;
    static LogPanel *s_instance;
};
