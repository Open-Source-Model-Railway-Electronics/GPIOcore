#include "i18n.h"
#include "mainwindow.h"
#include "soundplayer.h"
#include "uihelpers.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IOcore Configurator"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/ui/oshw.svg")));   // the Open Source Hardware gear
    QApplication::setOrganizationName(QStringLiteral("Train-Science"));
    QApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));
    Ui::applyAppStyle(app);

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("Configure IOcore boards over the serial port."));
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption demo(QStringLiteral("demo"), QStringLiteral("Connect to the built-in firmware simulator."));
    QCommandLineOption port(QStringLiteral("port"), QStringLiteral("Connect to this serial port at start."), QStringLiteral("name"));
    QCommandLineOption shot(QStringLiteral("screenshot"), QStringLiteral("Save a screenshot after reading the board, then quit."), QStringLiteral("file"));
    QCommandLineOption tab(QStringLiteral("tab"), QStringLiteral("Panel for --screenshot: outputs or inputs."), QStringLiteral("tab"), QStringLiteral("outputs"));
    QCommandLineOption select(QStringLiteral("select"), QStringLiteral("Row to select for --screenshot."), QStringLiteral("n"), QStringLiteral("0"));
    QCommandLineOption preset(QStringLiteral("preset"), QStringLiteral("Apply this preset after connecting (no confirmation): its name or file name, e.g. 16-servos."), QStringLiteral("name"));
    QCommandLineOption open(QStringLiteral("open"), QStringLiteral("Load this configuration file at start."), QStringLiteral("file"));
    QCommandLineOption size(QStringLiteral("size"), QStringLiteral("Window size, e.g. 1680x900 (for screenshots)."), QStringLiteral("WxH"));
    QCommandLineOption theme(QStringLiteral("theme"), QStringLiteral("auto, light or dark for this run (not saved)."), QStringLiteral("theme"));
    QCommandLineOption lang(QStringLiteral("lang"), QStringLiteral("Language for this run, e.g. en or nl (not saved)."), QStringLiteral("code"));
    cli.addOptions({ demo, port, shot, tab, select, preset, open, size, theme, lang });
    cli.process(app);
    if (cli.isSet(theme))
    {
        Ui::setThemeOverride(cli.value(theme));
        Ui::applyAppStyle(app);
    }
    Lang::install(app, cli.value(lang)); // before any window is created

    // The user's folders in Documents\IOcore, so they are there to find from the first start
    // (here and not in the installer: it runs as administrator, maybe as another user)
    QDir().mkpath(MainWindow::boardsDir());
    if (QSettings().value(QStringLiteral("sound/folder")).toString().isEmpty())
        QDir().mkpath(SoundPlayer::defaultFolder());

    MainWindow w;
    if (cli.isSet(preset))
        w.applyPresetOnStart(cli.value(preset));
    if (cli.isSet(shot))
        w.scheduleScreenshot(cli.value(shot), cli.value(tab), cli.value(select).toInt());
    if (cli.isSet(size))
    {
        const QStringList wh = cli.value(size).split(QLatin1Char('x'));
        if (wh.size() == 2)
            w.resize(wh[0].toInt(), wh[1].toInt());
    }
    if (cli.isSet(size)) w.show();       // screenshots: exactly this size
    else                 w.showMaximized();
    if (cli.isSet(open))
        w.loadFile(cli.value(open));

    if (cli.isSet(demo))      w.connectDemo();
    else if (cli.isSet(port)) w.connectPort(cli.value(port));

    return app.exec();
}
