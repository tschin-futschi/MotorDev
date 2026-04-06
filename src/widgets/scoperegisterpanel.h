#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;

class ScopeRegisterPanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeRegisterPanel(QWidget *parent = nullptr);

signals:
    void readRequested(int row);
    void writeRequested(int row);
    void startRequested();
    void stopRequested();
    void clearPanelRequested();
    void loadParamsRequested();

private:
    static constexpr int RowCount = 8;

    void setupUi();
    void connectSignals();

    QLineEdit *m_descEdits[RowCount] = {};
    QLineEdit *m_addrEdits[RowCount] = {};
    QLineEdit *m_valueEdits[RowCount] = {};
    QPushButton *m_readButtons[RowCount] = {};
    QPushButton *m_writeButtons[RowCount] = {};
    QLineEdit *m_intervalEdit = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_loadButton = nullptr;
};
