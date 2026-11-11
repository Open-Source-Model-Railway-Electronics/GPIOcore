#include "connectionview.h"
#include "connections.h"
#include "uihelpers.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QPainterPathStroker>
#include <QToolTip>

using namespace Gpio;

static constexpr qreal Margin = 16;
static constexpr qreal Top    = 44;     // below the column headers
static constexpr qreal NodeH  = 46;
static constexpr qreal Gap    = 8;

static const QColor kText   (0x0F, 0x17, 0x2A);
static const QColor kMuted  (0x64, 0x74, 0x8B);
static const QColor kBorder (0xE2, 0xE8, 0xF0);
static const QColor kIdle   (0xCB, 0xD5, 0xE1);
static const QColor kAccent (0x25, 0x63, 0xEB);
static const QColor kRed    (0xDC, 0x26, 0x26);
static const QColor kAmber  (0xD9, 0x77, 0x06);

ConnectionView::ConnectionView(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("connectionView"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_inDirty = m_inConflict = QVector<bool>(NumInputs, false);
    m_outDirty = m_outConflict = QVector<bool>(NumOutputs, false);
}

QSize ConnectionView::minimumSizeHint() const
{
    int rows = 1;
    for (int r : m_inRow)  rows = qMax(rows, r + 1);
    for (int r : m_outRow) rows = qMax(rows, r + 1);
    return QSize(620, int(Top + rows * (NodeH + Gap) + Margin));
}

void ConnectionView::setHideUnused(bool hide)
{
    m_hideUnused = hide;
    updateRows();
    rebuildWires();
    update();
}

void ConnectionView::updateRows()
{
    m_inRow.fill(-1, NumInputs);
    m_outRow.fill(-1, NumOutputs);
    int r = 0;
    for (int i = 0; i < NumInputs; ++i)
        if (!m_hideUnused || m_board.inputs[i].type != InputNotInUse || i == m_selInput)
            m_inRow[i] = r++;
    r = 0;
    for (int o = 0; o < NumOutputs; ++o)
        if (!m_hideUnused || m_board.outputs[o].type != OutputNotInUse || o == m_selOutput
            || !outputSources(m_board, o).isEmpty())
            m_outRow[o] = r++;
    updateGeometry();
}

QSize ConnectionView::sizeHint() const
{
    return QSize(780, minimumSizeHint().height());
}

void ConnectionView::setBoard(const BoardConfig &board)
{
    m_board = board;
    m_selWire = m_hoverWire = -1;
    updateRows();
    rebuildWires();
    update();
}

void ConnectionView::setMarks(const QVector<bool> &inputDirty, const QVector<bool> &outputDirty,
                              const QVector<bool> &inputConflict, const QVector<bool> &outputConflict)
{
    m_inDirty = inputDirty;     m_outDirty = outputDirty;
    m_inConflict = inputConflict; m_outConflict = outputConflict;
    update();
}

void ConnectionView::selectInput(int index)  { m_selInput = index; m_selOutput = -1; updateRows(); rebuildWires(); update(); }
void ConnectionView::selectOutput(int index) { m_selOutput = index; m_selInput = -1; updateRows(); rebuildWires(); update(); }

// ── Colours ─────────────────────────────────────────────────────────────────
QColor ConnectionView::inputColor(int i) const
{
    return inputConnected(m_board.inputs[i]) ? Ui::inputColor(i) : kIdle;
}

QColor ConnectionView::outputColor(int o) const
{
    const QVector<int> src = outputSources(m_board, o);
    return src.isEmpty() ? kIdle : Ui::inputColor(src.first());
}

// ── Geometry ────────────────────────────────────────────────────────────────
qreal ConnectionView::nodeWidth() const
{
    return qBound<qreal>(190, (width() - 2 * Margin) * 0.32, 270);
}

QRectF ConnectionView::inputRect(int i) const
{
    const int row = m_inRow.value(i, i);
    if (row < 0) return QRectF();
    return QRectF(Margin, Top + row * (NodeH + Gap), nodeWidth(), NodeH);
}

QRectF ConnectionView::outputRect(int o) const
{
    const int row = m_outRow.value(o, o);
    if (row < 0) return QRectF();
    const qreal w = nodeWidth();
    return QRectF(width() - Margin - w, Top + row * (NodeH + Gap), w, NodeH);
}

QPointF ConnectionView::inputPort(int i) const
{
    const QRectF r = inputRect(i);
    return QPointF(r.right(), r.center().y());
}

QPointF ConnectionView::outputPort(int o) const
{
    const QRectF r = outputRect(o);
    return QPointF(r.left(), r.center().y());
}

QFont ConnectionView::smallFont() const
{
    QFont f = font();
    f.setPointSizeF(qMax(7.0, font().pointSizeF() - 1));
    return f;
}

QPointF ConnectionView::routeLabelPos(int input, int step) const
{
    for (const Wire &w : m_wires)
        if (w.input == input && w.step == step)
            return w.label;
    return QPointF(-1, -1);
}

QPainterPath ConnectionView::curve(const QPointF &a, const QPointF &b)
{
    const qreal dx = qMax<qreal>(60, qAbs(b.x() - a.x()) * 0.5);
    QPainterPath path(a);
    path.cubicTo(a + QPointF(dx, 0), b - QPointF(dx, 0), b);
    return path;
}

void ConnectionView::rebuildWires()
{
    m_wires.clear();
    QHash<int, int> labelsAt;                  // route labels per output: spread them along the wire
    for (int i = 0; i < NumInputs; ++i)
    {
        const InputConfig &c = m_board.inputs[i];
        if (isDirectType(c.type) && c.outputId < NumOutputs)
        {
            Wire w;
            w.input = i; w.output = c.outputId; w.step = -1;
            w.path  = curve(inputPort(i), outputPort(w.output));
            w.label = w.path.pointAtPercent(0.5);
            m_wires.append(w);
        }
        else if (isRouteType(c.type))
        {
            const QVector<RouteStep> steps = routeSteps(c);
            for (int s = 0; s < steps.size(); ++s)
            {
                const int dup = labelsAt[steps[s].output]++;
                Wire w;
                w.input = i; w.output = steps[s].output; w.step = s; w.data = steps[s];
                w.path  = curve(inputPort(i), outputPort(w.output));
                w.label = w.path.pointAtPercent(qMax(0.3, 0.82 - 0.14 * dup));
                w.text  = QStringLiteral("%1 %2").arg(s + 1)
                              .arg(Ui::stepValueText(steps[s], m_board.outputs[w.output].type));
                const qreal tw = QFontMetricsF(smallFont()).horizontalAdvance(w.text) + 12;
                w.labelRect = QRectF(w.label - QPointF(tw / 2, 9), QSizeF(tw, 18));
                m_wires.append(w);
            }
        }
    }
}

int ConnectionView::inputAt(const QPointF &p) const
{
    for (int i = 0; i < NumInputs; ++i)
        if (inputShown(i) && inputRect(i).adjusted(0, 0, 10, 0).contains(p)) return i;
    return -1;
}

int ConnectionView::outputAt(const QPointF &p) const
{
    for (int o = 0; o < NumOutputs; ++o)
        if (outputShown(o) && outputRect(o).adjusted(-10, 0, 0, 0).contains(p)) return o;
    return -1;
}

int ConnectionView::wireAt(const QPointF &p) const
{
    QPainterPathStroker stroker;
    stroker.setWidth(10);
    for (int w = m_wires.size() - 1; w >= 0; --w)
    {
        if (m_wires[w].step >= 0 && m_wires[w].labelRect.contains(p))
            return w;
        if (stroker.createStroke(m_wires[w].path).contains(p))
            return w;
    }
    return -1;
}

// ── Painting ────────────────────────────────────────────────────────────────
static QString inputModeText(const InputConfig &c)
{
    return inputTypeName(c.type);
}

static QString inputSubtitle(const InputConfig &c)
{
    QStringList parts;
    if (c.address != AddrNone && inputTypeInfo(c.type).usesAddress)
        parts << QStringLiteral("DCC %1").arg(c.address);
    if (c.pin < NumPins)
        parts << pinName(c.pin);
    return parts.isEmpty() ? QObject::tr("no trigger") : parts.join(QStringLiteral(" · "));
}

static void drawBadge(QPainter &p, const QRectF &r, const QString &text, const QColor &bg, const QColor &fg)
{
    QPainterPath b;
    b.addRoundedRect(r, r.height() / 2, r.height() / 2);
    p.fillPath(b, bg);
    p.setPen(fg);
    p.drawText(r, Qt::AlignCenter, text);
}

void ConnectionView::drawInput(QPainter &p, int i)
{
    const InputConfig &c = m_board.inputs[i];
    const QRectF r = inputRect(i);
    const QColor col = inputColor(i);
    const bool sel = (i == m_selInput);
    const bool hot = (m_dragging && m_drag == Drag::FromOutput && inputAt(m_dragPos) == i);

    QPainterPath node;
    node.addRoundedRect(r, 8, 8);
    p.fillPath(node, Qt::white);
    p.setPen(QPen(sel || hot ? kAccent : kBorder, sel || hot ? 2.0 : 1.0));
    p.drawPath(node);

    QPainterPath bar;
    bar.addRoundedRect(QRectF(r.left(), r.top(), 6, r.height()), 3, 3);
    p.fillPath(bar, col);

    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.setPen(kText);
    QString title = (m_inDirty.value(i) ? QStringLiteral("   ") : QString()) + tr("In %1").arg(i);
    if (!c.name.isEmpty())
        title += QStringLiteral(" · ") + c.name;
    title = p.fontMetrics().elidedText(title, Qt::ElideRight, int(r.width() - 110));
    p.drawText(QRectF(r.left() + 14, r.top() + 4, r.width() - 110, 20), Qt::AlignLeft | Qt::AlignVCenter, title);
    if (m_inDirty.value(i))
    {
        p.setPen(kAmber);
        p.drawText(QRectF(r.left() + 14, r.top() + 4, 12, 20), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("●"));
    }

    QFont small = font();
    small.setPointSizeF(qMax(7.0, font().pointSizeF() - 1));
    p.setFont(small);
    const bool idle = (c.type == InputNotInUse);
    drawBadge(p, QRectF(r.right() - 96, r.top() + 6, 84, 17), inputModeText(c),
              idle ? QColor(0xF1, 0xF5, 0xF9) : Ui::inputTypeColor(c.type).lighter(185),
              idle ? kMuted : Ui::inputTypeColor(c.type).darker(170));
    p.setPen(m_inConflict.value(i) ? kRed : kMuted);
    p.drawText(QRectF(r.left() + 14, r.top() + 24, r.width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter,
               inputSubtitle(c));

    // port
    const QPointF port = inputPort(i);
    p.setPen(QPen(col.darker(115), 1.5));
    p.setBrush(inputConnected(c) ? col : QColor(Qt::white));
    p.drawEllipse(port, 6, 6);
    p.setBrush(Qt::NoBrush);
}

void ConnectionView::drawOutput(QPainter &p, int o)
{
    const OutputConfig &c = m_board.outputs[o];
    const QRectF r = outputRect(o);
    const QColor col = outputColor(o);
    const bool sel = (o == m_selOutput);
    const bool hot = (m_dragging && m_drag == Drag::FromInput && outputAt(m_dragPos) == o);

    QPainterPath node;
    node.addRoundedRect(r, 8, 8);
    p.fillPath(node, c.type == OutputNotInUse ? QColor(0xF8, 0xFA, 0xFC) : QColor(Qt::white));
    p.setPen(QPen(sel || hot ? kAccent : kBorder, sel || hot ? 2.0 : 1.0));
    p.drawPath(node);

    QPainterPath bar;
    bar.addRoundedRect(QRectF(r.left(), r.top(), 6, r.height()), 3, 3);
    p.fillPath(bar, col);

    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.setPen(kText);
    p.drawText(QRectF(r.left() + 14, r.top() + 4, 80, 20), Qt::AlignLeft | Qt::AlignVCenter,
               (m_outDirty.value(o) ? QStringLiteral("   ") : QString()) + tr("Out %1").arg(o));
    if (!c.name.isEmpty())   // the name replaces the type text below the title
    {
        QFont nf = font();
        nf.setItalic(true);
        p.setFont(nf);
        p.setPen(kMuted);
        p.drawText(QRectF(r.left() + 14, r.top() + 24, r.width() - 40, 18), Qt::AlignRight | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(c.name, Qt::ElideRight, int(r.width() / 2)));
        p.setFont(f);
    }
    if (m_outDirty.value(o))
    {
        p.setPen(kAmber);
        p.drawText(QRectF(r.left() + 14, r.top() + 4, 12, 20), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("●"));
    }

    QFont small = font();
    small.setPointSizeF(qMax(7.0, font().pointSizeF() - 1));
    p.setFont(small);

    // type with swatch
    const QRectF typeRect(r.left() + 78, r.top() + 6, r.width() - 90, 17);
    p.fillRect(QRectF(typeRect.left(), typeRect.top() + 3, 10, 10), Ui::outputTypeColor(c.type));
    p.setPen(kText);
    p.drawText(typeRect.adjusted(15, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
               outputTypeName(c.type));

    QString sub = Ui::outputSummaryPins(c);
    const QString relays = Ui::outputSummaryRelays(c);
    if (!relays.isEmpty()) sub += QStringLiteral("  ·  ") + relays;
    p.setPen(m_outConflict.value(o) ? kRed : kMuted);
    p.drawText(QRectF(r.left() + 14, r.top() + 24, r.width() - 40, 18), Qt::AlignLeft | Qt::AlignVCenter, sub);

    // extra sources: small dots
    const QVector<int> src = outputSources(m_board, o);
    for (int k = 1; k < src.size() && k < 6; ++k)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(Ui::inputColor(src[k]));
        p.drawEllipse(QPointF(r.right() - 10 - (k - 1) * 11, r.bottom() - 11), 4, 4);
    }
    p.setBrush(Qt::NoBrush);

    const QPointF port = outputPort(o);
    p.setPen(QPen(col.darker(115), 1.5));
    p.setBrush(src.isEmpty() ? QColor(Qt::white) : col);
    p.drawEllipse(port, 6, 6);
    p.setBrush(Qt::NoBrush);
}

void ConnectionView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QFont head = font();
    head.setBold(true);
    head.setPointSizeF(font().pointSizeF() + 1);
    p.setFont(head);
    p.setPen(kText);
    p.drawText(QRectF(Margin, 8, nodeWidth(), 26), Qt::AlignLeft | Qt::AlignVCenter, tr("Inputs"));
    p.drawText(QRectF(width() - Margin - nodeWidth(), 8, nodeWidth(), 26), Qt::AlignLeft | Qt::AlignVCenter, tr("Outputs"));
    QFont small = font();
    small.setPointSizeF(qMax(7.0, font().pointSizeF() - 1));
    p.setFont(small);
    p.setPen(kMuted);
    const QRectF hintRect(Margin + nodeWidth() + 8, 8, width() - 2 * (Margin + nodeWidth() + 8), 26);
    p.drawText(hintRect, Qt::AlignCenter,
               p.fontMetrics().elidedText(tr("drag an output onto an input"), Qt::ElideRight, int(hintRect.width())));

    // wires below the nodes
    for (int w = 0; w < m_wires.size(); ++w)
    {
        const Wire &wire = m_wires[w];
        const QColor col = Ui::inputColor(wire.input);
        const bool emph = (w == m_selWire || w == m_hoverWire || wire.input == m_selInput || wire.output == m_selOutput);
        QPen pen(col, emph ? 3.5 : 2.2);
        if (wire.step >= 0) pen.setStyle(Qt::DashLine);
        if (w == m_selWire) pen.setColor(col.darker(130));
        p.setPen(pen);
        p.drawPath(wire.path);
    }
    // route step labels on top of wires
    for (const Wire &wire : m_wires)
    {
        if (wire.step < 0) continue;
        const QColor col = Ui::inputColor(wire.input);
        const QRectF lr = wire.labelRect;
        QPainterPath b; b.addRoundedRect(lr, 9, 9);
        p.fillPath(b, Qt::white);
        p.setPen(QPen(col, 1.2));
        p.drawPath(b);
        p.setPen(col.darker(150));
        p.drawText(lr, Qt::AlignCenter, wire.text);
    }

    // drag preview
    if (m_dragging)
    {
        const QPointF a = (m_drag == Drag::FromInput) ? inputPort(m_dragIndex) : m_dragPos;
        const QPointF b = (m_drag == Drag::FromInput) ? m_dragPos : outputPort(m_dragIndex);
        QPen pen(m_drag == Drag::FromInput ? Ui::inputColor(m_dragIndex) : kAccent, 2.2, Qt::DashLine);
        p.setPen(pen);
        p.drawPath(curve(a, b));
    }

    for (int i = 0; i < NumInputs; ++i)  if (inputShown(i))  drawInput(p, i);
    for (int o = 0; o < NumOutputs; ++o) if (outputShown(o)) drawOutput(p, o);
}

// ── Mouse ───────────────────────────────────────────────────────────────────
void ConnectionView::mousePressEvent(QMouseEvent *e)
{
    setFocus();
    if (e->button() != Qt::LeftButton)
        return;
    const QPointF pos = e->pos();
    m_pressPos = pos;
    m_dragging = false;
    m_drag = Drag::None;

    const int in = inputAt(pos), out = outputAt(pos);
    if (in >= 0)       { m_drag = Drag::FromInput;  m_dragIndex = in;  }
    else if (out >= 0) { m_drag = Drag::FromOutput; m_dragIndex = out; }
    else
    {
        m_selWire = wireAt(pos);
        update();
    }
}

void ConnectionView::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF pos = e->pos();
    if (m_drag != Drag::None && (e->buttons() & Qt::LeftButton))
    {
        if (!m_dragging && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance())
            m_dragging = true;
        if (m_dragging)
        {
            m_dragPos = pos;
            update();
        }
        return;
    }
    const int hover = wireAt(pos);
    if (hover != m_hoverWire)
    {
        m_hoverWire = hover;
        update();
    }
    setCursor(inputAt(pos) >= 0 || outputAt(pos) >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void ConnectionView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || m_drag == Drag::None)
        return;
    const QPointF pos = e->pos();
    const Drag drag = m_drag;
    const int idx = m_dragIndex;
    const bool dragged = m_dragging;
    m_drag = Drag::None;
    m_dragging = false;
    update();

    if (dragged)
    {
        if (drag == Drag::FromOutput)
        {
            const int in = inputAt(pos);
            if (in >= 0) emit connectRequested(in, idx);
        }
        else
        {
            const int out = outputAt(pos);
            if (out >= 0) emit connectRequested(idx, out);
        }
        return;
    }

    m_selWire = -1;
    if (drag == Drag::FromInput) { selectInput(idx);  emit inputSelected(idx);  }
    else                         { selectOutput(idx); emit outputSelected(idx); }
}

