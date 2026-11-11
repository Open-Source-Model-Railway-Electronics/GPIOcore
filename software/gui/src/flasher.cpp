#include "flasher.h"
#include "infotexts.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// ATmega328P fuses for ISP programming (same as OS-software-tool/boards.ini)
const char *kIspLock  = "0x3F";
const char *kIspEfuse = "0xFD";
const char *kIspHfuse = "0xDA";
const char *kIspLfuse = "0xFF";

QString avrdudeExeName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("avrdude.exe");
#else
    return QStringLiteral("avrdude");
#endif
}

// Directories that may contain avrdude, in search order.
QStringList avrdudeDirs()
{
    QStringList dirs;
    const QString exeDir = QCoreApplication::applicationDirPath();
    dirs << exeDir + QStringLiteral("/tools/avrdude")
         << exeDir;
#ifdef GPIOCORE_SOURCE_DIR
    dirs << QStringLiteral(GPIOCORE_SOURCE_DIR) + QStringLiteral("/tools/avrdude");
#endif
    return dirs;
}

// "COM3" -> "\\.\COM3" (needed for COM10 and up), "ttyUSB0" -> "/dev/ttyUSB0"
QString avrdudePort(const QString &port)
{
    if (port.contains(QLatin1Char('/')) || port.contains(QLatin1Char('\\')))
        return port;
#ifdef Q_OS_WIN
    return QStringLiteral("\\\\.\\") + port;
#else
    return QStringLiteral("/dev/") + port;
#endif
}

} // namespace

// ---------------------------------------------------------------------------
// Flasher
// ---------------------------------------------------------------------------

Flasher::Flasher(QObject *parent)
    : QObject(parent)
{
}

Flasher::~Flasher()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->disconnect(this);
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
}

QString Flasher::findAvrdude()
{
    const QString name = avrdudeExeName();
    for (const QString &dir : avrdudeDirs()) {
        const QFileInfo fi(dir + QLatin1Char('/') + name);
        if (fi.isFile())
            return QDir::cleanPath(fi.absoluteFilePath());
    }
    return QStandardPaths::findExecutable(QStringLiteral("avrdude"));   // installed system-wide
}

QString Flasher::findAvrdudeConf(const QString &avrdudePath)
{
    if (avrdudePath.isEmpty())
        return QString();
    const QFileInfo fi(QFileInfo(avrdudePath).absolutePath() + QStringLiteral("/avrdude.conf"));
    return fi.isFile() ? QDir::cleanPath(fi.absoluteFilePath()) : QString();
}

bool Flasher::isRunning() const
{
    return !m_done;
}

QStringList Flasher::arguments(const QString &port, const QString &hexPath, Method m,
                               const QString &confPath)
{
    QStringList a;
    if (!confPath.isEmpty())
        a << QStringLiteral("-C") << QDir::toNativeSeparators(confPath);
    a << QStringLiteral("-v")
      << QStringLiteral("-p") << QStringLiteral("atmega328p");

    const QString flashOp = QStringLiteral("flash:w:") + QDir::toNativeSeparators(hexPath)
                          + QStringLiteral(":i");
    switch (m) {
    case UnoNewNano115200:
    case OldNano57600:
        // Same as the Arduino IDE: the bootloader erases pages itself, so -D (no chip erase).
        a << QStringLiteral("-c") << QStringLiteral("arduino")
          << QStringLiteral("-P") << avrdudePort(port)
          << QStringLiteral("-b") << (m == UnoNewNano115200 ? QStringLiteral("115200")
                                                            : QStringLiteral("57600"))
          << QStringLiteral("-D")
          << QStringLiteral("-U") << flashOp;
        break;
    case ArduinoIsp:
        // Same as OS-software-tool/UPLOAD_FIRMWARE.bat: chip erase, flash, lock and fuses.
        a << QStringLiteral("-c") << QStringLiteral("stk500v1")
          << QStringLiteral("-P") << avrdudePort(port)
          << QStringLiteral("-b") << QStringLiteral("19200")
          << QStringLiteral("-e")
          << QStringLiteral("-U") << flashOp
          << QStringLiteral("-U") << QStringLiteral("lock:w:%1:m").arg(QLatin1String(kIspLock))
          << QStringLiteral("-U") << QStringLiteral("efuse:w:%1:m").arg(QLatin1String(kIspEfuse))
          << QStringLiteral("-U") << QStringLiteral("hfuse:w:%1:m").arg(QLatin1String(kIspHfuse))
          << QStringLiteral("-U") << QStringLiteral("lfuse:w:%1:m").arg(QLatin1String(kIspLfuse));
        break;
    }
    return a;
}

