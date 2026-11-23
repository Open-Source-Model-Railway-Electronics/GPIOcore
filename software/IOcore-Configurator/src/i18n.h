#pragma once
// Language handling. See i18n/README.md for how to add a language.
//
//  - Short UI texts: tr() / QT_TRANSLATE_NOOP in the sources, translated in
//    i18n/iocore_<lang>.ts with Qt Linguist, compiled into the program.
//  - Long help texts: i18n/info/<lang>/<topic>.html, one file per topic.
//    A missing file falls back to English.

#include <QString>
#include <QStringList>

class QApplication;

namespace Lang {

void        install(QApplication &app, const QString &force = QString());   // before creating windows; force: this run only
QString     current();                    // "en", "nl", ...
QString     configured();                 // "" = follow the system language
void        setConfigured(const QString &code);   // takes effect after a restart
QStringList available();                  // languages with a .qm file or help texts

} // namespace Lang