void ConnectionView::keyPressEvent(QKeyEvent *e)
{
    if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && m_selWire >= 0)
    {
        const Wire w = m_wires.value(m_selWire);
        m_selWire = -1;
        emit disconnectRequested(w.input, w.step);
        return;
    }
    QWidget::keyPressEvent(e);
}

void ConnectionView::contextMenuEvent(QContextMenuEvent *e)
{
    const int w = wireAt(e->pos());
    const int in = inputAt(e->pos());
    QMenu menu(this);

    if (w >= 0)
    {
        const Wire wire = m_wires[w];
        m_selWire = w;
        update();
        menu.addAction(tr("Remove connection In %1 → Out %2").arg(wire.input).arg(wire.output),
                       this, [this, wire] { emit disconnectRequested(wire.input, wire.step); });
        if (wire.step >= 0)
        {
            if (!wire.data.raw)
                menu.addAction(wire.data.value ? tr("Set curved (0)") : tr("Set straight (1)"),
                               this, [this, wire] { emit toggleStepRequested(wire.input, wire.step); });
            else
                menu.addAction(tr("Edit value in the input panel"), this,
                               [this, wire] { emit inputSelected(wire.input); });
            QAction *up = menu.addAction(tr("Move earlier in the route"), this,
                                         [this, wire] { emit moveStepRequested(wire.input, wire.step, -1); });
            up->setEnabled(wire.step > 0);
            QAction *down = menu.addAction(tr("Move later in the route"), this,
                                           [this, wire] { emit moveStepRequested(wire.input, wire.step, +1); });
            down->setEnabled(wire.step < routeSteps(m_board.inputs[wire.input]).size() - 1);
        }
    }
    else if (in >= 0 && inputConnected(m_board.inputs[in]))
    {
        menu.addAction(tr("Remove all connections of In %1").arg(in), this, [this, in] {
            const InputConfig &c = m_board.inputs[in];
            if (isDirectType(c.type))
                emit disconnectRequested(in, -1);
            else
                for (int s = routeSteps(c).size() - 1; s >= 0; --s)
                    emit disconnectRequested(in, s);
        });
    }
    if (!menu.isEmpty())
        menu.exec(e->globalPos());
}

bool ConnectionView::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip)
    {
        auto *he = static_cast<QHelpEvent *>(e);
        const int w = wireAt(he->pos());
        if (w >= 0)
        {
            const Wire &wire = m_wires[w];
            const QString mode = inputTypeName(m_board.inputs[wire.input].type);
            QString text = tr("In %1 → Out %2 (%3)").arg(wire.input).arg(wire.output).arg(mode);
            if (wire.step >= 0)
                text += tr("\nstep %1: %2, then wait %3")
                            .arg(wire.step + 1)
                            .arg(Ui::stepValueText(wire.data, m_board.outputs[wire.output].type))
                            .arg(Ui::delayText(wire.data.delay));
            QToolTip::showText(he->globalPos(), text, this);
        }
        else
            QToolTip::hideText();
        return true;
    }
    if (e->type() == QEvent::Resize)
        rebuildWires();
    return QWidget::event(e);
}
