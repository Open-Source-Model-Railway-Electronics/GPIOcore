#pragma once
// Node editor: inputs on the left, outputs on the right, wires in between.
// Drag an output onto an input (or an input onto an output) to connect them.

#include "gpioconfig.h"

#include <QPainterPath>
#include <QVector>
#include <QElapsedTimer>
#include <QWidget>

class ConnectionView : public QWidget
{
    Q_OBJECT
public:
    explicit ConnectionView(QWidget *parent = nullptr);

    void setBoard(const Gpio::BoardConfig &board);
    void setMarks(const QVector<bool> &inputDirty, const QVector<bool> &outputDirty,
                  const QVector<bool> &inputConflict, const QVector<bool> &outputConflict);
    void selectInput(int index);
    // Hide inputs / outputs that are not in use (and not part of a route).
    void setHideUnused(bool hide);
    // Live test: inputs get buttons, outputs show what the board reports.
    void setLiveMode(bool on);
    void setEditable(bool on) { m_editable = on; update(); }   // false: select and live buttons only
    enum class Frame { None, Configure, Simulate };   // red: a real board is changed, yellow: simulation
    void setFrame(Frame f) { m_frame = f; update(); }
    Frame frame() const { return m_frame; }
    bool liveMode() const { return m_live; }
    void setOutputLive(int output, int state, int value);   // state 0/1, 2 = raw value
    void clearLive();
    int  liveState(int output) const { return m_liveState.value(output, -1); }
    int  liveFrog(int output) const  { return m_liveFrog.value(output, -1); }   // bit0 A, bit1 B, bit2 C
    QRectF liveButtonRect(int input, int direction) const;   // empty if the input has no such button
    bool   liveValueInput(int input) const;   // direct input on a signal / PWM output: sends a value
    bool hideUnused() const { return m_hideUnused; }
    bool inputShown(int index) const  { return m_inRow.value(index, -1) >= 0; }
    bool outputShown(int index) const { return m_outRow.value(index, -1) >= 0; }
    void selectOutput(int index);

    const Gpio::BoardConfig &board() const { return m_board; }
    QColor inputColor(int index) const;
    QColor outputColor(int index) const;
    QRectF inputRect(int index) const;
    QRectF outputRect(int index) const;
    QPointF inputPort(int index) const;
    QPointF outputPort(int index) const;
    bool    inputConflictShown(int index) const  { return m_inConflict.value(index); }
    bool    outputConflictShown(int index) const { return m_outConflict.value(index); }
    bool    outputDirtyShown(int index) const    { return m_outDirty.value(index); }
    QPointF routeLabelPos(int input, int step) const;   // centre of a route step label (-1,-1 if none)

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    void inputSelected(int index);
    void outputSelected(int index);
    void connectRequested(int input, int output);
    void disconnectRequested(int input, int step);      // step -1 = direct wire
    void toggleStepRequested(int input, int step);
    void moveStepRequested(int input, int step, int delta);
    void liveTriggerRequested(int input, int direction);
    void removeRequested(bool output, int index);   // trash on a hovered block
    void liveValueRequested(int input);             // signal aspect / PWM value (DCC extended)

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    bool event(QEvent *e) override;

private:
    struct Wire
    {
        int          input = 0;
        int          output = 0;
        int          step = -1;      // -1 = direct
        Gpio::RouteStep data;
        QString      text;           // route label, e.g. "3 45°"
        QPainterPath path;
        QPointF      label;
        QRectF       labelRect;
    };

    void   rebuildWires();
    int    inputAt(const QPointF &p) const;
    int    outputAt(const QPointF &p) const;
    int    wireAt(const QPointF &p) const;
    qreal  nodeWidth() const;
    static QPainterPath curve(const QPointF &a, const QPointF &b);
    void   drawInput(QPainter &p, int i);
    void   drawOutput(QPainter &p, int i);
    QFont  smallFont() const;

    Gpio::BoardConfig m_board;
    QVector<bool>     m_inDirty, m_outDirty, m_inConflict, m_outConflict;
    QVector<Wire>     m_wires;

    int     m_selInput  = -1;
    bool    m_hideUnused = false;
    int     m_hoverIn = -1, m_hoverOut = -1;
    QRectF  trashRect(bool output, int index) const;
    bool    m_live = false;
    bool    m_editable = true;
    Frame   m_frame = Frame::None;
    QVector<int>    m_liveState, m_liveValue;
    QVector<bool>   m_liveRaw;
    QVector<int>    m_liveFrog;           // frog relays reported by the board, -1 = unknown            // last report was a raw value (angle / aspect / PWM)
    QVector<qint64> m_liveFlashOut, m_liveFlashIn;
    QElapsedTimer   m_liveClock;
    QTimer         *m_liveAnim = nullptr;
    void    drawInputLive(QPainter &p, int i);
    void    drawOutputLive(QPainter &p, int o);
    qreal   flashLevel(qint64 t) const;
    QVector<int> m_inRow, m_outRow;   // row on the canvas, -1 = hidden
    void    updateRows();
    int     m_selOutput = -1;
    int     m_selWire   = -1;
    int     m_hoverWire = -1;

    // drag state
    enum class Drag { None, FromInput, FromOutput };
    Drag    m_drag = Drag::None;
    int     m_dragIndex = -1;
    bool    m_dragging = false;
    QPointF m_pressPos;
    QPointF m_dragPos;
};
