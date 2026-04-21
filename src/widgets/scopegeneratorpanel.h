#pragma once

#include <QVector>
#include <QWidget>

class QButtonGroup;
class QLineEdit;
class QRadioButton;
class QPushButton;
class QStackedWidget;

struct ScopeGeneratorCosineChannel {
    quint16 addr = 0;
    double phaseDeg = 0.0;
};

class ScopeGeneratorPanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeGeneratorPanel(QWidget *parent = nullptr);
    ~ScopeGeneratorPanel() override;
    void setRunning(bool running);

signals:
    void linearStartRequested(quint16 addr, qint16 min, qint16 max, qint16 step, int intervalMs);
    void cosineStartRequested(qint16 amplitude, qint16 offset, double frequencyHz, int intervalMs,
                              const QVector<ScopeGeneratorCosineChannel> &channels);
    void stopRequested();

private:
    void setupUi();
    void connectSignals();
    void setFieldError(QLineEdit *edit, bool error);
    bool parseUint16Field(QLineEdit *edit, quint16 *out, bool allowEmpty = false);
    bool parseInt16Field(QLineEdit *edit, qint16 *out);
    bool parsePositiveIntField(QLineEdit *edit, int *out);
    bool parseDoubleField(QLineEdit *edit, double *out, bool mustBePositive);
    void handleStartClicked();
    void handleLinearStart();
    void handleCosineStart();
    void clearErrors();

    QButtonGroup *m_modeGroup = nullptr;
    QRadioButton *m_linearRadio = nullptr;
    QRadioButton *m_cosineRadio = nullptr;
    QStackedWidget *m_modeStack = nullptr;

    QLineEdit *m_linearAddrEdit = nullptr;
    QLineEdit *m_linearMinEdit = nullptr;
    QLineEdit *m_linearMaxEdit = nullptr;
    QLineEdit *m_linearStepEdit = nullptr;
    QLineEdit *m_linearIntervalEdit = nullptr;

    QLineEdit *m_cosineAmplitudeEdit = nullptr;
    QLineEdit *m_cosineOffsetEdit = nullptr;
    QLineEdit *m_cosineFrequencyEdit = nullptr;
    QLineEdit *m_cosineIntervalEdit = nullptr;
    QLineEdit *m_cosineAddrEdits[3] = {};
    QLineEdit *m_cosinePhaseEdits[3] = {};

    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
};
