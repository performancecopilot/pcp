/*
 * Copyright (c) 2012, Red Hat.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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
#ifndef PMTIMEARCH_H
#define PMTIMEARCH_H

#include "ui_pmtimearch.h"
#include "pmtime.h"
#include "console.h"
#include "showboundsdialog.h"
#include "timezone.h"

class PmTimeArch : public PmTime, public Ui::PmTimeArch
{
    Q_OBJECT

public:
    PmTimeArch();

    virtual void play();
    virtual void back();
    virtual void stop();
    virtual int addDelta();
    virtual int subDelta();
    virtual int timerInterval();
    virtual void displayDeltaText();
    virtual void displayPositionText();
    virtual void displayPositionSlide();
    virtual void setPositionSlideScale();
    virtual void setPositionSlideRange();
    virtual void addTimezone(const char *string);
    virtual void setTime(PmTime::Packet *k, char *tzdata);
    virtual void addBound(PmTime::Packet *k, char *tzdata);

public slots:
    virtual void setControl(PmTime::State newstate, PmTime::Mode newmode);
    virtual void init();
    virtual void quit();
    virtual void play_clicked();
    virtual void back_clicked();
    virtual void stop_clicked();
    virtual void timerTick();
    virtual void changeDelta(int value);
    virtual void disableConsole();
    virtual void changeControl(int value);
    virtual void updateTime();
    virtual void pressedPosition();
    virtual void releasedPosition();
    virtual void changedPosition(double value);
    virtual void clickShowMsec();
    virtual void clickShowYear();
    virtual void resetSpeed();
    virtual void changeSpeed(double value);
    virtual void showBounds();
    virtual void doneBounds();
    virtual void lineEditDelta_changed(const QString & s);
    virtual void lineEditCtime_changed(const QString & s);
    virtual void lineEditDelta_validate();
    virtual void lineEditCtime_validate();
    virtual void lineEditSpeed_validate();
    virtual void setTimezone(QAction *action);

signals:
    void timePulse(PmTime::Packet *);
    void boundsPulse(PmTime::Packet *);
    void vcrModePulse(PmTime::Packet *, int);
    void tzPulse(PmTime::Packet *, char *, int, char *, int);

private:
    struct {
	PmTime::Packet pmtime;
	PmTime::DeltaUnits units;
	struct timeval absoluteStart;
	struct timeval absoluteEnd;
	double speed;

	bool first;
	bool showMilliseconds;
	bool showYear;

	QTimer *timer;
	Console *console;
	ShowBounds *bounds;
	QActionGroup *tzActions;
	QList<TimeZone*> tzlist;
    } my;
};

#endif // PMTIMEARCH_H
