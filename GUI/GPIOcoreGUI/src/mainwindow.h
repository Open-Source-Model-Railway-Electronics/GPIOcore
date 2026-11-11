#pragma once
#include "connections.h"
#include "gpioconfig.h"

#include <QMainWindow>
#include <array>

class ConnectionView;
class DeviceLink;
class InputEditor;
class OutputEditor;
class PinMapView;
class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QStackedWidget;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void connectDemo();
    void connectPort(const QString &port);
    bool loadFile(const QString &path);       // configuration JSON
    // Apply a preset (without asking) once the first read-all has finished.
    void applyPresetOnStart(int preset);
    // Save a screenshot once reading (and the start preset) has finished, then quit.
    void scheduleScreenshot(const QString &path, const QString &tab, int select);

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void buildUi();
    void buildToolbar();
    void refreshPorts();
    void toggleConnection();
    void onReady(const QString &version);
    void onConnectionChanged();
    void onOutputReceived(int index, const Gpio::OutputConfig &c);
    void onInputReceived(int index, const Gpio::InputConfig &c);
    void onRouteReceived(int index, const QVector<Gpio::RouteStep> &steps);
    void readFromBoard();
    void writeChanges();
    void revertChanges();
    void applyPreset();
    void openFile();
    void saveFile();
    void testOutput(int index, int state);

    void selectOutput(int index);
    void selectInput(int index);
    void boardEdited();

    enum OpenPanel { OpenNone, OpenInput, OpenOutput };
    void openPanel(OpenPanel which);
    void closePanels();
    Gpio::RouteLimits routeLimits() const;
    void applyCapabilities();
    void updateInputEditorLimits();
    void setDccNext(bool on);
    void refreshRows();
    void refreshAll();
    void refreshWarnings();
    void updateActions();
    void appendLog(const QString &kind, const QString &text);

    bool outputDirty(int index) const;
    bool inputDirty(int index) const;
    int  dirtyCount() const;
    bool outputHasConflict(int index) const;
    bool inputHasConflict(int index) const;
    QStringList outputNames() const;
    QVector<quint8> outputTypes() const;
    QStringList outputWarnings(int index) const;
    QStringList inputWarnings(int index) const;

    DeviceLink       *m_link = nullptr;
    Gpio::BoardConfig m_dev;              // last state read from the board
    Gpio::BoardConfig m_edit;             // working copy
    std::array<bool, Gpio::NumOutputs> m_outKnown {};
    std::array<bool, Gpio::NumInputs>  m_inKnown {};
    bool              m_keepEdits = false; // after opening a file: keep it over board data
    int               m_startPreset = -1;

    QTabWidget     *m_tabs = nullptr;
    ConnectionView *m_view = nullptr;
    QWidget        *m_leftPanel = nullptr;     // input settings, slides in on the left
    QWidget        *m_rightPanel = nullptr;    // output settings, slides in on the right
    OpenPanel       m_open = OpenNone;
    OutputEditor   *m_outEd = nullptr;
    InputEditor    *m_inEd = nullptr;
    PinMapView     *m_pinMap = nullptr;
    QPlainTextEdit *m_log = nullptr;

    QComboBox      *m_port = nullptr;
    QComboBox      *m_preset = nullptr;
    QCheckBox      *m_dccNext = nullptr;
    QAction        *m_actRefresh = nullptr;
    QAction        *m_actConnect = nullptr;
    QAction        *m_actDemo = nullptr;
    QAction        *m_actRead = nullptr;
    QAction        *m_actWrite = nullptr;
    QAction        *m_actRevert = nullptr;
    QAction        *m_actPreset = nullptr;
    QAction        *m_actOpen = nullptr;
    QAction        *m_actSave = nullptr;
    QAction        *m_actRaw = nullptr;
    QLabel         *m_connChip = nullptr;
    QLabel         *m_legacyChip = nullptr;
    QLabel         *m_dirtyChip = nullptr;
    QLabel         *m_protoChip = nullptr;
    QProgressBar   *m_progress = nullptr;
};
