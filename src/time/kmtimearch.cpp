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
#include "kmtimearch.h"

#include <QtCore/QDir>
#include <QtCore/QTimer>
#include <QtCore/QLibraryInfo>
#include <QtGui/QValidator>
#include <QtGui/QWhatsThis>
#include <QtGui/QMessageBox>
#include <QtGui/QCloseEvent>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "version.h"
#include "aboutdialog.h"
#include "seealsodialog.h"

KmTimeArch::KmTimeArch() : QMainWindow(NULL)
{
    setupUi(this);
}

typedef struct {
    KmTime::State	state;
    KmTime::Mode	mode;
    QIcon 		*back;
    QIcon 		*stop;
    QIcon 		*play;
} IconStateMap;

static void setup(IconStateMap *map, KmTime::State state, KmTime::Mode mode,
		  QIcon *back, QIcon *stop, QIcon *play)
{
    map->state = state;
    map->mode = mode;
    map->back = back;
    map->stop = stop;
    map->play = play;
}

void KmTimeArch::setControl(KmTime::State state, KmTime::Mode mode)
{
    static IconStateMap maps[3 * 3];
    static int nmaps;

    if (!nmaps) {
	nmaps = sizeof(maps) / sizeof(maps[0]);
	setup(&maps[0], KmTime::StoppedState, KmTime::NormalMode,
			KmTime::icon(KmTime::BackwardOff),
			KmTime::icon(KmTime::StoppedOn),
			KmTime::icon(KmTime::ForwardOff));
	setup(&maps[1], KmTime::ForwardState, KmTime::NormalMode,
			KmTime::icon(KmTime::BackwardOff),
			KmTime::icon(KmTime::StoppedOff),
			KmTime::icon(KmTime::ForwardOn));
	setup(&maps[2], KmTime::BackwardState, KmTime::NormalMode,
			KmTime::icon(KmTime::BackwardOn),
			KmTime::icon(KmTime::StoppedOff),
			KmTime::icon(KmTime::ForwardOff));
	setup(&maps[3], KmTime::StoppedState, KmTime::FastMode,
			KmTime::icon(KmTime::FastBackwardOff),
			KmTime::icon(KmTime::StoppedOn),
			KmTime::icon(KmTime::FastForwardOff));
	setup(&maps[4], KmTime::ForwardState, KmTime::FastMode,
			KmTime::icon(KmTime::FastBackwardOff),
			KmTime::icon(KmTime::StoppedOff),
			KmTime::icon(KmTime::FastForwardOn));
	setup(&maps[5], KmTime::BackwardState, KmTime::FastMode,
			KmTime::icon(KmTime::FastBackwardOn),
			KmTime::icon(KmTime::StoppedOff),
			KmTime::icon(KmTime::FastForwardOff));
	setup(&maps[6], KmTime::StoppedState, KmTime::StepMode,
			KmTime::icon(KmTime::StepBackwardOff),
			KmTime::icon(KmTime::StoppedOn),
			KmTime::icon(KmTime::StepForwardOff));
	setup(&maps[7], KmTime::ForwardState, KmTime::StepMode,
			KmTime::icon(KmTime::StepBackwardOff),
			KmTime::icon(KmTime::StoppedOff),
			KmTime::icon(KmTime::StepForwardOn));
	setup(&maps[8], KmTime::BackwardState, KmTime::StepMode,
			KmTime::icon(KmTime::StepBackwardOn),
			KmTime::icon(KmTime::StoppedOff),
			KmTime::icon(KmTime::StepForwardOff));
    }

    if (my.kmtime.state != state || my.kmtime.mode != mode) {
	for (int i = 0; i < nmaps; i++) {
	    if (maps[i].state == state && maps[i].mode == mode) {
		buttonBack->setIcon(*maps[i].back);
		buttonStop->setIcon(*maps[i].stop);
		buttonPlay->setIcon(*maps[i].play);
		break;
	    }
	}
	my.kmtime.state = state;
	my.kmtime.mode = mode;
	if (my.kmtime.mode == KmTime::NormalMode) {
	    buttonSpeed->setEnabled(true);
	    textLabelSpeed->setEnabled(true);
	    lineEditSpeed->setEnabled(true);
	    wheelSpeed->setEnabled(true);
	}
	else {
	    buttonSpeed->setEnabled(false);
	    textLabelSpeed->setEnabled(false);
	    lineEditSpeed->setEnabled(false);
	    wheelSpeed->setEnabled(false);
	}
    }
}

