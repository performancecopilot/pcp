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
#include "pmtimelive.h"

#include <QtCore/QTimer>
#include <QtGui/QValidator>
#include <QtGui/QActionGroup>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "pmtime.h"
#include "aboutdialog.h"
#include "seealsodialog.h"

PmTimeLive::PmTimeLive() : PmTime()
{
    setupUi(this);
#ifdef Q_OS_MAC        // fixup after relocation of the MenuBar by Qt
    QSize size = QSize(width(), height() - MenuBar->height());
    setMinimumSize(size);
    setMaximumSize(size);
#endif
}

typedef struct {
    PmTime::State	state;
    QIcon 		*stop;
    QIcon 		*play;
} IconStateMap;

static void setup(IconStateMap *map, PmTime::State state,
		  QIcon *stop, QIcon *play)
{
    map->state = state;
    map->stop = stop;
    map->play = play;
}

void PmTimeLive::setControl(PmTime::State state)
{
    static IconStateMap maps[2];
    static int nmaps;

    if (!nmaps) {
	nmaps = sizeof(maps) / sizeof(maps[0]);
	setup(&maps[0], PmTime::StoppedState,
			PmTime::icon(PmTime::StoppedOn),
			PmTime::icon(PmTime::ForwardOff));
	setup(&maps[1], PmTime::ForwardState,
			PmTime::icon(PmTime::StoppedOff),
			PmTime::icon(PmTime::ForwardOn));
    }

    if (my.pmtime.state != state) {
	for (int i = 0; i < nmaps; i++) {
	    if (maps[i].state == state) {
		buttonStop->setIcon(*maps[i].stop);
		buttonPlay->setIcon(*maps[i].play);
		break;
	    }
	}
	my.pmtime.state = state;
    }
}

void PmTimeLive::init()
{
    static const char *UTC = "UTC\0Universal Coordinated Time";

    console->post("Starting Live Time Control...");

    my.units = PmTime::Seconds;
    my.first = true;
    my.tzActions = NULL;

    memset(&my.pmtime, 0, sizeof(my.pmtime));
    my.pmtime.source = PmTime::HostSource;
    my.pmtime.delta.tv_sec = PmTime::DefaultDelta;

    my.showMilliseconds = false;
    optionsDetailShow_MillisecondsAction->setChecked(my.showMilliseconds);
    my.showYear = false;
    optionsDetailShow_YearAction->setChecked(my.showYear);

    addTimezone(UTC);
    displayPosition();
    displayDeltaText();
    setControl(PmTime::ForwardState);

    my.timer = new QTimer(this);
    connect(my.timer, SIGNAL(timeout()), SLOT(updateTime()));
    my.timer->start(timerInterval());
    lineEditDelta->setAlignment(Qt::AlignRight);
    lineEditDelta->setValidator(new QDoubleValidator
		(0.001, INT_MAX, 3, lineEditDelta));
}

void PmTimeLive::quit()
{
    console->post("live quit!\n");
}

int PmTimeLive::timerInterval()
{
    return (int)((my.pmtime.delta.tv_sec * 1000) +
		 (my.pmtime.delta.tv_usec / 1000));
}

void PmTimeLive::play_clicked()
{
    if (lineEditDelta->isModified())
	lineEditDelta_validate();
    if (my.pmtime.state != PmTime::ForwardState)
	play();
}

void PmTimeLive::play()
{
    console->post("PmTimeLive::play");
    setControl(PmTime::ForwardState);
    __pmtimevalNow(&my.pmtime.position);
    displayPosition();
    emit vcrModePulse(&my.pmtime, 0);
    if (!my.timer->isActive())
	my.timer->start(timerInterval());
}

void PmTimeLive::stop_clicked()
{
    if (my.pmtime.state != PmTime::StoppedState)
	stop();
}

void PmTimeLive::stop()
{
    console->post("PmTimeLive::stop stopped time");
    setControl(PmTime::StoppedState);
    my.timer->stop();
    __pmtimevalNow(&my.pmtime.position);
    emit vcrModePulse(&my.pmtime, 0);
}

void PmTimeLive::updateTime()
{
    __pmtimevalNow(&my.pmtime.position);
    displayPosition();
    emit timePulse(&my.pmtime);
}

void PmTimeLive::displayPosition()
{
    QString text;
    char ctimebuf[32], msecbuf[5];

    pmCtime((const time_t *)&my.pmtime.position.tv_sec, ctimebuf);
    text = tr(ctimebuf);
    if (my.showYear == false)
	text.remove(19, 5);
    if (my.showMilliseconds == true) {
	sprintf(msecbuf, ".%03u", (uint)my.pmtime.position.tv_usec / 1000);
	text.insert(19, msecbuf);
    }
    lineEditCtime->setText(text.simplified());
}

