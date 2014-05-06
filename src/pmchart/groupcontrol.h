/*
 * Copyright (c) 2006-2008, Aconex.  All Rights Reserved.
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
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QPixmap>
#include <qwt_plot.h>
#include <qwt_scale_draw.h>
#include <qmc_group.h>
#include <qmc_time.h>
#include "gadget.h"
#include "timebutton.h"

class GroupControl : public QObject, public QmcGroup
{
    Q_OBJECT

public:
    GroupControl();
    void init(int, int, struct timeval *, struct timeval *);

    void addGadget(Gadget *);
    void deleteGadget(Gadget *);
    int gadgetCount() const;

    bool isArchiveSource();
    void updateBackground();

    void setVisibleHistory(int);
    int visibleHistory();
    void setSampleHistory(int);
    int sampleHistory();

    double *timeAxisData(void);

    void step(QmcTime::Packet *);
    void VCRMode(QmcTime::Packet *, bool);
    void setTimezone(QmcTime::Packet *, char *);

    void setupWorldView();
    void updateTimeButton();
    void updateTimeAxis(void);
    void updateTimeAxis(time_t secs);

    TimeButton::State buttonState();
    QmcTime::State pmtimeState();
    void newButtonState(QmcTime::State, QmcTime::Mode, bool);

public Q_SLOTS:
    void timeSelectionActive(Gadget *, int);
    void timeSelectionReactive(Gadget *, int);
    void timeSelectionInactive(Gadget *);

private:
    typedef enum {
	StartState,
	ForwardState,
	BackwardState,
	EndLogState,
	StandbyState,
    } State;

    char *timeState();
    void refreshGadgets(bool);
    bool isActive(QmcTime::Packet *);
    void adjustWorldView(QmcTime::Packet *, bool);
    void adjustLiveWorldViewForward(QmcTime::Packet *);
    void adjustLiveWorldViewStopped(QmcTime::Packet *);
    void adjustArchiveWorldViewForward(QmcTime::Packet *, bool);
    void adjustArchiveWorldViewStopped(QmcTime::Packet *, bool);
    void adjustArchiveWorldViewBackward(QmcTime::Packet *, bool);

    struct {
	QList<Gadget*> gadgetsList;	// gadgets with metrics in this group

	double realDelta;		// current update interval
	double realPosition;		// current time position
	struct timeval delta;
	struct timeval position;

	int visible;			// -v visible points
	int samples;			// -s total number of samples
	double *timeData;		// time array (intervals)

	TimeButton::State buttonState;
	QmcTime::Source pmtimeSource;	// reliable archive/host test
	QmcTime::State pmtimeState;
	State timeState;
    } my;
};

#endif	// GROUPCONTROL_H