void KmTimeArch::init()
{
    static char UTC[] = "UTC\0Universal Coordinated Time";

    console->post(KmTime::DebugApp, "Starting Archive Time Control...");

    my.units = KmTime::Seconds;
    my.first = true;
    my.tzActions = NULL;
    my.assistant = NULL;

    memset(&my.absoluteStart, 0, sizeof(struct timeval));
    memset(&my.absoluteEnd, 0, sizeof(struct timeval));
    memset(&my.kmtime, 0, sizeof(my.kmtime));
    my.kmtime.source = KmTime::ArchiveSource;
    my.kmtime.delta.tv_sec = KmTime::DefaultDelta;

    my.showMilliseconds = false;
    optionsDetailShow_MillisecondsAction->setChecked(my.showMilliseconds);
    my.showYear = false;
    optionsDetailShow_YearAction->setChecked(my.showYear);

    my.speed = 1.0;
    my.timer = new QTimer(this);
    my.timer->setInterval(timerInterval());
    my.timer->stop();
    connect(my.timer, SIGNAL(timeout()), SLOT(timerTick()));

    addTimezone(UTC);
    displayDeltaText();
    setControl(KmTime::StoppedState, KmTime::NormalMode);

    double delta = KmTime::secondsFromTimeval(&my.kmtime.delta);
    changeSpeed(KmTime::defaultSpeed(delta));
    wheelSpeed->setRange(
		KmTime::minimumSpeed(delta), KmTime::maximumSpeed(delta), 0.1);
    wheelSpeed->setValue(KmTime::defaultSpeed(delta));
    lineEditDelta->setAlignment(Qt::AlignRight);
    lineEditDelta->setValidator(
		new QDoubleValidator(0.001, INT_MAX, 3, lineEditDelta));
    lineEditSpeed->setAlignment(Qt::AlignRight);
    lineEditSpeed->setValidator(
		new QDoubleValidator(0.001, INT_MAX, 1, lineEditSpeed));

    my.bounds = new ShowBounds(this);
    my.bounds->init(&my.absoluteStart, &my.kmtime.start,
			    &my.absoluteEnd, &my.kmtime.end);
    connect(my.bounds, SIGNAL(boundsChanged()), this, SLOT(doneBounds()));

    console->post("KmTimeArch::init absS=%p S=%p absE=%p E=%p\n",
		   &my.absoluteStart, &my.kmtime.start,
		   &my.absoluteEnd, &my.kmtime.end);
}

void KmTimeArch::quit()
{
    if (my.assistant)
	my.assistant->closeAssistant();
}

void KmTimeArch::helpAbout()
{
    AboutDialog about(this);
    about.exec();
}

void KmTimeArch::helpSeeAlso()
{
    SeeAlsoDialog about(this);
    about.exec();
}

void KmTimeArch::whatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}

int KmTimeArch::timerInterval()
{
    return (int)(((my.kmtime.delta.tv_sec * 1000) +
		  (my.kmtime.delta.tv_usec / 1000)) / my.speed);
}

void KmTimeArch::play_clicked()
{
    if (lineEditCtime->isModified())
	lineEditCtime_validate();
    if (lineEditDelta->isModified())
	lineEditDelta_validate();
    if (my.kmtime.state != KmTime::ForwardState ||
	my.kmtime.mode == KmTime::StepMode)
	play();
}

