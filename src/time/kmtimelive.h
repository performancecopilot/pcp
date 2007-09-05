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
#ifndef KMTIMELIVE_H
#define KMTIMELIVE_H

#include "ui_kmtimelive.h"
#include <kmtime.h>
#include "console.h"
#include "timezone.h"

class KmTimeLive : public QMainWindow, public Ui::KmTimeLive
{
    Q_OBJECT

public:
    KmTimeLive();

    virtual void play();
    virtual void stop();
    virtual int timerInterval();
    virtual void displayPosition();
    virtual void displayDeltaText();
    virtual void popup(bool hello_popetts);
    virtual void addTimezone(char *string);
    virtual void setTime(KmTime::Packet *k, char *tzdata);
    virtual void style(char *style, void *source);
    virtual void setupAssistant();

public slots:
    virtual void setControl(KmTime::State newstate);
    virtual void init();
    virtual void helpAbout();
    virtual void helpSeeAlso();
    virtual void whatsThis();
    virtual void play_clicked();
    virtual void stop_clicked();
    virtual void updateTime();
    virtual void clickShowMsec();
    virtual void clickShowYear();
    virtual void changeDelta(int value);
    virtual void showConsole();
    virtual void disableConsole();
    virtual void hideWindow();
    virtual void lineEditDelta_changed(const QString &s);
    virtual void lineEditDelta_validate();
    virtual void setTimezone(QAction * action);
    virtual void helpContents();
    virtual void helpManual();

signals:
    void timePulse(KmTime::Packet *);
    void vcrModePulse(KmTime::Packet *, int);
    void tzPulse(KmTime::Packet *, char *, int, char *, int);
    void stylePulse(KmTime::Packet *, char *, int, void *);

protected:
    virtual void closeEvent(QCloseEvent *ce);

private:
    struct {
	KmTime::Packet kmtime;
	KmTime::DeltaUnits units;
	bool first;
	bool showMilliseconds;
	bool showYear;
	QTimer *timer;
	QActionGroup *tzActions;
	QList<TimeZone *> tzlist;
	QAssistantClient *assistant;
    } my;
};

#endif // KMTIMELIVE_H
