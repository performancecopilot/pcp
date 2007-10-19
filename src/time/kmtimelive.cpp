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
#include "kmtimelive.h"

#include <QtCore/QDir>
#include <QtCore/QTimer>
#include <QtCore/QLibraryInfo>
#include <QtGui/QValidator>
#include <QtGui/QWhatsThis>
#include <QtGui/QMessageBox>
#include <QtGui/QCloseEvent>
#include <QtGui/QActionGroup>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <sys/time.h>
#include <kmtime.h>
#include "version.h"
#include "aboutdialog.h"
#include "seealsodialog.h"

KmTimeLive::KmTimeLive() : QMainWindow(NULL)
{
    setupUi(this);
}

typedef struct {
    KmTime::State	state;
    QIcon 		*stop;
    QIcon 		*play;
} IconStateMap;

static void setup(IconStateMap *map, KmTime::State state,
		  QIcon *stop, QIcon *play)
{
    map->state = state;
    map->stop = stop;
    map->play = play;
}

void KmTimeLive::setControl(KmTime::State state)
{
    static IconStateMap maps[2];
    static int nmaps;

    if (!nmaps) {
	nmaps = sizeof(maps) / sizeof(maps[0]);
	setup(&maps[0], KmTime::StoppedState,
			KmTime::icon(KmTime::StoppedOn),
			KmTime::icon(KmTime::ForwardOff));
	setup(&maps[1], KmTime::ForwardState,
			KmTime::icon(KmTime::StoppedOff),
			KmTime::icon(KmTime::ForwardOn));
    }

    if (my.kmtime.state != state) {
	for (int i = 0; i < nmaps; i++) {
	    if (maps[i].state == state) {
		buttonStop->setIcon(*maps[i].stop);
		buttonPlay->setIcon(*maps[i].play);
		break;
	    }
	}
	my.kmtime.state = state;
    }
}

void KmTimeLive::init()
{
    static char *UTC = "UTC\0Universal Coordinated Time";

    console->post("Starting Live Time Control...");

    my.units = KmTime::Seconds;
    my.first = true;
    my.tzActions = NULL;
    my.assistant = NULL;

    memset(&my.kmtime, 0, sizeof(my.kmtime));
    my.kmtime.source = KmTime::HostSource;
    my.kmtime.delta.tv_sec = KmTime::DefaultDelta;

    my.showMilliseconds = false;
    optionsDetailShow_MillisecondsAction->setChecked(my.showMilliseconds);
    my.showYear = false;
    optionsDetailShow_YearAction->setChecked(my.showYear);

    addTimezone(UTC);
    displayPosition();
    displayDeltaText();
    setControl(KmTime::ForwardState);

    my.timer = new QTimer(this);
    connect(my.timer, SIGNAL(timeout()), SLOT(updateTime()));
    my.timer->start(timerInterval());
    lineEditDelta->setAlignment(Qt::AlignRight);
    lineEditDelta->setValidator(new QDoubleValidator
		(0.001, INT_MAX, 3, lineEditDelta));
}

void KmTimeLive::quit()
{
    console->post("live quit!\n");
    if (my.assistant)
	my.assistant->closeAssistant();
}

void KmTimeLive::helpAbout()
{
    AboutDialog about(this);
    about.exec();
}

void KmTimeLive::helpSeeAlso()
{
    SeeAlsoDialog about(this);
    about.exec();
}

void KmTimeLive::whatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}

int KmTimeLive::timerInterval()
{
    return (int)((my.kmtime.delta.tv_sec * 1000) +
		 (my.kmtime.delta.tv_usec / 1000));
}

void KmTimeLive::play_clicked()
{
    if (lineEditDelta->isModified())
	lineEditDelta_validate();
    if (my.kmtime.state != KmTime::ForwardState)
	play();
}

void KmTimeLive::play()
{
    console->post("KmTimeLive::play");
    setControl(KmTime::ForwardState);
    updateTime();
    if (!my.timer->isActive())
	my.timer->start(timerInterval());
}

void KmTimeLive::stop_clicked()
{
    if (my.kmtime.state != KmTime::StoppedState)
	stop();
}

void KmTimeLive::stop()
{
    console->post("KmTimeLive::stop stopped time");
    setControl(KmTime::StoppedState);
    my.timer->stop();
    emit vcrModePulse(&my.kmtime, 0);
}

void KmTimeLive::updateTime()
{
    gettimeofday(&my.kmtime.position, NULL);
    displayPosition();
    emit timePulse(&my.kmtime);
}