void KmTimeArch::play()
{
    if (addDelta()) {
	setControl(KmTime::ForwardState, my.kmtime.mode);
	updateTime();
	if (my.kmtime.mode == KmTime::NormalMode)
	    my.timer->start(timerInterval());
	else if (my.kmtime.mode == KmTime::FastMode)
	    my.timer->start(KmTime::FastModeDelay);
	console->post(KmTime::DebugApp, "KmTimeArch::play moved time forward");
    } else {
	console->post(KmTime::DebugApp, "KmTimeArch::play reached archive end");
	emit boundsPulse(&my.kmtime);
	stop();
    }
}

void KmTimeArch::back_clicked()
{
    if (lineEditCtime->isModified())
	lineEditCtime_validate();
    if (lineEditDelta->isModified())
	lineEditDelta_validate();
    if (my.kmtime.state != KmTime::BackwardState ||
	my.kmtime.mode == KmTime::StepMode)
	back();
}
    
void KmTimeArch::back()
{
    if (subDelta()) {
	setControl(KmTime::BackwardState, my.kmtime.mode);
	updateTime();
	if (my.kmtime.mode == KmTime::NormalMode)
	    my.timer->start(timerInterval());
	else if (my.kmtime.mode == KmTime::FastMode)
	    my.timer->start(KmTime::FastModeDelay);
	console->post(KmTime::DebugApp, "KmTimeArch::back moved time backward");
    } else {
	console->post(KmTime::DebugApp, "KmTimeArch::back reached archive end");
	emit boundsPulse(&my.kmtime);
	stop();
    }
}

void KmTimeArch::stop_clicked()
{
    if (my.kmtime.state != KmTime::StoppedState)
	stop();
}

void KmTimeArch::stop()
{
    setControl(KmTime::StoppedState, my.kmtime.mode);
    my.timer->stop();
    emit vcrModePulse(&my.kmtime, 0);
    console->post(KmTime::DebugApp, "KmTimeArch::stop stopped time");
}

void KmTimeArch::timerTick()
{
    if (my.kmtime.state == KmTime::ForwardState)
	play();
    else if (my.kmtime.state == KmTime::BackwardState)
	back();
    else
	console->post(KmTime::DebugApp, "KmTimeArch::timerTick: dodgey state?");
}

int KmTimeArch::addDelta()
{
    struct timeval current = my.kmtime.position;

#if DESPERATE
    console->post(KmTime::DebugProtocol,
	"now=%u.%u end=%u.%u start=%u.%u delta=%u.%u speed=%.3e",
    	my.kmtime.position.tv_sec, my.kmtime.position.tv_usec,
	my.kmtime.end.tv_sec, my.kmtime.end.tv_usec, my.kmtime.start.tv_sec,
	my.kmtime.start.tv_usec, my.kmtime.delta.tv_sec,
	my.kmtime.delta.tv_usec, speed);
#endif

    KmTime::timevalAdd(&current, &my.kmtime.delta);
    if (KmTime::timevalCompare(&current, &my.kmtime.end) > 0 ||
	KmTime::timevalCompare(&current, &my.kmtime.start) < 0)
	return 0;
    my.kmtime.position = current;
    return 1;
}

int KmTimeArch::subDelta()
{
    struct timeval current = my.kmtime.position;

    KmTime::timevalSub(&current, &my.kmtime.delta);
    if (KmTime::timevalCompare(&current, &my.kmtime.end) > 0 ||
	KmTime::timevalCompare(&current, &my.kmtime.start) < 0)
	return 0;
    my.kmtime.position = current;
    return 1;
}

void KmTimeArch::changeDelta(int value)
{
    my.units = (KmTime::DeltaUnits)value;
    displayDeltaText();
}

void KmTimeArch::changeControl(int value)
{
    setControl(KmTime::StoppedState, (KmTime::Mode)value);
}

