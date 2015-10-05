/*
 * Copyright (c) 2013-2014, Red Hat.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#ifndef QED_LED_H
#define QED_LED_H

#include <QtGui>
#include "qed_gadget.h"
#include "qed_legend.h"

class QedLED : public QedGadget
{
    Q_OBJECT

public:
    QedLED(QWidget *parent, QColor color);
    QedLED(QWidget *parent, int x, int y, int w, int h, QedLegend *color);

protected:
    virtual void paintEvent(QPaintEvent *) = 0;
    virtual void resizeEvent(QResizeEvent *);

    struct {
	QColor	color;
	qreal	bound;
    } my;
};

class QedRoundLED : public QedLED
{
    Q_OBJECT

public:
    QedRoundLED(QWidget *parent, QColor color) : QedLED(parent, color) { }
    QedRoundLED(QWidget *parent, int x, int y, int w, int h, QedLegend *l)
	: QedLED(parent, x, y, w, h, l) { }

private:
    void paintEvent(QPaintEvent *);
};

class QedSquareLED : public QedLED
{
    Q_OBJECT

public:
    QedSquareLED(QWidget *parent, QColor color) : QedLED(parent, color) { }
    QedSquareLED(QWidget *parent, int x, int y, int w, int h, QedLegend *l)
	: QedLED(parent, x, y, w, h, l) { }

private:
    void paintEvent(QPaintEvent *);
};

#endif // QED_LED_H
