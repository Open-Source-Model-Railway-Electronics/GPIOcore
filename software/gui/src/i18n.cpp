#include "i18n.h"

#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QTranslator>

static QString g_current = QStringLiteral("en");

QString Lang::current()
{
    return g_current;
}

QStringList Lang::available()
{
    QStringList langs { QStringLiteral("en") };
    // compiled translations: :/i18n/gpiocore_<lang>.qm
    for (const QString &f : QDir(QStringLiteral(":/i18n")).entryList({ QStringLiteral("gpiocore_*.qm") }))
        langs << f.mid(9, f.size() - 9 - 3);
    // help texts, compiled in or next to the program (i18n/info/<lang>/)
    for (const QString &base : { QStringLiteral(":/info"),
                                 QCoreApplication::applicationDirPath() + QStringLiteral("/i18n/info") })
        langs << QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    langs.removeDuplicates();
    langs.sort();
    return langs;
}

void Lang::install(QApplication &app, const QString &force)
{
    QString code = force;                           // English only: no configured or system language
    if (!available().contains(code))
        code = QStringLiteral("en");
    g_current = code;
    if (code == QLatin1String("en"))
        return;                                     // the sources are English

    auto *own = new QTranslator(&app);
    if (own->load(QStringLiteral(":/i18n/gpiocore_%1.qm").arg(code)))
        app.installTranslator(own);

    // Qt's own texts (standard dialogs: Yes / No, Open / Save, ...)
    auto *qt = new QTranslator(&app);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QString qtDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    const QString qtDir = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    if (qt->load(QStringLiteral("qtbase_%1").arg(code), qtDir))
        app.installTranslator(qt);
}
