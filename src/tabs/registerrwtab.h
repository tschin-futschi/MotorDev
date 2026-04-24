#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QLineEdit;
class QSplitter;
class CommandDispatcher;
class RegisterService;
class RegisterTable;

class RegisterRwTab : public QWidget {
    Q_OBJECT

public:
    explicit RegisterRwTab(CommandDispatcher *dispatcher, QWidget *parent = nullptr);
    ~RegisterRwTab() override;

public slots:
    void setConnected(bool connected);

private:
    void setupUi();
    void connectSignals();
    QString configFilePath() const;

    RegisterService *m_service = nullptr;
    QWidget *m_sidebarContent = nullptr;
    QWidget *m_mainContent = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    RegisterTable *m_registerTable = nullptr;
    QPushButton *m_readAllButton = nullptr;
    QPushButton *m_writeAllButton = nullptr;
    QPushButton *m_decButton = nullptr;
    QPushButton *m_hexButton = nullptr;
    QPushButton *m_batchBtn[4] = {};
    QLineEdit *m_batchDescEdit[4] = {};
    QLineEdit *m_batchPathEdit[4] = {};
    QPushButton *m_batchBrowseBtn[4] = {};
};
