/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2006-2009, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#ifndef QED_GROUPCONTROL_H
#define QED_GROUPCONTROL_H

#include <QtCore/QList>
#include <qmc_group.h>
#include <qmc_time.h>
#include "qed_timebutton.h"

class QedGroupControl : public QObject, public QmcGroup
{
    Q_OBJECT

public:
    QedGroupControl();
    void init(struct timeval *, struct timeval *);

    virtual bool isArchiveSource() = 0;

    double timeInterval() const { return my.realDelta; }
    double timePosition() const { return my.realPosition; }

    virtual void step(QmcTime::Packet *);
    virtual void VCRMode(QmcTime::Packet *, bool);
    virtual void setTimezone(QmcTime::Packet *, char *);

    virtual void adjustStep(QmcTime::Packet *) = 0;
    virtual void updateTimeButton() = 0;
    virtual void updateTimeAxis(void) = 0;

    virtual void setupWorldView(struct timeval *, struct timeval *,
				struct timeval *, struct timeval *);
    static bool fuzzyTimeMatch(double, double, double);

    QedTimeButton::State buttonState() { return my.buttonState; }
    QmcTime::State pmtimeState();
    void newButtonState(QmcTime::State, QmcTime::Mode, bool);
    bool isStateBackward() { return my.timeState == BackwardState; }

protected:
    typedef enum {
	StartState,
	ForwardState,
	BackwardState,
	EndLogState,
	StandbyState,
    } QedTimeState;

    char *timeState();
    void setTimeState(QedTimeState state) { my.timeState = state; }
    virtual void setButtonState(QedTimeButton::State) = 0;

    virtual bool isActive(QmcTime::Packet *) = 0;
    virtual bool isRecording(QmcTime::Packet *) = 0;
    virtual void adjustWorldView(QmcTime::Packet *, bool);
    virtual void adjustLiveWorldViewForward(QmcTime::Packet *);
    virtual void adjustLiveWorldViewStopped(QmcTime::Packet *);
    virtual void adjustArchiveWorldViewForward(QmcTime::Packet *, bool);
    virtual void adjustArchiveWorldViewStopped(QmcTime::Packet *, bool);
    virtual void adjustArchiveWorldViewBackward(QmcTime::Packet *, bool);

    struct {
	double realDelta;		// current update interval
	double realPosition;		// current time position
	struct timeval delta;
	struct timeval position;

	QedTimeButton::State buttonState;
	QmcTime::Source pmtimeSource;	// reliable archive/host test
	QmcTime::State pmtimeState;
	QedTimeState timeState;
    } my;
};

#endif	// QED_GROUPCONTROL_H