void Flasher::flash(const QString &port, const QString &hexPath, Method m)
{
    if (isRunning())
        return;
    m_port = port;
    m_hex = QFileInfo(hexPath).absoluteFilePath();   // avrdude runs in its own directory
    m_retried = false;
    m_cancelled = false;
    m_failure = None;
    m_done = false;
    m_method = m;

    // Problems we can see before starting avrdude. Reported asynchronously,
    // like a real run, so callers can rely on finished() coming after flash().
    Failure pre = None;
    QString msg;
    if (findAvrdude().isEmpty()) {
        pre = AvrdudeMissing;
        msg = tr("The upload program (avrdude) was not found next to GPIOcoreGUI (tools/avrdude).");
    } else if (!QFileInfo(hexPath).isFile()) {
        pre = HexMissing;
        msg = tr("The firmware file was not found: %1").arg(QDir::toNativeSeparators(hexPath));
    } else if (port.trimmed().isEmpty()) {
        pre = PortNotFound;
        msg = tr("No port selected.");
    }
    if (pre != None) {
        QTimer::singleShot(0, this, [this, pre, msg] {
            emit outputLine(msg);
            finish(false, pre, msg);
        });
        return;
    }
    start(m);
}

void Flasher::start(Method m)
{
    m_method = m;
    m_lineBuf.clear();
    m_allOutput.clear();
    m_phase = Idle;
    m_inBar = false;
    m_hashes = 0;

    const QString exe = findAvrdude();
    const QStringList args = arguments(m_port, m_hex, m, findAvrdudeConf(exe));

    if (!m_proc) {
        m_proc = new QProcess(this);
        m_proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_proc, &QProcess::readyRead, this, &Flasher::onReadyRead);
        connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &Flasher::onFinished);
        connect(m_proc, &QProcess::errorOccurred, this, &Flasher::onError);
    }
    m_proc->setWorkingDirectory(QFileInfo(exe).absolutePath());

    emit started(m);
    emit progress(-1);
    emit outputLine(QStringLiteral("> \"%1\" %2").arg(QDir::toNativeSeparators(exe), args.join(QLatin1Char(' '))));
    m_proc->start(exe, args);
}

void Flasher::cancel()
{
    if (!isRunning())
        return;
    m_cancelled = true;
    if (m_proc && m_proc->state() != QProcess::NotRunning)
        m_proc->kill();
}

void Flasher::onReadyRead()
{
    handleText(QString::fromLocal8Bit(m_proc->readAll()));
}

// Splits avrdude output into lines and follows its progress bars.
// Without a terminal avrdude 6.x prints "Writing | " and then one '#' per 2 %,
// followed by " | 100% 1.23s". The first "Writing" bar is the flash write,
// the "Reading" bar right after it is the verify; later bars (fuses) are ignored.
void Flasher::handleText(const QString &text)
{
    m_allOutput += text;
    for (const QChar c : text) {
        if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            const QString line = m_lineBuf.trimmed();
            if (!line.isEmpty())
                emit outputLine(line);
            m_lineBuf.clear();
            m_inBar = false;
            continue;
        }
        m_lineBuf += c;

        if (m_inBar) {
            if (c == QLatin1Char('#')) {
                ++m_hashes;
                const int pct = qMin(m_hashes, 50) + (m_phase == Verifying ? 50 : 0);
                emit progress(pct);
            } else if (c == QLatin1Char('|')) {
                m_inBar = false;
                if (m_phase == Verifying)
                    m_phase = AfterVerify;
            }
            continue;
        }
        if (m_lineBuf.endsWith(QLatin1String("Writing | "))) {
            if (m_phase == Idle) {
                m_phase = Writing;
                m_inBar = true;
                m_hashes = 0;
                emit progress(0);
            }
        } else if (m_lineBuf.endsWith(QLatin1String("Reading | "))) {
            if (m_phase == Writing) {
                m_phase = Verifying;
                m_inBar = true;
                m_hashes = 0;
                emit progress(50);
            }
        }
    }
}

