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
#ifndef PMTIMELIVE_H
#define PMTIMELIVE_H

#include "ui_pmtimelive.h"
#include "pmtime.h"
#include "console.h"
#include "timezone.h"

class PmTimeLive : public PmTime, public Ui::PmTimeLive
{
    Q_OBJECT

public:
    PmTimeLive();

    virtual void play();
    virtual void stop();
    virtual int timerInterval();
    virtual void displayPosition();
    virtual void displayDeltaText();
    virtual void addTimezone(const char *string);
    virtual void setTime(PmTime::Packet *k, char *tzdata);

public slots:
    virtual void setControl(PmTime::State newstate);
    virtual void init();
    virtual void quit();
    virtual void play_clicked();
    virtual void stop_clicked();
    virtual void updateTime();
    virtual void clickShowMsec();
    virtual void clickShowYear();
    virtual void changeDelta(int value);
    virtual void disableConsole();
    virtual void lineEditDelta_changed(const QString &s);
    virtual void lineEditDelta_validate();
    virtual void setTimezone(QAction * action);

signals:
    void timePulse(PmTime::Packet *);
    void vcrModePulse(PmTime::Packet *, int);
    void tzPulse(PmTime::Packet *, char *, int, char *, int);

private:
    struct {
	PmTime::Packet pmtime;
	PmTime::DeltaUnits units;
	bool first;
	bool showMilliseconds;
	bool showYear;
	QTimer *timer;
	QActionGroup *tzActions;
	QList<TimeZone *> tzlist;
    } my;
};

#endif // PMTIMELIVE_H
