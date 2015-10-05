/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#ifndef QED_TIMEBUTTON_H
#define QED_TIMEBUTTON_H

#include <QtGui/QIcon>
#include <QtGui/QToolButton>

class QedTimeButton : public QToolButton
{
    Q_OBJECT

public:
    typedef enum {
	Timeless = 1,
	ForwardLive = 2,
	StoppedLive = 3,
	ForwardRecord = 4,
	StoppedRecord = 5,
	ForwardArchive = 6,
	StoppedArchive = 7,
	BackwardArchive = 8,
	StepForwardArchive = 9,
	StepBackwardArchive = 10,
	FastForwardArchive = 11,
	FastBackwardArchive = 12,
    } State;

    QedTimeButton(QWidget *);
    void setButtonState(State state);

private:
    struct {
	State state;
	QIcon forwardLiveIcon;
	QIcon stoppedLiveIcon;
	QIcon forwardRecordIcon;
	QIcon stoppedRecordIcon;
	QIcon forwardArchiveIcon;
	QIcon stoppedArchiveIcon;
	QIcon backwardArchiveIcon;
	QIcon stepForwardArchiveIcon;
	QIcon stepBackwardArchiveIcon;
	QIcon fastForwardArchiveIcon;
	QIcon fastBackwardArchiveIcon;
    } my;
};

#endif	// QED_TIMEBUTTON_H