Flasher::Failure Flasher::classify() const
{
    const QString o = m_allOutput.toLower();
    if (o.contains(QLatin1String("can't open device")) || o.contains(QLatin1String("ser_open"))) {
        if (o.contains(QLatin1String("denied")) || o.contains(QLatin1String("geweigerd"))
            || o.contains(QLatin1String("busy")) || o.contains(QLatin1String("in use")))
            return PortBusy;
        return PortNotFound;
    }
    if (o.contains(QLatin1String("can't open input file"))
        || (o.contains(QLatin1String("read from file")) && o.contains(QLatin1String("failed")))
        || o.contains(QLatin1String("invalid record")))
        return HexMissing;
    if (o.contains(QLatin1String("expected signature")) || o.contains(QLatin1String("invalid device signature"))
        || o.contains(QLatin1String("device signature = 0x000000")))
        return Signature;
    if (o.contains(QLatin1String("verification error")) || o.contains(QLatin1String("content mismatch")))
        return Verify;
    if (o.contains(QLatin1String("not in sync")) || o.contains(QLatin1String("stk500_recv"))
        || o.contains(QLatin1String("stk500_getsync")) || o.contains(QLatin1String("not responding")))
        return NoSync;
    return Other;
}

void Flasher::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_done)
        return;
    if (!m_lineBuf.trimmed().isEmpty())
        emit outputLine(m_lineBuf.trimmed());
    m_lineBuf.clear();

    if (m_cancelled) {
        finish(false, Cancelled, tr("Cancelled."));
        return;
    }
    if (status == QProcess::NormalExit && exitCode == 0) {
        finish(true, None, tr("Firmware written and checked."));
        return;
    }

    const Failure f = status == QProcess::NormalExit ? classify() : Other;
    if (f == NoSync && m_method == UnoNewNano115200 && m_autoRetry && !m_retried) {
        m_retried = true;
        emit outputLine(tr("No answer at 115200 baud. Trying again at 57600 baud (old Nano bootloader)..."));
        start(OldNano57600);
        return;
    }

    QString msg;
    switch (f) {
    case PortBusy:     msg = tr("The port could not be opened: another program is using it."); break;
    case PortNotFound: msg = tr("The port could not be opened: the board was not found."); break;
    case HexMissing:   msg = tr("The firmware file could not be read."); break;
    case NoSync:       msg = tr("The board did not answer."); break;
    case Signature:    msg = tr("The chip on the board was not recognised."); break;
    case Verify:       msg = tr("Writing failed: the check after writing found differences."); break;
    default:
        msg = status == QProcess::NormalExit ? tr("Uploading failed (avrdude exit code %1).").arg(exitCode)
                                             : tr("The upload program stopped unexpectedly.");
        break;
    }
    finish(false, f, msg);
}

void Flasher::onError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart || m_done)
        return;
    const QString msg = tr("The upload program (avrdude) could not be started: %1").arg(m_proc->errorString());
    emit outputLine(msg);
    finish(false, AvrdudeMissing, msg);
}

void Flasher::finish(bool ok, Failure failure, const QString &summary)
{
    m_done = true;
    m_failure = ok ? None : failure;
    if (ok)
        emit progress(100);
    emit finished(ok, summary);
}

// ---------------------------------------------------------------------------
// FlashDialog
// ---------------------------------------------------------------------------

QString FlashDialog::defaultHexPath()
{
    const QString name = QStringLiteral("GPIOcore.hex");
    QStringList candidates;
#ifdef GPIOCORE_SOURCE_DIR
    // GPIOCORE_SOURCE_DIR is software/gui; the repository root is two levels up.
    candidates << QDir::cleanPath(QStringLiteral(GPIOCORE_SOURCE_DIR) + QStringLiteral("/../../release/") + name);
#endif
    // Without the define: look for <dir>/release/GPIOcore.hex above the executable.
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        candidates << d.absoluteFilePath(QStringLiteral("release/") + name);
        if (!d.cdUp())
            break;
    }
    for (const QString &c : candidates)
        if (QFileInfo(c).isFile())
            return QDir::cleanPath(c);
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + QLatin1Char('/') + name);
}

