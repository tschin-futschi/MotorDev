#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class TopBar;
}

class TopBar : public QWidget {
    Q_OBJECT

public:
    explicit TopBar(QWidget *parent = nullptr);
    ~TopBar() override;

public slots:
    void onSerialConnected(const QString &port, qint32 baudRate);
    void onSerialDisconnected();

private:
    std::unique_ptr<Ui::TopBar> ui;
};
