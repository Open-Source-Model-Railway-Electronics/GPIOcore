#pragma once
// Language handling. For now the GUI always runs in English: the configured and the system language are
// ignored, only --lang (screenshots) loads a translation. See i18n/README.md for how to add a language.
//
//  - Short UI texts: tr() / QT_TRANSLATE_NOOP in the sources, translated in
//    i18n/gpiocore_<lang>.ts with Qt Linguist, compiled into the program.
//  - Long help texts: i18n/info/<lang>/<topic>.html, one file per topic; the GUI shows the English ones.

#include <QString>
#include <QStringList>

class QApplication;

namespace Lang {

void        install(QApplication &app, const QString &force = QString());   // before creating windows; force: this run only
QString     current();                    // "en", "nl", ...
QStringList available();                  // languages with a .qm file or help texts

} // namespace Lang
