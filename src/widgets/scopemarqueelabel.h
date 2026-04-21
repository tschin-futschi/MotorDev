#pragma once

#include <QString>
#include <QWidget>

class QTimer;

class ScopeMarqueeLabel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeMarqueeLabel(QWidget *parent = nullptr);
    ~ScopeMarqueeLabel() override;

    void setText(const QString &text);
    QString text() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void recalculate();

    QString m_text;
    QTimer *m_scrollTimer = nullptr;
    int m_textWidth = 0;
    int m_offset = 0;
    int m_gap = 40;
    bool m_shouldScroll = false;
};
