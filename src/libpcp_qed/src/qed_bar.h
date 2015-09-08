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
#ifndef QED_BAR_H
#define QED_BAR_H

#include <QtGui>
#include "qed_gadget.h"
#include "qed_colorlist.h"

class QedBar : public QedGadget
{
    Q_OBJECT

public:
    QedBar(QWidget *parent, int x, int y, int w, int h);
    void setMinimum(double min);
    void setMaximum(double max);
    void setColor(const char *color);
    void setOrientation(Qt::Orientation);
    void setScaleRange(int range);

protected:
    virtual void paintEvent(QPaintEvent *);
    virtual void resizeEvent(QResizeEvent *);

    struct {
	QColor		color;
	Qt::Orientation	oriented;
	int		scaleRange;
	double		minimum;
	double		maximum;
    } my;
};

class QedMultiBar : public QedBar
{
    Q_OBJECT

public:
    QedMultiBar(QWidget *parent, int x, int y, int w, int h,
		QedColorList *l, int history);
    void setOutline(bool);
    void setMaximum(double, bool);
};

class QedBarGraph : public QedBar
{
    Q_OBJECT

public:
    QedBarGraph(QWidget *parent, int x, int y, int w, int h, int history);
    void clipRange();
};

#endif // QED_BAR_H
