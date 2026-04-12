#pragma once

#include <QColor>
#include <QWidget>

class QComboBox;
class QPushButton;
class QSpinBox;

class ScopeStylePanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeStylePanel(QWidget *parent = nullptr);

    void setChannelColor(int index, const QColor &color);
    void setChannelLineWidth(int index, int width);
    void setChannelLineStyle(int index, Qt::PenStyle style);

signals:
    void colorChanged(int index, const QColor &color);
    void lineWidthChanged(int index, int width);
    void lineStyleChanged(int index, Qt::PenStyle style);
    void defaultSettingsRequested();

private:
    static constexpr int kChannelCount = 8;

    struct ChannelRow {
        QPushButton *colorButton = nullptr;
        QSpinBox *widthSpin = nullptr;
        QComboBox *styleCombo = nullptr;
        QColor currentColor;
    };

    void setupUi();
    void connectSignals();
    void updateColorButton(int index);

    ChannelRow m_rows[kChannelCount];
    QPushButton *m_defaultButton = nullptr;
};
