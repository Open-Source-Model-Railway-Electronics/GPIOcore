#pragma once
#include "gpioconfig.h"

#include <QWidget>

// Overview of all Arduino pins and which output / input uses them.
class PinMapView : public QWidget
{
    Q_OBJECT
public:
    explicit PinMapView(QWidget *parent = nullptr);

    void setBoard(const Gpio::BoardConfig &board);
    QSize sizeHint() const override { return QSize(760, 520); }

signals:
    void outputActivated(int index);
    void inputActivated(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    QRectF cardRect(int pin) const;

    Gpio::BoardConfig m_board;
};