void KmTimeArch::updateTime()
{
    emit timePulse(&my.kmtime);
    displayPositionText();
    displayPositionSlide();
}

void KmTimeArch::displayDeltaText()
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

void KmTimeArch::displayPositionText()
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

void KmTimeArch::displayPositionSlide(void)
{
    sliderPosition->setValue(KmTime::secondsFromTimeval(&my.kmtime.position));
}

void KmTimeArch::setPositionSlideRange(void)
{
    sliderPosition->setRange(KmTime::secondsFromTimeval(&my.kmtime.start),
			     KmTime::secondsFromTimeval(&my.kmtime.end));
}

void KmTimeArch::setPositionSlideDelta(void)
{
    sliderPosition->setStep(KmTime::secondsFromTimeval(&my.kmtime.delta));
}

void KmTimeArch::pressedPosition()
{
    emit vcrModePulse(&my.kmtime, 1);
}

void KmTimeArch::releasedPosition()
{
    emit vcrModePulse(&my.kmtime, 0);
}

void KmTimeArch::changedPosition(double value)
{
#if DESPERATE
    console->post("KmTimeArch::changedPosition changing pos from %d.%d",
			my.kmtime.position.tv_sec, my.kmtime.position.tv_usec);
#endif

    KmTime::secondsToTimeval(value, &my.kmtime.position);
    displayPositionText();

#if DESPERATE
    console->post("KmTimeArch::changedPosition changed pos to %d.%d",
			my.kmtime.position.tv_sec, my.kmtime.position.tv_usec);
#endif
}

void KmTimeArch::clickShowMsec()
{
    if (my.showMilliseconds == true)
	my.showMilliseconds = false;
    else
	my.showMilliseconds = true;
    optionsDetailShow_MillisecondsAction->setChecked(my.showMilliseconds);
    displayPositionText();
}

void KmTimeArch::clickShowYear()
{
    if (my.showYear == true)
	my.showYear = false;
    else
	my.showYear = true;
    optionsDetailShow_YearAction->setChecked(my.showYear);
    displayPositionText();
}

void KmTimeArch::resetSpeed()
{
    double delta = KmTime::secondsFromTimeval(&my.kmtime.delta);
    changeSpeed(KmTime::defaultSpeed(delta));
}

void KmTimeArch::changeSpeed(double value)
{
    QString text;
    int reset = my.timer->isActive();
    double upper, lower, delta = KmTime::secondsFromTimeval(&my.kmtime.delta);

    my.timer->stop();

    upper = KmTime::maximumSpeed(delta);
    lower = KmTime::minimumSpeed(delta);
    if (value > upper)
	value = upper;
    else if (value < lower)
	value = lower;
    text.sprintf("%.1f", value);
    lineEditSpeed->setText(text);
    if (wheelSpeed->value() != value)
	wheelSpeed->setValue(value);

    my.speed = value;
    if (reset)
	my.timer->start(timerInterval());

    console->post("KmTimeArch::changeSpeed changed delta to %d.%d (%.2fs)",
			my.kmtime.delta.tv_sec, my.kmtime.delta.tv_usec, value);
}

void KmTimeArch::showBounds()
{
    my.bounds->reset();
    console->post("KmTimeArch::showBounds: absS=%p S=%p absE=%p E=%p\n",
	&my.absoluteStart, &my.kmtime.start, &my.absoluteEnd, &my.kmtime.end);
    my.bounds->show();
}

void KmTimeArch::doneBounds(void)
{
    int tellclients = 0;

    console->post("KmTimeArch::doneBounds signal received\n");

    my.bounds->flush();
    if (KmTime::timevalCompare(&my.kmtime.position, &my.kmtime.start) < 0) {
	my.kmtime.position = my.kmtime.start;
	tellclients = 1;
    }
    if (KmTime::timevalCompare(&my.kmtime.position, &my.kmtime.end) > 0) {
	my.kmtime.position = my.kmtime.end;
	tellclients = 1;
    }
    setPositionSlideRange();
    if (tellclients)
	emit vcrModePulse(&my.kmtime, 0);
}

