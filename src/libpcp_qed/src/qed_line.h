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
#ifndef QED_LINE_H
#define QED_LINE_H

#include <QtGui>
#include "qed_gadget.h"

class QedLine : public QedGadget
{
    Q_OBJECT

public:
    QedLine(QWidget *parent, qreal length);
    QedLine(QWidget *parent, int length, Qt::Orientation o);
    QedLine(QWidget *parent, int x, int y, int w, int h);

    void setOrientation(Qt::Orientation o) { my.oriented = o; }

private:
    void setup(void);
    virtual void paintEvent(QPaintEvent *);
    virtual void resizeEvent(QResizeEvent *);

    struct {
	QRect			box;
	qreal			bound;
	qreal			length;
	enum Qt::Orientation	oriented;
    } my;
};

#endif // QED_LINE_H
