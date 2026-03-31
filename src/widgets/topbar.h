#pragma once

#include <QWidget>

class QLabel;
class QComboBox;
class QSvgWidget;

class TopBar : public QWidget {
    Q_OBJECT

public:
    explicit TopBar(QWidget *parent = nullptr);

private:
    QLabel *m_connectionIndicator = nullptr;
    QLabel *m_portValueLabel = nullptr;
    QComboBox *m_languageCombo = nullptr;
};