void KmTimeArch::showConsole()
{
    console->show();
}

void KmTimeArch::disableConsole()
{
    optionsShowConsoleAction->setVisible(false);
}

void KmTimeArch::hideWindow()
{
    if (isVisible())
	hide();
    else
	show();
}

void KmTimeArch::popup(bool hello_popetts)
{
    if (hello_popetts)
	show();
    else
	hide();
}

void KmTimeArch::closeEvent(QCloseEvent *ce)
{
    hide();
    ce->ignore();
}

void KmTimeArch::lineEditDelta_changed(const QString &)
{
    if (lineEditDelta->isModified())
	stop_clicked();
}

void KmTimeArch::lineEditCtime_changed(const QString &)
{
    if (lineEditCtime->isModified())
	stop_clicked();
}

void KmTimeArch::lineEditDelta_validate()
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

void KmTimeArch::lineEditCtime_validate()
{
    struct timeval current;
    QString input, error;
    char *msg;

    input = lineEditCtime->text().simplified();
    if (input.length() == 0) {
	error.sprintf("Position time has not been set.\n");
	QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	return;
    }
    if (input[0] != '@')
	input.prepend("@");
    if (__pmParseTime(input.toAscii(),
			&my.kmtime.start, &my.kmtime.end, &current, &msg) < 0) {
	error.sprintf("Invalid position date/time:\n\n%s\n", msg);
	QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	displayPositionText();	// reset to previous, known-good position
	free(msg);
    } else {
	my.kmtime.position = current;
	displayPositionText();
	displayPositionSlide();
	emit vcrModePulse(&my.kmtime, 0);
    }
}

void KmTimeArch::lineEditSpeed_validate()
{
    double value, delta = KmTime::secondsFromTimeval(&my.kmtime.delta);
    bool ok, reset = my.timer->isActive();

    value = lineEditSpeed->text().toDouble(&ok);
    if (!ok ||
	value < KmTime::minimumSpeed(delta) ||
	value > KmTime::maximumSpeed(delta)) {
	wheelSpeed->setValue(my.speed);	// reset to previous, known-good speed
    } else {
	my.speed = value;
	wheelSpeed->setValue(my.speed);
	if (reset) {
	    my.timer->stop();
	    my.timer->start(timerInterval());
	}
    }
}

void KmTimeArch::setTimezone(QAction *action)
{
    for (int i = 0; i < my.tzlist.size(); i++) {
	TimeZone *tz = my.tzlist.at(i);
	if (tz->action() == action) {
	    my.first = true;	// resetting time completely
	    pmUseZone(tz->handle());
	    emit tzPulse(&my.kmtime, tz->tz(), strlen(tz->tz()) + 1,
				tz->tzlabel(), strlen(tz->tzlabel()) + 1);
	    console->post("KmTimeArch::setTimezone sent TZ %s (%s) to clients",
				tz->tz(), tz->tzlabel());
	    setTime(&my.kmtime, NULL);	// re-display the time, no messages
	    break;
	}
    }
}

void KmTimeArch::addTimezone(char *string)
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
	connect(my.tzActions, SIGNAL(selected(QAction *)),
			this, SLOT(setTimezone(QAction *)));
    }
    my.tzActions->addAction(tzAction);
    optionsTimezoneAction->addActions(my.tzActions->actions());
    console->post("KmTimeArch::addTimezone added tz=%s label=%s", tz, label);
}