void PmTimeLive::clickShowMsec()
{
    if (my.showMilliseconds == true)
	my.showMilliseconds = false;
    else
	my.showMilliseconds = true;
    optionsDetailShow_MillisecondsAction->setChecked(my.showMilliseconds);
    displayPosition();
}

void PmTimeLive::clickShowYear()
{
    if (my.showYear == true)
	my.showYear = false;
    else
	my.showYear = true;
    optionsDetailShow_YearAction->setChecked(my.showYear);
    displayPosition();
}

void PmTimeLive::changeDelta(int value)
{
    my.units = (PmTime::DeltaUnits)value;
    displayDeltaText();
}

void PmTimeLive::displayDeltaText()
{
    QString text;
    double delta = PmTime::secondsFromTimeval(&my.pmtime.delta);

    delta = PmTime::secondsToUnits(delta, my.units);
    if ((double)(int)delta == delta)
	text.sprintf("%.2f", delta);
    else
	text.sprintf("%.6f", delta);
    lineEditDelta->setText(text);
}

void PmTimeLive::disableConsole()
{
    optionsShowConsoleAction->setVisible(false);
}

void PmTimeLive::lineEditDelta_changed(const QString &)
{
    if (lineEditDelta->isModified())
	stop_clicked();
}

void PmTimeLive::lineEditDelta_validate()
{
    double delta;
    bool ok, reset = my.timer->isActive();

    delta = lineEditDelta->text().toDouble(&ok);
    if (!ok || delta <= 0) {
	displayDeltaText();	// reset to previous, known-good delta
    } else {
	my.timer->stop();
	delta = PmTime::unitsToSeconds(delta, my.units);
	PmTime::secondsToTimeval(delta, &my.pmtime.delta);
	emit vcrModePulse(&my.pmtime, 0);
	if (reset)
	    my.timer->start(timerInterval());
    }
}

void PmTimeLive::setTimezone(QAction *action)
{
    for (int i = 0; i < my.tzlist.size(); i++) {
	TimeZone *tz = my.tzlist.at(i);
	if (tz->action() == action) {
	    pmUseZone(tz->handle());
	    emit tzPulse(&my.pmtime, tz->tz(), strlen(tz->tz()) + 1,
				tz->tzlabel(), strlen(tz->tzlabel()) + 1);
	    setTime(&my.pmtime, NULL);	// re-display the time, no messages
	    console->post("PmTimeLive::setTimezone sent TZ %s(%s) to clients\n",
				tz->tz(), tz->tzlabel());
	    break;
	}
    }
}

void PmTimeLive::addTimezone(const char *string)
{
    TimeZone *tmp, *tzp;
    QAction *tzAction;
    char *label, *tz;
    int handle;

    if ((handle = pmNewZone(string)) < 0)
	return;

    if ((tz = strdup(string)) == NULL)
	return;

    if ((label = strdup(string + strlen(string) + 1)) == NULL) {
	free(tz);
	return;
    }

    for (int i = 0; i < my.tzlist.size(); i++) {
	tmp = my.tzlist.at(i);
	if (strcmp(tmp->tzlabel(), label) == 0) {
	    free(label);
	    free(tz);
	    return;
	}
    }

    tzAction = new QAction(this);
    tzAction->setCheckable(true);
    tzAction->setToolTip(tz);
    tzAction->setText(label);

    tzp = new TimeZone(tz, label, tzAction, handle);
    my.tzlist.append(tzp);

    if (!my.tzActions) {
	my.tzActions = new QActionGroup(this);
	connect(my.tzActions, SIGNAL(selected(QAction *)) , this,
				SLOT(setTimezone(QAction *)));
    }
    my.tzActions->addAction(tzAction);
    optionsTimezoneAction->addActions(my.tzActions->actions());
    console->post("PmTimeLive::addTimezone added TZ=%s label=%s", tz, label);
}

void PmTimeLive::setTime(PmTime::Packet *k, char *tzdata)
{
    if (tzdata != NULL)
	addTimezone(tzdata);

    if (my.first == true) {
	bool reset = my.timer->isActive();

	my.first = false;
	my.pmtime.position = k->position;
	my.pmtime.delta = k->delta;
	my.timer->stop();
	if (reset)
	    my.timer->start(timerInterval());
	displayDeltaText();
	displayPosition();
    }
}
