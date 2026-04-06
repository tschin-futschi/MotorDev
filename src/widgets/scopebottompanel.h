#pragma once

#include <QWidget>

class ScopeChannelStrip;
class QEvent;
class ScopeGeneratorPanel;
class ScopeRegisterPanel;
class QPushButton;
class QPoint;
class QWidget;

class ScopeBottomPanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeBottomPanel(QWidget *overlayHost, QWidget *parent = nullptr);
    ~ScopeBottomPanel() override;

signals:
    void registerReadRequested(int row);
    void registerWriteRequested(int row);
    void clearPanelRequested();
    void loadParamsRequested();
    void channelToggled(int index, bool enabled);
    void channelDescriptionChanged(int index, const QString &text);
    void channelAddressChanged(int index, const QString &text);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setupUi();
    void connectSignals();
    QWidget *createOverlayWindow(const QString &title, QWidget *content, const QSize &size);
    void refreshPanels();

    QWidget *m_overlayHost = nullptr;
    QWidget *m_channelFrame = nullptr;
    ScopeRegisterPanel *m_registerPanel = nullptr;
    ScopeGeneratorPanel *m_generatorPanel = nullptr;
    ScopeChannelStrip *m_channels[8] = {};
    QPushButton *m_channelsToggleButton = nullptr;
    QPushButton *m_registerToggleButton = nullptr;
    QPushButton *m_generatorToggleButton = nullptr;
    QWidget *m_channelsWindow = nullptr;
    QWidget *m_registerWindow = nullptr;
    QWidget *m_generatorWindow = nullptr;
    bool m_channelsVisible = true;
};
