#include "pinmapview.h"
#include "uihelpers.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

using namespace Gpio;

static constexpr int Cols = 5;
static constexpr int Rows = (NumPins + Cols - 1) / Cols;

PinMapView::PinMapView(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(560, 400);
    setMouseTracking(true);
}

void PinMapView::setBoard(const BoardConfig &board)
{
    m_board = board;
    update();
}

QRectF PinMapView::cardRect(int pin) const
{
    const qreal margin = 16, gap = 10, top = 46;
    const qreal w = (width()  - 2 * margin - (Cols - 1) * gap) / Cols;
    const qreal h = (height() - top - margin - (Rows - 1) * gap) / Rows;
    const int r = pin / Cols, c = pin % Cols;
    return QRectF(margin + c * (w + gap), top + r * (h + gap), w, h);
}

void PinMapView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QFont titleFont = font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 2);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(Ui::themed(QColor(0x0F, 0x17, 0x2A)));
    p.drawText(QRectF(16, 8, width() - 32, 30), Qt::AlignLeft | Qt::AlignVCenter,
               tr("Pin usage (Arduino Uno / Nano)"));

    QFont small = font();
    small.setPointSizeF(qMax(7.0, font().pointSizeF() - 1));
    p.setFont(small);
    p.setPen(Ui::themed(QColor(0x64, 0x74, 0x8B)));
    p.drawText(QRectF(16, 8, width() - 32, 30), Qt::AlignRight | Qt::AlignVCenter,
               tr("click a card to edit  ·  red = conflict"));

    const auto usage = Ui::pinUsage(m_board);

    for (int pin = 0; pin < NumPins; ++pin)
    {
        const QRectF r = cardRect(pin);
        const auto &users = usage[pin];
        const QString reserved = pinReservedReason(quint8(pin));
        const bool conflict = Ui::pinClash(users) || (!reserved.isEmpty() && !users.isEmpty());

        QColor accent = Ui::themed(QColor(0xE2, 0xE8, 0xF0));
        if (!users.isEmpty())
        {
            const Ui::PinUse &u = users.first();
            accent = u.output ? Ui::outputTypeColor(m_board.outputs[u.index].type)
                              : Ui::inputTypeColor(m_board.inputs[u.index].type);
        }

        QPainterPath path;
        path.addRoundedRect(r, 8, 8);
        p.fillPath(path, reserved.isEmpty() ? Ui::themed(QColor(Qt::white)) : Ui::themed(QColor(0xF1, 0xF5, 0xF9)));
        p.setPen(QPen(conflict ? QColor(0xDC, 0x26, 0x26) : Ui::themed(QColor(0xE2, 0xE8, 0xF0)), conflict ? 2.0 : 1.0));
        p.drawPath(path);

        // accent bar
        QPainterPath bar;
        bar.addRoundedRect(QRectF(r.left(), r.top(), 5, r.height()), 2.5, 2.5);
        p.fillPath(bar, accent);

        // pin name
        QFont nameFont = font();
        nameFont.setBold(true);
        nameFont.setPointSizeF(font().pointSizeF() + 1);
        p.setFont(nameFont);
        p.setPen(Ui::themed(QColor(0x0F, 0x17, 0x2A)));
        p.drawText(QRectF(r.left() + 14, r.top() + 6, r.width() - 20, 22), Qt::AlignLeft | Qt::AlignVCenter,
                   pinName(quint8(pin)));

        p.setFont(small);
        if (isPwmPin(quint8(pin)))
        {
            const QRectF badge(r.right() - 44, r.top() + 8, 36, 17);
            QPainterPath b; b.addRoundedRect(badge, 8, 8);
            p.fillPath(b, Ui::themed(QColor(0xFC, 0xE7, 0xF3)));
            p.setPen(Ui::themed(QColor(0x9D, 0x17, 0x4D)));
            p.drawText(badge, Qt::AlignCenter, QStringLiteral("PWM"));
        }

        QStringList lines;
        if (!reserved.isEmpty())
            lines << reserved;
        for (const Ui::PinUse &u : users)
        {
            if (u.output)
                lines << tr("Out %1 %2 · %3").arg(u.index).arg(u.role,
                         outputTypeName(m_board.outputs[u.index].type));
            else
                lines << tr("In %1 · %2").arg(u.index)
                         .arg(inputTypeName(m_board.inputs[u.index].type));
        }
        if (lines.isEmpty())
            lines << tr("free");

        p.setPen(users.isEmpty() ? QColor(0x94, 0xA3, 0xB8)
                                 : (conflict ? QColor(0xB9, 0x1C, 0x1C) : QColor(0x33, 0x41, 0x55)));
        p.drawText(QRectF(r.left() + 14, r.top() + 30, r.width() - 20, r.height() - 34),
                   Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, lines.join('\n'));
    }
}

void PinMapView::mousePressEvent(QMouseEvent *e)
{
    const auto usage = Ui::pinUsage(m_board);
    for (int pin = 0; pin < NumPins; ++pin)
    {
        if (!cardRect(pin).contains(e->pos()) || usage[pin].isEmpty())
            continue;
        const Ui::PinUse &u = usage[pin].first();
        if (u.output) emit outputActivated(u.index);
        else          emit inputActivated(u.index);
        return;
    }
}
