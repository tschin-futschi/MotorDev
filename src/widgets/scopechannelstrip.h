#pragma once

#include <QCheckBox>
#include <QLineEdit>
#include <QWidget>

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
    void setupUi();
    void connectSignals();

    QCheckBox *m_checkBox = nullptr;
    QLineEdit *m_descEdit = nullptr;
    QLineEdit *m_addrEdit = nullptr;
    int m_index = 0;
};
