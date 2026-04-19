#pragma once

#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>
#include <QtGlobal>

class LogPanel : public QWidget {
    Q_OBJECT

public:
    explicit LogPanel(QWidget *parent = nullptr);
    ~LogPanel() override;

    static LogPanel *instance() { return s_instance; }

public slots:
    void appendMessage(QtMsgType type, const QString &msg);

private:
    void setupUi();

    QLabel *m_titleLabel = nullptr;
    QPushButton *m_clearButton = nullptr;
    QTextEdit *m_textEdit = nullptr;
    static LogPanel *s_instance;
};