FlashDialog::FlashDialog(const QString &preselectPort, QWidget *parent)
    : QDialog(parent)
    , m_flasher(new Flasher(this))
{
    setWindowTitle(tr("Update firmware"));
    setMinimumWidth(520);

    auto *root = new QVBoxLayout(this);

    // Intro + help button
    auto *introRow = new QHBoxLayout;
    auto *intro = new QLabel(tr("Puts the GPIOcore program (firmware) on your Arduino. "
                                "Plug the board in with USB, check the settings below and click <b>Flash</b>. "
                                "Your settings on the board are normally kept."), this);
    intro->setWordWrap(true);
    introRow->addWidget(intro, 1);
    introRow->addWidget(Info::button(QStringLiteral("flash.firmware"), this), 0, Qt::AlignTop);
    root->addLayout(introRow);

    auto *form = new QFormLayout;

    // Port
    m_port = new QComboBox(this);
    m_refresh = new QToolButton(this);
    m_refresh->setText(tr("Refresh"));
    m_refresh->setToolTip(tr("Look again for connected boards"));
    auto *portRow = new QHBoxLayout;
    portRow->addWidget(m_port, 1);
    portRow->addWidget(m_refresh);
    form->addRow(tr("Port:"), portRow);

    // Firmware file
    m_hex = new QLineEdit(QDir::toNativeSeparators(defaultHexPath()), this);
    m_browse = new QPushButton(tr("Browse…"), this);
    auto *hexRow = new QHBoxLayout;
    hexRow->addWidget(m_hex, 1);
    hexRow->addWidget(m_browse);
    form->addRow(tr("Firmware file:"), hexRow);

    // Board type / method
    m_method = new QComboBox(this);
    m_method->addItem(tr("Arduino Uno / Nano (new bootloader)"), int(Flasher::UnoNewNano115200));
    m_method->addItem(tr("Arduino Nano (old bootloader)"),       int(Flasher::OldNano57600));
    m_method->addItem(tr("Via ArduinoISP programmer (advanced)"), int(Flasher::ArduinoIsp));
    {
        const int saved = QSettings().value(QStringLiteral("flash/method"), int(Flasher::UnoNewNano115200)).toInt();
        const int idx = m_method->findData(saved);
        m_method->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    form->addRow(tr("Board type:"), m_method);
    root->addLayout(form);

    m_ispNote = new QLabel(tr("<b>Advanced:</b> choose the port of the <i>programmer</i> Arduino (running ArduinoISP), "
                              "wired to the ICSP pins of your board. This also sets the fuses and "
                              "<b>removes the USB bootloader</b>: afterwards the board can only be updated "
                              "with the programmer again. The settings stored on the board are erased."), this);
    m_ispNote->setWordWrap(true);
    m_ispNote->setStyleSheet(QStringLiteral("color: #b35900;"));
    root->addWidget(m_ispNote);

    // Flash button + progress
    m_flashBtn = new QPushButton(tr("Flash"), this);
    m_flashBtn->setDefault(true);
    m_bar = new QProgressBar(this);
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
    auto *runRow = new QHBoxLayout;
    runRow->addWidget(m_flashBtn);
    runRow->addWidget(m_bar, 1);
    root->addLayout(runRow);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    m_result = new QLabel(this);
    m_result->setWordWrap(true);
    m_result->setTextFormat(Qt::RichText);
    m_result->hide();
    root->addWidget(m_result);

    // Collapsible log
    m_detailsBtn = new QToolButton(this);
    m_detailsBtn->setCheckable(true);
    m_detailsBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_detailsBtn->setAutoRaise(true);
    root->addWidget(m_detailsBtn, 0, Qt::AlignLeft);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_log->setMaximumBlockCount(5000);
    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    m_log->setFont(mono);
    m_log->setMinimumHeight(180);
    root->addWidget(m_log, 1);
    setDetailsVisible(false);

    // Close
    m_closeBtn = new QPushButton(tr("Close"), this);
    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    bottom->addWidget(m_closeBtn);
    root->addLayout(bottom);

    refreshPorts(preselectPort);

    auto updateIspNote = [this] {
        m_ispNote->setVisible(m_method->currentData().toInt() == int(Flasher::ArduinoIsp));
    };
    updateIspNote();

    connect(m_refresh, &QToolButton::clicked, this, [this] { refreshPorts(m_port->currentData().toString()); });
    connect(m_browse, &QPushButton::clicked, this, &FlashDialog::browseHex);
    connect(m_method, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateIspNote);
    connect(m_flashBtn, &QPushButton::clicked, this, &FlashDialog::startFlash);
    connect(m_closeBtn, &QPushButton::clicked, this, &FlashDialog::reject);
    connect(m_detailsBtn, &QToolButton::toggled, this, &FlashDialog::setDetailsVisible);

    connect(m_flasher, &Flasher::outputLine, this, &FlashDialog::onOutputLine);
    connect(m_flasher, &Flasher::progress, this, &FlashDialog::onProgress);
    connect(m_flasher, &Flasher::finished, this, &FlashDialog::onFinished);
    connect(m_flasher, &Flasher::started, this, [this](Flasher::Method m) {
        m_status->setText(m == Flasher::OldNano57600 && m_method->currentData().toInt() != int(Flasher::OldNano57600)
                              ? tr("Trying again with the old Nano bootloader…")
                              : tr("Uploading… do not unplug the board."));
    });
}

bool FlashDialog::isBusy() const
{
    return m_flasher->isRunning();
}

void FlashDialog::refreshPorts(const QString &select)
{
    m_port->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &p : ports) {
        QString text = p.portName();
        if (!p.description().isEmpty())
            text += QStringLiteral(" – ") + p.description();
        m_port->addItem(text, p.portName());
    }
    if (m_port->count() == 0) {
        m_port->addItem(tr("(no board found – plug it in and click Refresh)"), QString());
        m_port->setEnabled(false);
    } else {
        m_port->setEnabled(true);
        const int idx = m_port->findData(select);
        if (idx >= 0)
            m_port->setCurrentIndex(idx);
    }
    if (m_flashBtn)
        m_flashBtn->setEnabled(m_port->isEnabled() && !isBusy());
}

