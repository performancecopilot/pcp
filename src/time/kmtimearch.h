/*
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
#ifndef KMTIMEARCH_H
#define KMTIMEARCH_H

#include "ui_kmtimearch.h"
#include <kmtime.h>
#include "console.h"
#include "showboundsdialog.h"
#include "timezone.h"

class KmTimeArch : public QMainWindow, public Ui::KmTimeArch
{
    Q_OBJECT

public:
    KmTimeArch();

    virtual void play();
    virtual void back();
    virtual void stop();
    virtual int addDelta();
    virtual int subDelta();
    virtual int timerInterval();
    virtual void displayDeltaText();
    virtual void displayPositionText();
    virtual void displayPositionSlide();
    virtual void setPositionSlideRange();
    virtual void setPositionSlideDelta();
    virtual void popup(bool hello_popetts);
    virtual void addTimezone(char *string);
    virtual void setTime(KmTime::Packet *k, char *tzdata);
    virtual void addBound(KmTime::Packet *k, char *tzdata);
    virtual void setupAssistant();

public slots:
    virtual void setControl(KmTime::State newstate, KmTime::Mode newmode);
    virtual void init();
    virtual void helpAbout();
    virtual void helpSeeAlso();
    virtual void whatsThis();
    virtual void play_clicked();
    virtual void back_clicked();
    virtual void stop_clicked();
    virtual void timerTick();
    virtual void changeDelta(int value);
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
    virtual void showConsole();
    virtual void disableConsole();
    virtual void hideWindow();
    virtual void lineEditDelta_changed(const QString & s);
    virtual void lineEditCtime_changed(const QString & s);
    virtual void lineEditDelta_validate();
    virtual void lineEditCtime_validate();
    virtual void lineEditSpeed_validate();
    virtual void setTimezone(QAction *action);
    virtual void helpContents();
    virtual void helpManual();

signals:
    void timePulse(KmTime::Packet *);
    void boundsPulse(KmTime::Packet *);
    void vcrModePulse(KmTime::Packet *, int);
    void tzPulse(KmTime::Packet *, char *, int, char *, int);

protected:
    virtual void closeEvent(QCloseEvent * ce);

private:
    struct {
	KmTime::Packet kmtime;
	KmTime::DeltaUnits units;
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
	QAssistantClient *assistant;
    } my;
};

#endif // KMTIMEARCH_H