void KmTimeLive::displayPosition()
{
    QString text;
    char ctimebuf[32], msecbuf[5];

    pmCtime(&my.kmtime.position.tv_sec, ctimebuf);
    text = tr(ctimebuf);
    if (my.showYear == false)
	text.remove(19, 5);
    if (my.showMilliseconds == true) {
	sprintf(msecbuf, ".%03u", (uint)my.kmtime.position.tv_usec / 1000);
	text.insert(19, msecbuf);
    }
    lineEditCtime->setText(text.simplified());
}

void KmTimeLive::clickShowMsec()
{
    if (my.showMilliseconds == true)
	my.showMilliseconds = false;
    else
	my.showMilliseconds = true;
    optionsDetailShow_MillisecondsAction->setChecked(my.showMilliseconds);
    displayPosition();
}

void KmTimeLive::clickShowYear()
{
    if (my.showYear == true)
	my.showYear = false;
    else
	my.showYear = true;
    optionsDetailShow_YearAction->setChecked(my.showYear);
    displayPosition();
}

void KmTimeLive::changeDelta(int value)
{
    my.units = (KmTime::DeltaUnits)value;
    displayDeltaText();
}

void KmTimeLive::displayDeltaText()
{
    QString text;
    double delta = KmTime::secondsFromTimeval(&my.kmtime.delta);

    delta = KmTime::secondsToUnits(delta, my.units);
    if ((double)(int)delta == delta)
	text.sprintf("%.2f", delta);
    else
	text.sprintf("%.6f", delta);
    lineEditDelta->setText(text);
}

void KmTimeLive::showConsole()
{
    console->show();
}

void KmTimeLive::disableConsole()
{
    optionsShowConsoleAction->setVisible(false);
}

void KmTimeLive::hideWindow()
{
    if (isVisible())
	hide();
    else
	show();
}

void KmTimeLive::popup(bool hello_popetts)
{
    if (hello_popetts)
	show();
    else
	hide();
}

void KmTimeLive::closeEvent(QCloseEvent *ce)
{
    hide();
    ce->ignore();
}

void KmTimeLive::lineEditDelta_changed(const QString &)
{
    if (lineEditDelta->isModified())
	stop_clicked();
}

void KmTimeLive::lineEditDelta_validate()
{
    double delta;
    bool ok, reset = my.timer->isActive();

    delta = lineEditDelta->text().toDouble(&ok);
    if (!ok || delta <= 0) {
	displayDeltaText();	// reset to previous, known-good delta
    } else {
	my.timer->stop();
	delta = KmTime::unitsToSeconds(delta, my.units);
	KmTime::secondsToTimeval(delta, &my.kmtime.delta);
	emit vcrModePulse(&my.kmtime, 0);
	if (reset)
	    my.timer->start(timerInterval());
    }
}

void KmTimeLive::setTimezone(QAction *action)
{
    for (int i = 0; i < my.tzlist.size(); i++) {
	TimeZone *tz = my.tzlist.at(i);
	if (tz->action() == action) {
	    pmUseZone(tz->handle());
	    emit tzPulse(&my.kmtime, tz->tz(), strlen(tz->tz()) + 1,
				tz->tzlabel(), strlen(tz->tzlabel()) + 1);
	    setTime(&my.kmtime, NULL);	// re-display the time, no messages
	    console->post("KmTimeLive::setTimezone sent TZ %s(%s) to clients\n",
				tz->tz(), tz->tzlabel());
	    break;
	}
    }
}

void KmTimeLive::addTimezone(char *string)
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
    console->post("KmTimeLive::addTimezone added TZ=%s label=%s", tz, label);
}

void KmTimeLive::setTime(KmTime::Packet *k, char *tzdata)
{
    if (tzdata != NULL)
	addTimezone(tzdata);

    if (my.first == true) {
	bool reset = my.timer->isActive();

	my.first = false;
	my.kmtime.position = k->position;
	my.kmtime.delta = k->delta;
	my.timer->stop();
	if (reset)
	    my.timer->start(timerInterval());
	displayDeltaText();
	displayPosition();
    }
}

void KmTimeLive::style(char *style, void *source)
{
    emit stylePulse(&my.kmtime, style, strlen(style) + 1, source);
}

void KmTimeLive::assistantError(const QString &msg)
{
    QMessageBox::warning(this, pmProgname, msg);
}

void KmTimeLive::setupAssistant()
{
    if (my.assistant)
	return;
    my.assistant = new QAssistantClient(
		QLibraryInfo::location(QLibraryInfo::BinariesPath), this);
    connect(my.assistant, SIGNAL(error(const QString &)),
		    this, SLOT(assistantError(const QString &)));
    QStringList arguments;
    QString documents = HTMLDIR;
    arguments << "-profile" << documents.append("/kmtime.adp");
    my.assistant->setArguments(arguments);
}

void KmTimeLive::helpManual()
{
    setupAssistant();
    QString documents = HTMLDIR;
    my.assistant->showPage(documents.append("/manual.html"));
}
