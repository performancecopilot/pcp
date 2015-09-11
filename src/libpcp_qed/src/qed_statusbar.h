/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2008, Aconex.  All Rights Reserved.
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
#ifndef QED_STATUSBAR_H
#define QED_STATUSBAR_H

#include <QtGui/QLabel>
#include <QtGui/QStatusBar>
#include <QtGui/QGridLayout>
#include "qed_timebutton.h"

class QedStatusBar : public QStatusBar
{
    Q_OBJECT

public:
    QedStatusBar();
    virtual ~QedStatusBar() { }
    virtual void init();

    static int buttonSize() { return 56; }	// pixels

    QLabel *dateLabel() { return my.dateLabel; }
    QToolButton *timeFrame() { return my.timeFrame; }
    QedTimeButton *timeButton() { return my.timeButton; }

    QString dateText() { return my.dateLabel->text(); }
    void setDateText(QString &s) { my.dateLabel->setText(s); }
    void setValueText(QString &s) { my.valueLabel->setText(s); }
    void clearValueText() { my.valueLabel->clear(); }

    void setTimeAxisRightAlignment(int w);

protected:
    bool event(QEvent *);
    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);

    struct {
	QGridLayout *grid;
	QSpacerItem *labelSpacer;	// spacer between date/value labels
	QSpacerItem *rightSpacer;	// spacer at right edge for toolbar
	QToolButton *timeFrame;
	QedTimeButton *timeButton;
	QLabel *gadgetLabel;
	QLabel *valueLabel;
	QLabel *dateLabel;
    } my;
};

#endif	// QED_STATUSBAR_H
