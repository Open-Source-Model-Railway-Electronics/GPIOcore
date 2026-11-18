// Help texts behind the (i) buttons and the Help menu.
//
// The texts are HTML files: i18n/info/<language>/<topic>.html. They are compiled
// into the program; a folder i18n/info/ next to the .exe overrides them, so a
// translator can edit and check texts without rebuilding.

#include "infotexts.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

static QString readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

QString Info::text(const QString &key)
{
    // English only for now: the other languages' texts stay in i18n/info/ for later
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/i18n/info/");
    for (const QString &base : { local, QStringLiteral(":/info/") })
    {
        const QString html = readFile(base + QStringLiteral("en/") + key + QStringLiteral(".html"));
        if (!html.isEmpty())
            return html;
    }
    return QCoreApplication::translate("Info", "No help text for '%1' yet.").arg(key);
}

QToolButton *Info::button(const QString &key, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setObjectName(QStringLiteral("info"));
    b->setText(QStringLiteral("i"));
    b->setToolTip(QCoreApplication::translate("Info", "What is this? Click for an explanation."));
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoRaise(true);
    b->setFixedSize(20, 20);
    QObject::connect(b, &QToolButton::clicked, b, [b, key] {
        auto *popup = new QFrame(b, Qt::Popup);
        popup->setObjectName(QStringLiteral("infoPopup"));
        popup->setAttribute(Qt::WA_DeleteOnClose);
        auto *lay = new QVBoxLayout(popup);
        lay->setContentsMargins(4, 4, 4, 4);

        auto *label = new QLabel(Info::text(key));
        label->setObjectName(QStringLiteral("infoText"));
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);
        label->setOpenExternalLinks(true);
        label->setContentsMargins(12, 8, 12, 10);
        label->setFixedWidth(500);

        auto *scroll = new QScrollArea;
        scroll->setWidget(label);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        label->adjustSize();
        scroll->setFixedWidth(label->width() + 18);
        scroll->setFixedHeight(qMin(label->sizeHint().height() + 4, 460));
        lay->addWidget(scroll);
        popup->adjustSize();

        QPoint pos = b->mapToGlobal(QPoint(0, b->height() + 4));
        if (const QScreen *screen = QGuiApplication::screenAt(pos))
        {
            const QRect avail = screen->availableGeometry();
            pos.setX(qMax(avail.left() + 8, qMin(pos.x(), avail.right() - popup->width() - 8)));
            if (pos.y() + popup->height() > avail.bottom())
                pos.setY(qMax(avail.top() + 8, b->mapToGlobal(QPoint(0, 0)).y() - popup->height() - 4));
        }
        popup->move(pos);
        popup->show();
    });
    return b;
}

void Info::showDialog(const QString &key, const QString &title, QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.resize(720, 640);
    auto *lay = new QVBoxLayout(&dlg);
    auto *view = new QTextBrowser;
    view->setOpenExternalLinks(true);
    view->setHtml(Info::text(key));
    lay->addWidget(view);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(buttons);
    dlg.exec();
}
