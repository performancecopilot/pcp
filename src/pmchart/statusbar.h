/*
 * Copyright (c) 2008, Aconex.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <QtGui/QLabel>
#include <QtGui/QStatusBar>
#include <QtGui/QGridLayout>
#include "qed_timebutton.h"
#include "timeaxis.h"

class StatusBar : public QStatusBar
{
    Q_OBJECT

public:
    StatusBar();
    void init();
    void resetFont();

    static int buttonSize() { return 56; }	// pixels
    static int timeAxisHeight() { return 34; }	// pixels

    QLabel *dateLabel() { return my.dateLabel; }
    TimeAxis *timeAxis() { return my.timeAxis; }
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

private:
    struct {
	QGridLayout *grid;
	QSpacerItem *labelSpacer;	// spacer between date/value labels
	QSpacerItem *rightSpacer;	// spacer at right edge for toolbar
	QToolButton *timeFrame;
	QedTimeButton *timeButton;
	TimeAxis *timeAxis;
	QLabel *gadgetLabel;
	QLabel *valueLabel;
	QLabel *dateLabel;
    } my;
};

#endif
