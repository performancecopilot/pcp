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
#include <pmtime.h>
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

    virtual void step(PmTime::Packet *);
    virtual void VCRMode(PmTime::Packet *, bool);
    virtual void setTimezone(PmTime::Packet *, char *);

    virtual void adjustStep(PmTime::Packet *) = 0;
    virtual void updateTimeButton() = 0;
    virtual void updateTimeAxis(void) = 0;

    virtual void setupWorldView(struct timeval *, struct timeval *,
				struct timeval *, struct timeval *);
    static bool fuzzyTimeMatch(double, double, double);

    TimeButton::State buttonState() { return my.buttonState; }
    PmTime::State pmtimeState();
    void newButtonState(PmTime::State, PmTime::Mode, bool);
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

    virtual bool isActive(PmTime::Packet *) = 0;
    virtual bool isRecording(PmTime::Packet *) = 0;
    virtual void adjustWorldView(PmTime::Packet *, bool);
    virtual void adjustLiveWorldViewForward(PmTime::Packet *);
    virtual void adjustLiveWorldViewStopped(PmTime::Packet *);
    virtual void adjustArchiveWorldViewForward(PmTime::Packet *, bool);
    virtual void adjustArchiveWorldViewStopped(PmTime::Packet *, bool);
    virtual void adjustArchiveWorldViewBackward(PmTime::Packet *, bool);

    struct {
	double realDelta;		// current update interval
	double realPosition;		// current time position
	struct timeval delta;
	struct timeval position;

	TimeButton::State buttonState;
	PmTime::Source pmtimeSource;	// reliable archive/host test
	PmTime::State pmtimeState;
	State timeState;
    } my;
};

#endif	// GROUPCONTROL_H
