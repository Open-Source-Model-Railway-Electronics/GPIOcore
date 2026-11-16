#pragma once
#include "connections.h"
#include "gpioconfig.h"

#include <QMainWindow>
#include <QSet>
#include <array>

class ConnectionView;
class DeviceLink;
class WebRemote;
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
class QLineEdit;
class QTimer;
class QToolButton;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void connectDemo();                     // Simulate mode (your configuration on a simulated board)
    void connectSimulatedBoard();           // tests: a simulated board that behaves like a real one (Configure)
    enum Mode { ModeConfigure, ModeSimulate, ModeLive };
    void setMode(Mode m);
    Mode mode() const { return m_mode; }
    void connectPort(const QString &port);
    bool loadFile(const QString &path);       // configuration JSON
    void setExpertMode(bool on);              // simple mode hides bytes, log and protocol details
    void showWorkspace();                     // leave the start page
    static QString boardsDir();               // where <board name>.json files are kept
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
    void applyPresetNow(int preset, bool clearNames = true);   // replace the configuration, then live write
    void openFile();
    void saveFile();
    void testOutput(int index, int state);

    void selectOutput(int index);
    void selectInput(int index);
    void boardEdited();

    enum OpenPanel { OpenNone, OpenInput, OpenOutput };
    void openPanel(OpenPanel which);
    void closePanels();
    void showInput(int index);              // refresh the input panel only when needed
    void showOutput(int index);
    Gpio::RouteLimits routeLimits() const;
    void applyCapabilities();
    void updateInputEditorLimits();
    void setBoardType(int type);
    void userEdited();                        // an edit by the user: send it to the board soon
    void sendLive();
    void writeDirty(bool live);               // live: skip items the board already refused to take
    Gpio::InputConfig asStored(const Gpio::InputConfig &c) const;   // what the board keeps of an input
    void onBoardReceived(const Gpio::BoardSettings &b);
    void editBoardSettings();
    void askBoardName();
    void saveBoardFile();
    void backupBoardFile(const QString &path);   // copy to boards/backup/ before it is overwritten
    void quickSave();                            // Ctrl+S: always 'save as' (nothing is overwritten unseen)
    void markFileDirty();                        // the configuration differs from the saved file
    void updateEditing();                        // editing only when configuring or offline
    void writeAllToBoard();                      // send the whole configuration on screen to the board
    void newOfflineBoard();
    void addBlock(bool output);
    void removeSelected();
    void flashFirmware();
    void openPhoneRemote();                  // web page for phones (QR code)
    void setupWebRemote();
    void setLiveTest(bool on);
    // A pin newly given to output 'output' / input 'input' is taken from every other user, which
    // becomes 'not in use'. Returns a description for the status bar.
    // share: Ui::ShareAnalog / ShareSound, users of the same kind keep the pin (they may share it).
    QString takeOverPins(const QVector<quint8> &pins, int output, int input, int share = 0);
    void openLiveValue(int input);           // live test: aspect / PWM value via DCC extended
    void openAnalogValue(int input);         // simulation: the voltage on an analog input's pin
    void pollAnalog();                       // live reading of the analog pins in view (GET_ANALOG)
    QWidget *buildStartPage();
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
    QStringList inputNames() const;
    QVector<quint8> inputTypes() const;
    QStringList outputWarnings(int index) const;
    QStringList inputWarnings(int index) const;

    DeviceLink       *m_link = nullptr;
    WebRemote        *m_web = nullptr;
    Gpio::BoardConfig m_dev;              // last state read from the board
    Gpio::BoardConfig m_edit;             // working copy
    std::array<bool, Gpio::NumOutputs> m_outKnown {};
    std::array<bool, Gpio::NumInputs>  m_inKnown {};
    // Connected without reading: an item the user changed is sent although the board state is unknown.
    std::array<bool, Gpio::NumOutputs> m_outTouched {};
    std::array<bool, Gpio::NumInputs>  m_inTouched {};
    Gpio::BoardConfig m_shadow;           // m_edit before the last user edit (to see what was touched)
    bool              m_readingBoard = false;   // 'Read board' in progress: names from the file, board wins
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
    QComboBox      *m_boardType = nullptr;
    QStackedWidget *m_stack = nullptr;          // 0 = start page, 1 = workspace
    QTimer         *m_liveTimer = nullptr;
    QTimer         *m_analogTimer = nullptr;    // live reading of analog pins, every 250 ms
    QLineEdit      *m_inName = nullptr;
    QLineEdit      *m_outName = nullptr;
    QWidget        *m_lampRow = nullptr;       // signal: lamp colours (board file only)
    QList<QToolButton *> m_lampButtons;
    void            refreshLampRow();
    QAction        *m_actExpert = nullptr;
    QAction        *m_actShowUnused = nullptr;
    QAction        *m_actBoardSettings = nullptr;
    QAction        *m_actLive = nullptr;
    QAction        *m_actConfigure = nullptr;   // mode button: connected to a real board
    QAction        *m_actWriteAll = nullptr;
    QAction        *m_actSaveAs = nullptr;
    QList<QAction *> m_expertOnly;              // toolbar items shown in expert mode only
    bool            m_expert = false;
    bool            m_nameAsked = false;
    bool            m_simulating = false;
    Mode            m_mode = ModeConfigure;
    QLabel         *m_connState = nullptr;      // toolbar: connected / offline
    QAction        *m_actConnectTb = nullptr;   // toolbar: Connect / Disconnect
    void            startSimulation();
    void            updateConnState();
    bool            m_fileDirty = false;        // unsaved changes (asked about when closing)
    QString         m_filePath;                 // last opened / saved file
    QWidget        *m_addIn = nullptr, *m_addOut = nullptr;
    QList<QWidget *> m_editWidgets;             // disabled during simulation / live test
    void            updateFrame();
    QSet<int>       m_liveTried;                // live write: items sent since the last user edit
                                                // (inputs 0-15, outputs 100-115)
    bool            m_liveWarned = false;
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
    QLabel         *m_dirtyChip = nullptr;
    QLabel         *m_protoChip = nullptr;
    QProgressBar   *m_progress = nullptr;
};
