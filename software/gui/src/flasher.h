#pragma once
// Firmware upload for GPIOcore boards (Arduino Uno / Nano) via avrdude.
//
// Flasher     : runs avrdude asynchronously through QProcess.
// FlashDialog : beginner-friendly dialog around Flasher.
//
// avrdude is looked up in <exeDir>/tools/avrdude/, then in
// <GPIOCORE_SOURCE_DIR>/tools/avrdude/ (when that define is set), then in PATH.

#include <QDialog>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QToolButton;

class Flasher : public QObject
{
    Q_OBJECT
public:
    enum Method {
        UnoNewNano115200,   // Arduino bootloader (Optiboot), 115200 baud: Uno, new Nano
        OldNano57600,       // Arduino bootloader, 57600 baud: old Nano ("ATmega328P (Old Bootloader)")
        ArduinoIsp          // Arduino as ISP programmer (stk500v1, 19200 baud), also writes fuses
    };
    Q_ENUM(Method)

    // Why the last run failed (None when it succeeded or is still running).
    enum Failure {
        None,
        AvrdudeMissing,     // avrdude.exe / avrdude.conf not found
        HexMissing,         // firmware file does not exist or cannot be read
        PortBusy,           // port is open in another program (access denied)
        PortNotFound,       // port does not exist (board unplugged?)
        NoSync,             // no answer from the bootloader: wrong board type / baud rate
        Signature,          // wrong chip or ISP wiring problem
        Verify,             // written data does not match
        Cancelled,
        Other
    };
    Q_ENUM(Failure)

    explicit Flasher(QObject *parent = nullptr);
    ~Flasher() override;

    // Full path of avrdude, or empty when not found.
    static QString findAvrdude();
    // avrdude.conf next to avrdude, or empty (then avrdude uses its built-in default).
    static QString findAvrdudeConf(const QString &avrdudePath);

    // When set (default true), a UnoNewNano115200 run that fails with a sync
    // error is retried once with OldNano57600.
    void setAutoRetry(bool on) { m_autoRetry = on; }
    bool autoRetry() const     { return m_autoRetry; }

    bool    isRunning() const;
    Method  usedMethod() const  { return m_method; }   // method of the current / last attempt
    Failure lastFailure() const { return m_failure; }

    // avrdude arguments for a method (without the program itself). Public for tests / logging.
    static QStringList arguments(const QString &port, const QString &hexPath, Method m,
                                 const QString &confPath = QString());

public slots:
    // Starts avrdude; returns immediately. Result arrives through finished().
    void flash(const QString &port, const QString &hexPath, Method m);
    // Stops a running avrdude (finished(false, ...) follows).
    void cancel();

signals:
    void started(Flasher::Method m);
    void outputLine(const QString &line);   // one line of avrdude output (or an info line from Flasher)
    void progress(int percent);             // 0..100, or -1 when the step has no known length (busy)
    void finished(bool ok, const QString &summary);

private:
    void start(Method m);
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onError(QProcess::ProcessError error);
    void handleText(const QString &text);
    void finish(bool ok, Failure failure, const QString &summary);
    Failure classify() const;

    QProcess   *m_proc = nullptr;
    QString     m_port;
    QString     m_hex;
    Method      m_method = UnoNewNano115200;
    Failure     m_failure = None;
    bool        m_autoRetry = true;
    bool        m_retried = false;
    bool        m_cancelled = false;
    bool        m_done = true;
    QString     m_lineBuf;       // incomplete output line
    QString     m_allOutput;     // full output of the current attempt (for classify())
    // Progress parsing: avrdude prints "Writing | ####...# | 100% 1.23s" (50 '#').
    enum Phase { Idle, Writing, Verifying, AfterVerify };
    Phase       m_phase = Idle;
    bool        m_inBar = false;
    int         m_hashes = 0;
};

class FlashDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FlashDialog(const QString &preselectPort = QString(), QWidget *parent = nullptr);

    // <repo>/release/GPIOcore.hex when it exists, otherwise <exeDir>/GPIOcore.hex.
    static QString defaultHexPath();

    bool isBusy() const;

signals:
    // Emitted right before avrdude starts: close your own connection to the port now.
    void aboutToFlash();
    // Emitted when avrdude is done (success or not): you may reconnect.
    void flashFinished(bool ok);

public slots:
    void reject() override;   // ignored while flashing

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void refreshPorts(const QString &select);
    void browseHex();
    void startFlash();
    void onOutputLine(const QString &line);
    void onProgress(int percent);
    void onFinished(bool ok, const QString &summary);
    void setBusy(bool busy);
    void setDetailsVisible(bool visible);
    void showResult(bool ok, const QString &text);
    QString failureHint(Flasher::Failure f) const;

    Flasher        *m_flasher = nullptr;
    QComboBox      *m_port = nullptr;
    QToolButton    *m_refresh = nullptr;
    QLineEdit      *m_hex = nullptr;
    QPushButton    *m_browse = nullptr;
    QComboBox      *m_method = nullptr;
    QLabel         *m_ispNote = nullptr;
    QPushButton    *m_flashBtn = nullptr;
    QPushButton    *m_closeBtn = nullptr;
    QProgressBar   *m_bar = nullptr;
    QLabel         *m_status = nullptr;
    QLabel         *m_result = nullptr;
    QToolButton    *m_detailsBtn = nullptr;
    QPlainTextEdit *m_log = nullptr;
};
