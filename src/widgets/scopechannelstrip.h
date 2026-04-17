#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ScopeChannelStrip;
}

class ScopeChannelStrip : public QWidget {
    Q_OBJECT

public:
    explicit ScopeChannelStrip(int index, QWidget *parent = nullptr);
    ~ScopeChannelStrip() override;

    bool isChannelEnabled() const;

signals:
    void channelToggled(int index, bool enabled);
    void descriptionChanged(int index, const QString &text);
    void addressChanged(int index, const QString &text);

private:
    void connectSignals();

    std::unique_ptr<Ui::ScopeChannelStrip> ui;
    int m_index = 0;
};