void FlashDialog::browseHex()
{
    const QString start = m_hex->text().isEmpty() ? defaultHexPath() : m_hex->text();
    const QString f = QFileDialog::getOpenFileName(this, tr("Choose firmware file"), start,
                                                   tr("Firmware (*.hex);;All files (*)"));
    if (!f.isEmpty())
        m_hex->setText(QDir::toNativeSeparators(f));
}

void FlashDialog::startFlash()
{
    const QString port = m_port->currentData().toString();
    const QString hex = QDir::fromNativeSeparators(m_hex->text().trimmed());
    const auto method = Flasher::Method(m_method->currentData().toInt());

    m_result->hide();
    m_log->clear();

    if (!QFileInfo(hex).isFile()) {
        showResult(false, tr("The firmware file was not found.") + QStringLiteral("<br>")
                              + failureHint(Flasher::HexMissing));
        return;
    }
    if (port.isEmpty()) {
        showResult(false, tr("No board found.") + QStringLiteral("<br>") + failureHint(Flasher::PortNotFound));
        return;
    }

    QSettings().setValue(QStringLiteral("flash/method"), int(method));
    emit aboutToFlash();   // caller closes its own connection to the port now
    setBusy(true);
    m_bar->setValue(0);
    m_status->setText(tr("Uploading… do not unplug the board."));
    m_flasher->flash(port, hex, method);
}

void FlashDialog::onOutputLine(const QString &line)
{
    m_log->appendPlainText(line);
}

void FlashDialog::onProgress(int percent)
{
    if (percent < 0) {
        m_bar->setRange(0, 0);   // busy indicator
    } else {
        m_bar->setRange(0, 100);
        m_bar->setValue(percent);
    }
}

