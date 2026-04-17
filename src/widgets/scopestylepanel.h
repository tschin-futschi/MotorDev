#pragma once

#include <memory>
#include <QColor>
#include <QWidget>

class QComboBox;
class QPushButton;
class QSpinBox;
namespace Ui {
class ScopeStylePanel;
}

class ScopeStylePanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeStylePanel(QWidget *parent = nullptr);
    ~ScopeStylePanel() override;

    void setChannelColor(int index, const QColor &color);
    void setChannelLineWidth(int index, int width);
    void setChannelLineStyle(int index, Qt::PenStyle style);
    void setChannelShowDataPoints(int index, bool show);

signals:
    void colorChanged(int index, const QColor &color);
    void lineWidthChanged(int index, int width);
    void lineStyleChanged(int index, Qt::PenStyle style);
    void dataPointsChanged(int index, bool showDataPoints);
    void defaultSettingsRequested();

private:
    static constexpr int kChannelCount = 8;

    struct ChannelRow {
        QPushButton *colorButton = nullptr;
        QSpinBox *widthSpin = nullptr;
        QComboBox *styleCombo = nullptr;
        QColor currentColor;
    };

    void connectSignals();
    void updateColorButton(int index);

    std::unique_ptr<Ui::ScopeStylePanel> ui;
    ChannelRow m_rows[kChannelCount];
};
