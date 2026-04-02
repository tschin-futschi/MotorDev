#pragma once

#include <QWidget>

#include <cstdint>

class QPushButton;
class QTableWidget;

class RegisterTable : public QWidget {
    Q_OBJECT

public:
    enum class ValueMode {
        Dec,
        Hex,
    };

    explicit RegisterTable(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void setValueMode(ValueMode mode);
    void updateRowValue(int globalRow, qint16 value);
    void markRowError(int globalRow);
    void markRowPending(int globalRow);
    void markWriteButtonFeedback(int globalRow, bool success);

    bool rowHasAddress(int globalRow) const;
    bool rowHasValue(int globalRow) const;
    quint16 rowAddress(int globalRow) const;
    qint16 rowValue(int globalRow) const;

    void saveConfig(const QString &path) const;
    void loadConfig(const QString &path);

signals:
    void readRowRequested(int globalRow, quint16 addr);
    void writeRowRequested(int globalRow, quint16 addr, qint16 value);
    void configChanged();

private:
    void setupUi();
    void connectSignals();
    void applyColumnWidths();

    static int descCol(int group) { return group * 5 + 0; }
    static int addrCol(int group) { return group * 5 + 1; }
    static int valueCol(int group) { return group * 5 + 2; }
    static int readCol(int group) { return group * 5 + 3; }
    static int writeCol(int group) { return group * 5 + 4; }

    QTableWidget *m_table = nullptr;
    QPushButton *m_readButtons[80] = {};
    QPushButton *m_writeButtons[80] = {};
    ValueMode m_valueMode = ValueMode::Hex;
};
