#pragma once
#include <QString>

class QToolButton;
class QWidget;

namespace Info {

// HTML help text for a topic, e.g. "output.frog", in the current language
// (i18n/info/<lang>/<topic>.html, falls back to English).
QString text(const QString &key);

// Small (i) button that shows the topic in a popup when clicked.
QToolButton *button(const QString &key, QWidget *parent = nullptr);

// Larger window for long texts (Help menu).
void showDialog(const QString &key, const QString &title, QWidget *parent);

} // namespace Info