void KmTimeArch::setTime(KmTime::Packet *k, char *tzdata)
{
#if DESPERATE
    console->post(KmTime::DebugProtocol, "KmTimeArch::setTime START: "
	"1st=%d now=%u.%u end=%u.%u start=%u.%u delta=%u.%u",
	my.first, my.kmtime.position.tv_sec, my.kmtime.position.tv_usec,
	my.kmtime.end.tv_sec, my.kmtime.end.tv_usec, my.kmtime.start.tv_sec,
	my.kmtime.start.tv_usec, my.kmtime.delta.tv_sec,
	my.kmtime.delta.tv_usec);
#endif

    if (my.first == true) {
	my.first = false;
	if (tzdata != NULL)
	    addTimezone(tzdata);
	my.absoluteStart = my.kmtime.start = k->start;
	my.absoluteEnd = my.kmtime.end = k->end;
	my.kmtime.position = k->position;
	my.kmtime.delta = k->delta;
	sliderPosition->blockSignals(true);
	setPositionSlideRange();
	setPositionSlideDelta();
	sliderPosition->blockSignals(false);
	displayDeltaText();
	displayPositionText();
	displayPositionSlide();
	my.bounds->reset();
	double delta = KmTime::secondsFromTimeval(&k->delta);
	changeSpeed(KmTime::defaultSpeed(delta));
    } else {
	addBound(k, tzdata);
    }

#if DESPERATE
    console->post(KmTime::DebugProtocol, "KmTimeArch::setTime ENDED: "
	"1st=%d now=%u.%u end=%u.%u start=%u.%u delta=%u.%u",
	my.first, my.kmtime.position.tv_sec, my.kmtime.position.tv_usec,
	my.kmtime.end.tv_sec, my.kmtime.end.tv_usec, my.kmtime.start.tv_sec,
	my.kmtime.start.tv_usec, my.kmtime.delta.tv_sec,
	my.kmtime.delta.tv_usec);
#endif
}

void KmTimeArch::addBound(KmTime::Packet *k, char *tzdata)
{
    // Note: kmchart can start kmtime up without an archive
    // so, we need to explicitly initialise some fields now
    // that one might otherwise have expected to be setup.

    bool needPulse = KmTime::timevalNonZero(&my.kmtime.position);

    console->post(KmTime::DebugProtocol, "KmTimeArch::addBound START: "
		"p?=%d now=%u.%u end=%u.%u start=%u.%u", needPulse,
		my.kmtime.position.tv_sec, my.kmtime.position.tv_usec,
		my.kmtime.end.tv_sec, my.kmtime.end.tv_usec,
		my.kmtime.start.tv_sec, my.kmtime.start.tv_usec);

    if (tzdata != NULL)
	addTimezone(tzdata);

    if (KmTime::timevalCompare(&k->start, &my.absoluteStart) < 0 || needPulse)
	my.absoluteStart = k->start;
    if (KmTime::timevalCompare(&k->end, &my.absoluteEnd) > 0 || needPulse)
	my.absoluteEnd = k->end;
    if (!needPulse) {	// first-time archive initialisation
	my.kmtime.position = k->position;
	my.kmtime.start = k->start;
	my.kmtime.end = k->end;
    }

    setPositionSlideRange();
    my.bounds->reset();

    if (needPulse)
	emit vcrModePulse(&my.kmtime, 0);

    console->post(KmTime::DebugProtocol, "KmTimeArch::addBound ENDED: "
		"p?=%d now=%u.%u end=%u.%u start=%u.%u", needPulse,
		my.kmtime.position.tv_sec, my.kmtime.position.tv_usec,
		my.kmtime.end.tv_sec, my.kmtime.end.tv_usec,
		my.kmtime.start.tv_sec, my.kmtime.start.tv_usec);
}

void KmTimeArch::assistantError(const QString &msg)
{
    QMessageBox::warning(this, pmProgname, msg);
}

void KmTimeArch::setupAssistant()
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

void KmTimeArch::helpManual()
{
    setupAssistant();
    QString documents = HTMLDIR;
    my.assistant->showPage(documents.append("/timecontrol.html"));
}
