#pragma once

#include <QWidget>
#include <QtGlobal>

class QTextEdit;

class LogPanel : public QWidget {
    Q_OBJECT

public:
    explicit LogPanel(QWidget *parent = nullptr);
    ~LogPanel() override;

    static LogPanel *instance() { return s_instance; }

public slots:
    void appendMessage(QtMsgType type, const QString &msg);

private:
    QTextEdit *m_textEdit = nullptr;
    static LogPanel *s_instance;
};