void FlashDialog::onFinished(bool ok, const QString &summary)
{
    setBusy(false);
    m_bar->setRange(0, 100);
    m_status->clear();
    if (ok) {
        m_bar->setValue(100);
        QString text = tr("Done! The new firmware is on the board.");
        if (m_flasher->usedMethod() == Flasher::OldNano57600
            && m_method->currentData().toInt() == int(Flasher::UnoNewNano115200)) {
            text += QStringLiteral("<br>") + tr("Your board has the old Nano bootloader. "
                                                "Next time choose \"Arduino Nano (old bootloader)\".");
            m_method->setCurrentIndex(m_method->findData(int(Flasher::OldNano57600)));
            QSettings().setValue(QStringLiteral("flash/method"), int(Flasher::OldNano57600));
        }
        showResult(true, text);
    } else {
        m_bar->setValue(0);
        QString text = tr("Uploading did not work.") + QLatin1Char(' ') + summary.toHtmlEscaped();
        const QString hint = failureHint(m_flasher->lastFailure());
        if (!hint.isEmpty())
            text += QStringLiteral("<br>") + hint;
        showResult(false, text);
    }
    emit flashFinished(ok);
}

QString FlashDialog::failureHint(Flasher::Failure f) const
{
    const bool isp = m_method->currentData().toInt() == int(Flasher::ArduinoIsp);
    switch (f) {
    case Flasher::AvrdudeMissing:
        return tr("The folder <i>tools/avrdude</i> next to GPIOcoreGUI is missing. Install the program again.");
    case Flasher::HexMissing:
        return tr("Click <b>Browse…</b> and choose the GPIOcore <i>.hex</i> file.");
    case Flasher::PortBusy:
        return tr("Close other programs that use the board (Arduino IDE serial monitor, other configuration "
                  "tools) and try again.");
    case Flasher::PortNotFound:
        return tr("Check the USB cable, click <b>Refresh</b> and choose the right port.");
    case Flasher::NoSync:
        if (isp)
            return tr("Check that the programmer Arduino runs ArduinoISP, that the right port is chosen "
                      "and that the wires to the board are correct. A 10 µF capacitor between RESET and GND "
                      "of the programmer often helps.");
        return tr("Choose another <b>board type</b> (for example \"Arduino Nano (old bootloader)\") and try "
                  "again. Also check that the right port is chosen and close other programs that use the board.");
    case Flasher::Signature:
        return isp ? tr("Check the six wires between the programmer and the board, and that the board has power.")
                   : tr("This board does not have an ATmega328P chip. GPIOcore needs an Arduino Uno or Nano.");
    case Flasher::Verify:
        return tr("Try again. If it keeps failing, try another USB cable or USB port.");
    case Flasher::Cancelled:
        return QString();
    default:
        return tr("Click <b>Show details</b> to see what happened.");
    }
}

void FlashDialog::showResult(bool ok, const QString &text)
{
    m_result->setStyleSheet(ok ? QStringLiteral("color: #1b7f2a; font-weight: bold;")
                               : QStringLiteral("color: #c62828;"));
    m_result->setText(ok ? text : QStringLiteral("<b>%1</b>").arg(text));
    m_result->show();
}

void FlashDialog::setBusy(bool busy)
{
    const bool hasPort = !m_port->currentData().toString().isEmpty();
    m_flashBtn->setEnabled(!busy && hasPort);
    m_closeBtn->setEnabled(!busy);
    m_port->setEnabled(!busy && hasPort);
    m_refresh->setEnabled(!busy);
    m_hex->setEnabled(!busy);
    m_browse->setEnabled(!busy);
    m_method->setEnabled(!busy);
}

void FlashDialog::setDetailsVisible(bool visible)
{
    m_detailsBtn->setText(visible ? tr("Hide details") : tr("Show details"));
    m_detailsBtn->setArrowType(visible ? Qt::DownArrow : Qt::RightArrow);
    if (m_detailsBtn->isChecked() != visible)
        m_detailsBtn->setChecked(visible);
    m_log->setVisible(visible);
    if (!visible)
        adjustSize();
}

void FlashDialog::reject()
{
    if (isBusy())
        return;   // never interrupt avrdude halfway: that can leave the board without firmware
    QDialog::reject();
}

void FlashDialog::closeEvent(QCloseEvent *e)
{
    if (isBusy()) {
        e->ignore();
        return;
    }
    QDialog::closeEvent(e);
}
