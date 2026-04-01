#pragma once

#include <QWidget>

class OscilloscopTab : public QWidget {
    Q_OBJECT

public:
    explicit OscilloscopTab(QWidget *parent = nullptr);

private:
    void setupUi();
    void connectSignals();
};
