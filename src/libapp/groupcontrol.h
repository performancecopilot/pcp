/*
 * Copyright (c) 2006-2009, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#ifndef GROUPCONTROL_H
#define GROUPCONTROL_H

#include <QtCore/QList>
#include <qmc_group.h>
#include <qmc_time.h>
#include "timebutton.h"

class GroupControl : public QObject, public QmcGroup
{
    Q_OBJECT

public:
    GroupControl();
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

    TimeButton::State buttonState() { return my.buttonState; }
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
    } State;

    char *timeState();
    void setTimeState(State state) { my.timeState = state; }
    virtual void setButtonState(TimeButton::State) = 0;

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

	TimeButton::State buttonState;
	QmcTime::Source pmtimeSource;	// reliable archive/host test
	QmcTime::State pmtimeState;
	State timeState;
    } my;
};

#endif	// GROUPCONTROL_H
