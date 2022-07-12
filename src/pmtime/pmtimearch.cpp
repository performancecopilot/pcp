/*
 * Copyright (c) 2012-2017, Red Hat.
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
#include "pmtimearch.h"

#include <QTimer>
#include <QValidator>
#include <QMessageBox>
#include <pcp/pmapi.h>
#include <pcp/libpcp.h>
#include "aboutdialog.h"
#include "seealsodialog.h"

PmTimeArch::PmTimeArch() : PmTime()
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
    PmTime::Mode	mode;
    QIcon 		*back;
    QIcon 		*stop;
    QIcon 		*play;
} ArchIconStateMap;

static void setup(ArchIconStateMap *map, PmTime::State state, PmTime::Mode mode,
		  QIcon *back, QIcon *stop, QIcon *play)
{
    map->state = state;
    map->mode = mode;
    map->back = back;
    map->stop = stop;
    map->play = play;
}

void PmTimeArch::setControl(PmTime::State state, PmTime::Mode mode)
{
    static ArchIconStateMap maps[3 * 3];
    static int nmaps;

    if (!nmaps) {
	nmaps = sizeof(maps) / sizeof(maps[0]);
	setup(&maps[0], PmTime::StoppedState, PmTime::NormalMode,
			PmTime::icon(PmTime::BackwardOff),
			PmTime::icon(PmTime::StoppedOn),
			PmTime::icon(PmTime::ForwardOff));
	setup(&maps[1], PmTime::ForwardState, PmTime::NormalMode,
			PmTime::icon(PmTime::BackwardOff),
			PmTime::icon(PmTime::StoppedOff),
			PmTime::icon(PmTime::ForwardOn));
	setup(&maps[2], PmTime::BackwardState, PmTime::NormalMode,
			PmTime::icon(PmTime::BackwardOn),
			PmTime::icon(PmTime::StoppedOff),
			PmTime::icon(PmTime::ForwardOff));
	setup(&maps[3], PmTime::StoppedState, PmTime::FastMode,
			PmTime::icon(PmTime::FastBackwardOff),
			PmTime::icon(PmTime::StoppedOn),
			PmTime::icon(PmTime::FastForwardOff));
	setup(&maps[4], PmTime::ForwardState, PmTime::FastMode,
			PmTime::icon(PmTime::FastBackwardOff),
			PmTime::icon(PmTime::StoppedOff),
			PmTime::icon(PmTime::FastForwardOn));
	setup(&maps[5], PmTime::BackwardState, PmTime::FastMode,
			PmTime::icon(PmTime::FastBackwardOn),
			PmTime::icon(PmTime::StoppedOff),
			PmTime::icon(PmTime::FastForwardOff));
	setup(&maps[6], PmTime::StoppedState, PmTime::StepMode,
			PmTime::icon(PmTime::StepBackwardOff),
			PmTime::icon(PmTime::StoppedOn),
			PmTime::icon(PmTime::StepForwardOff));
	setup(&maps[7], PmTime::ForwardState, PmTime::StepMode,
			PmTime::icon(PmTime::StepBackwardOff),
			PmTime::icon(PmTime::StoppedOff),
			PmTime::icon(PmTime::StepForwardOn));
	setup(&maps[8], PmTime::BackwardState, PmTime::StepMode,
			PmTime::icon(PmTime::StepBackwardOn),
			PmTime::icon(PmTime::StoppedOff),
			PmTime::icon(PmTime::StepForwardOff));
    }

    if (my.pmtime.state != state || my.pmtime.mode != mode) {
	for (int i = 0; i < nmaps; i++) {
	    if (maps[i].state == state && maps[i].mode == mode) {
		buttonBack->setIcon(*maps[i].back);
		buttonStop->setIcon(*maps[i].stop);
		buttonPlay->setIcon(*maps[i].play);
		break;
	    }
	}
	my.pmtime.state = state;
	my.pmtime.mode = mode;
	if (my.pmtime.mode == PmTime::NormalMode) {
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

void PmTimeArch::init()
{
    static char UTC[] = "UTC\0Universal Coordinated Time";
    double delta, minspeed, maxspeed;

    console->post(PmTime::DebugApp, "Starting Archive Time Control...");

    my.units = PmTime::Seconds;
    my.first = true;
    my.tzActions = NULL;

    memset(&my.absoluteStart, 0, sizeof(struct timeval));
    memset(&my.absoluteEnd, 0, sizeof(struct timeval));
    memset(&my.pmtime, 0, sizeof(my.pmtime));
    my.pmtime.source = PmTime::ArchiveSource;
    my.pmtime.delta.tv_sec = PmTime::DefaultDelta;

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
    setControl(PmTime::StoppedState, PmTime::NormalMode);

    delta = PmTime::secondsFromTimeval(&my.pmtime.delta);
    minspeed = PmTime::minimumSpeed(delta);
    maxspeed = PmTime::maximumSpeed(delta);
    changeSpeed(PmTime::defaultSpeed(delta));
    wheelSpeed->setRange(minspeed, maxspeed);
    wheelSpeed->setSingleStep(0.1);
    wheelSpeed->setValue(PmTime::defaultSpeed(delta));
    lineEditDelta->setAlignment(Qt::AlignRight);
    lineEditDelta->setValidator(
		new QDoubleValidator(0.001, INT_MAX, 3, lineEditDelta));
    lineEditSpeed->setAlignment(Qt::AlignRight);
    lineEditSpeed->setValidator(
		new QDoubleValidator(0.001, INT_MAX, 1, lineEditSpeed));

    my.bounds = new ShowBounds(this);
    my.bounds->init(&my.absoluteStart, &my.pmtime.start,
			    &my.absoluteEnd, &my.pmtime.end);
    connect(my.bounds, SIGNAL(boundsChanged()), this, SLOT(doneBounds()));

    console->post("PmTimeArch::init absS=%p S=%p absE=%p E=%p\n",
		   &my.absoluteStart, &my.pmtime.start,
		   &my.absoluteEnd, &my.pmtime.end);
}

void PmTimeArch::quit()
{
    console->post("arch quit!\n");
}

int PmTimeArch::timerInterval()
{
    return (int)(((my.pmtime.delta.tv_sec * 1000) +
		  (my.pmtime.delta.tv_usec / 1000)) / my.speed);
}

void PmTimeArch::play_clicked()
{
    if (lineEditCtime->isModified())
	lineEditCtime_validate();
    if (lineEditDelta->isModified())
	lineEditDelta_validate();
    if (my.pmtime.state != PmTime::ForwardState ||
	my.pmtime.mode == PmTime::StepMode)
	play();
}

void PmTimeArch::play()
{
    if (addDelta()) {
	setControl(PmTime::ForwardState, my.pmtime.mode);
	updateTime();
	if (my.pmtime.mode == PmTime::NormalMode)
	    my.timer->start(timerInterval());
	else if (my.pmtime.mode == PmTime::FastMode)
	    my.timer->start(PmTime::FastModeDelay);
	console->post(PmTime::DebugApp, "PmTimeArch::play moved time forward");
    } else {
	console->post(PmTime::DebugApp, "PmTimeArch::play reached archive end");
	emit boundsPulse(&my.pmtime);
	stop();
    }
}

void PmTimeArch::back_clicked()
{
    if (lineEditCtime->isModified())
	lineEditCtime_validate();
    if (lineEditDelta->isModified())
	lineEditDelta_validate();
    if (my.pmtime.state != PmTime::BackwardState ||
	my.pmtime.mode == PmTime::StepMode)
	back();
}
    
void PmTimeArch::back()
{
    if (subDelta()) {
	setControl(PmTime::BackwardState, my.pmtime.mode);
	updateTime();
	if (my.pmtime.mode == PmTime::NormalMode)
	    my.timer->start(timerInterval());
	else if (my.pmtime.mode == PmTime::FastMode)
	    my.timer->start(PmTime::FastModeDelay);
	console->post(PmTime::DebugApp, "PmTimeArch::back moved time backward");
    } else {
	console->post(PmTime::DebugApp, "PmTimeArch::back reached archive end");
	emit boundsPulse(&my.pmtime);
	stop();
    }
}

void PmTimeArch::stop_clicked()
{
    if (my.pmtime.state != PmTime::StoppedState)
	stop();
}

void PmTimeArch::stop()
{
    setControl(PmTime::StoppedState, my.pmtime.mode);
    my.timer->stop();
    emit vcrModePulse(&my.pmtime, 0);
    console->post(PmTime::DebugApp, "PmTimeArch::stop stopped time");
}

void PmTimeArch::timerTick()
{
    if (my.pmtime.state == PmTime::ForwardState)
	play();
    else if (my.pmtime.state == PmTime::BackwardState)
	back();
    else
	console->post(PmTime::DebugApp, "PmTimeArch::timerTick: dodgey state?");
}

int PmTimeArch::addDelta()
{
    struct timeval current = my.pmtime.position;

#if DESPERATE
    console->post(PmTime::DebugProtocol,
	"now=%u.%u end=%u.%u start=%u.%u delta=%u.%u speed=%.3e",
    	my.pmtime.position.tv_sec, my.pmtime.position.tv_usec,
	my.pmtime.end.tv_sec, my.pmtime.end.tv_usec, my.pmtime.start.tv_sec,
	my.pmtime.start.tv_usec, my.pmtime.delta.tv_sec,
	my.pmtime.delta.tv_usec, speed);
#endif

    PmTime::timevalAdd(&current, &my.pmtime.delta);
    if (PmTime::timevalCompare(&current, &my.pmtime.end) > 0 ||
	PmTime::timevalCompare(&current, &my.pmtime.start) < 0)
	return 0;
    my.pmtime.position = current;
    return 1;
}

int PmTimeArch::subDelta()
{
    struct timeval current = my.pmtime.position;

    PmTime::timevalSub(&current, &my.pmtime.delta);
    if (PmTime::timevalCompare(&current, &my.pmtime.end) > 0 ||
	PmTime::timevalCompare(&current, &my.pmtime.start) < 0)
	return 0;
    my.pmtime.position = current;
    return 1;
}

void PmTimeArch::changeDelta(int value)
{
    my.units = (PmTime::DeltaUnits)value;
    displayDeltaText();
}

void PmTimeArch::changeControl(int value)
{
    setControl(PmTime::StoppedState, (PmTime::Mode)value);
}

void PmTimeArch::updateTime()
{
    emit timePulse(&my.pmtime);
    displayPositionText();
    displayPositionSlide();
}

void PmTimeArch::displayDeltaText()
{
    char text[64];
    double delta = PmTime::secondsFromTimeval(&my.pmtime.delta);

    delta = PmTime::secondsToUnits(delta, my.units);
    if ((double)(int)delta == delta)
	pmsprintf(text, sizeof(text), "%.2f", delta);
    else
	pmsprintf(text, sizeof(text), "%.6f", delta);
    lineEditDelta->setText(QString(text));
}

void PmTimeArch::displayPositionText()
{
    QString text;
    char ctimebuf[32], msecbuf[5];

    pmCtime((const time_t*)&my.pmtime.position.tv_sec, ctimebuf);
    text = tr(ctimebuf);
    if (my.showYear == false)
	text.remove(19, 5);
    if (my.showMilliseconds == true) {
	pmsprintf(msecbuf, 5, ".%03u", (uint)my.pmtime.position.tv_usec/1000);
	text.insert(19, msecbuf);
    }
    lineEditCtime->setText(text.simplified());
}

void PmTimeArch::displayPositionSlide(void)
{
    sliderPosition->setValue(PmTime::secondsFromTimeval(&my.pmtime.position));
}

void PmTimeArch::setPositionSlideScale(void)
{
    double delta, start, end;

    delta = PmTime::secondsFromTimeval(&my.pmtime.delta);
    start = PmTime::secondsFromTimeval(&my.pmtime.start);
    end = PmTime::secondsFromTimeval(&my.pmtime.end);

    sliderPosition->blockSignals(true);
    sliderPosition->setScale(start, end);
    sliderPosition->setTotalSteps((end - start) / delta);
    sliderPosition->blockSignals(false);
}

void PmTimeArch::setPositionSlideRange(void)
{
    double start, end;

    start = PmTime::secondsFromTimeval(&my.pmtime.start);
    end = PmTime::secondsFromTimeval(&my.pmtime.end);

    sliderPosition->blockSignals(true);
    sliderPosition->setScale(start, end);
    sliderPosition->blockSignals(false);
}

void PmTimeArch::pressedPosition()
{
    emit vcrModePulse(&my.pmtime, 1);
}

void PmTimeArch::releasedPosition()
{
    emit vcrModePulse(&my.pmtime, 0);
}

void PmTimeArch::changedPosition(double value)
{
#if DESPERATE
    console->post("PmTimeArch::changedPosition changing pos from %d.%d",
			my.pmtime.position.tv_sec, my.pmtime.position.tv_usec);
#endif

    PmTime::secondsToTimeval(value, &my.pmtime.position);
    displayPositionText();

#if DESPERATE
    console->post("PmTimeArch::changedPosition changed pos to %d.%d",
			my.pmtime.position.tv_sec, my.pmtime.position.tv_usec);
#endif
}

void PmTimeArch::clickShowMsec()
{
    if (my.showMilliseconds == true)
	my.showMilliseconds = false;
    else
	my.showMilliseconds = true;
    optionsDetailShow_MillisecondsAction->setChecked(my.showMilliseconds);
    displayPositionText();
}

void PmTimeArch::clickShowYear()
{
    if (my.showYear == true)
	my.showYear = false;
    else
	my.showYear = true;
    optionsDetailShow_YearAction->setChecked(my.showYear);
    displayPositionText();
}

void PmTimeArch::resetSpeed()
{
    double delta = PmTime::secondsFromTimeval(&my.pmtime.delta);
    changeSpeed(PmTime::defaultSpeed(delta));
}

void PmTimeArch::changeSpeed(double value)
{
    char text[64];
    int reset = my.timer->isActive();
    double upper, lower, delta = PmTime::secondsFromTimeval(&my.pmtime.delta);

    my.timer->stop();

    upper = PmTime::maximumSpeed(delta);
    lower = PmTime::minimumSpeed(delta);
    if (value > upper)
	value = upper;
    else if (value < lower)
	value = lower;
    pmsprintf(text, sizeof(text), "%.1f", value);
    lineEditSpeed->setText(QString(text));
    if (wheelSpeed->value() != value)
	wheelSpeed->setValue(value);

    my.speed = value;
    if (reset)
	my.timer->start(timerInterval());

    console->post("PmTimeArch::changeSpeed changed delta to %llu.%u (%.2fs)",
			(unsigned long long) my.pmtime.delta.tv_sec,
			(unsigned int) my.pmtime.delta.tv_usec, value);
}

void PmTimeArch::showBounds()
{
    my.bounds->reset();
    console->post("PmTimeArch::showBounds: absS=%p S=%p absE=%p E=%p\n",
	&my.absoluteStart, &my.pmtime.start, &my.absoluteEnd, &my.pmtime.end);
    my.bounds->show();
}

void PmTimeArch::doneBounds(void)
{
    int tellclients = 0;

    console->post("PmTimeArch::doneBounds signal received\n");

    my.bounds->flush();
    if (PmTime::timevalCompare(&my.pmtime.position, &my.pmtime.start) < 0) {
	my.pmtime.position = my.pmtime.start;
	tellclients = 1;
    }
    if (PmTime::timevalCompare(&my.pmtime.position, &my.pmtime.end) > 0) {
	my.pmtime.position = my.pmtime.end;
	tellclients = 1;
    }
    setPositionSlideRange();
    if (tellclients)
	emit vcrModePulse(&my.pmtime, 0);
}

void PmTimeArch::disableConsole()
{
    optionsShowConsoleAction->setVisible(false);
}

void PmTimeArch::lineEditDelta_changed(const QString &)
{
    if (lineEditDelta->isModified())
	stop_clicked();
}

void PmTimeArch::lineEditCtime_changed(const QString &)
{
    if (lineEditCtime->isModified())
	stop_clicked();
}

void PmTimeArch::lineEditDelta_validate()
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

void PmTimeArch::lineEditCtime_validate()
{
    struct timeval current;
    QString input, error;
    char *msg;

    input = lineEditCtime->text().simplified();
    if (input.length() == 0) {
	error = QString("Position time has not been set.\n");
	QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	return;
    }
    if (input[0] != '@')
	input.prepend("@");
    if (__pmParseTime(input.toLatin1(),
			&my.pmtime.start, &my.pmtime.end, &current, &msg) < 0) {
	error = QString("Invalid position date/time:\n\n%1\n").arg(msg);
	QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	displayPositionText();	// reset to previous, known-good position
	free(msg);
    } else {
	my.pmtime.position = current;
	displayPositionText();
	displayPositionSlide();
	emit vcrModePulse(&my.pmtime, 0);
    }
}

void PmTimeArch::lineEditSpeed_validate()
{
    double value, delta = PmTime::secondsFromTimeval(&my.pmtime.delta);
    bool ok, reset = my.timer->isActive();

    value = lineEditSpeed->text().toDouble(&ok);
    if (!ok ||
	value < PmTime::minimumSpeed(delta) ||
	value > PmTime::maximumSpeed(delta)) {
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

void PmTimeArch::setTimezone(QAction *action)
{
    for (int i = 0; i < my.tzlist.size(); i++) {
	TimeZone *tz = my.tzlist.at(i);
	if (tz->action() == action) {
	    my.first = true;	// resetting time completely
	    pmUseZone(tz->handle());
	    emit tzPulse(&my.pmtime, tz->tz(), strlen(tz->tz()) + 1,
				tz->tzlabel(), strlen(tz->tzlabel()) + 1);
	    console->post("PmTimeArch::setTimezone sent TZ %s (%s) to clients",
				tz->tz(), tz->tzlabel());
	    setTime(&my.pmtime, NULL);	// re-display the time, no messages
	    break;
	}
    }
}

void PmTimeArch::addTimezone(const char *string)
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
	connect(my.tzActions, SIGNAL(triggered(QAction *)),
			this, SLOT(setTimezone(QAction *)));
	connect(my.tzActions, SIGNAL(selected(QAction *)),
			this, SLOT(setTimezone(QAction *)));
    }
    my.tzActions->addAction(tzAction);
    optionsTimezoneAction->addActions(my.tzActions->actions());
    console->post("PmTimeArch::addTimezone added tz=%s label=%s", tz, label);
}

void PmTimeArch::setTime(PmTime::Packet *k, char *tzdata)
{
#if DESPERATE
    console->post(PmTime::DebugProtocol, "PmTimeArch::setTime START: "
	"1st=%d now=%u.%u end=%u.%u start=%u.%u delta=%u.%u",
	my.first, my.pmtime.position.tv_sec, my.pmtime.position.tv_usec,
	my.pmtime.end.tv_sec, my.pmtime.end.tv_usec, my.pmtime.start.tv_sec,
	my.pmtime.start.tv_usec, my.pmtime.delta.tv_sec,
	my.pmtime.delta.tv_usec);
#endif

    if (my.first == true) {
	my.first = false;
	if (tzdata != NULL)
	    addTimezone(tzdata);
	my.absoluteStart = my.pmtime.start = k->start;
	my.absoluteEnd = my.pmtime.end = k->end;
	my.pmtime.position = k->position;
	my.pmtime.delta = k->delta;
	setPositionSlideScale();
	displayDeltaText();
	displayPositionText();
	displayPositionSlide();
	my.bounds->reset();
	double delta = PmTime::secondsFromTimeval(&k->delta);
	changeSpeed(PmTime::defaultSpeed(delta));
    } else {
	addBound(k, tzdata);
    }

#if DESPERATE
    console->post(PmTime::DebugProtocol, "PmTimeArch::setTime ENDED: "
	"1st=%d now=%u.%u end=%u.%u start=%u.%u delta=%u.%u",
	my.first, my.pmtime.position.tv_sec, my.pmtime.position.tv_usec,
	my.pmtime.end.tv_sec, my.pmtime.end.tv_usec, my.pmtime.start.tv_sec,
	my.pmtime.start.tv_usec, my.pmtime.delta.tv_sec,
	my.pmtime.delta.tv_usec);
#endif
}

void PmTimeArch::addBound(PmTime::Packet *k, char *tzdata)
{
    // Note: pmchart can start pmtime up without an archive
    // so, we need to explicitly initialise some fields now
    // that one might otherwise have expected to be setup.

    bool needPulse = PmTime::timevalNonZero(&my.pmtime.position);

    console->post(PmTime::DebugProtocol, "PmTimeArch::addBound START: "
		"p?=%d now=%llu.%u end=%llu.%u start=%llu.%u", needPulse,
		(unsigned long long) my.pmtime.position.tv_sec,
		(unsigned int) my.pmtime.position.tv_usec,
		(unsigned long long) my.pmtime.end.tv_sec,
		(unsigned int) my.pmtime.end.tv_usec,
		(unsigned long long) my.pmtime.start.tv_sec,
		(unsigned int) my.pmtime.start.tv_usec);

    if (tzdata != NULL)
	addTimezone(tzdata);

    if (PmTime::timevalCompare(&k->start, &my.absoluteStart) < 0)
	my.absoluteStart = my.pmtime.start = k->start;
    if (PmTime::timevalCompare(&k->end, &my.absoluteEnd) > 0)
	my.absoluteEnd = my.pmtime.end = k->end;
    if (!needPulse) {	// first-time archive initialisation
	my.pmtime.position = k->position;
	my.pmtime.start = k->start;
	my.pmtime.end = k->end;
    }

    setPositionSlideRange();
    displayPositionText();
    displayPositionSlide();
    my.bounds->reset();

    if (needPulse)
	emit vcrModePulse(&my.pmtime, 0);

    console->post(PmTime::DebugProtocol, "PmTimeArch::addBound ENDED: "
		"p?=%d now=%llu.%u end=%llu.%u start=%llu.%u", needPulse,
		(unsigned long long) my.pmtime.position.tv_sec,
		(unsigned int) my.pmtime.position.tv_usec,
		(unsigned long long) my.pmtime.end.tv_sec,
		(unsigned int) my.pmtime.end.tv_usec,
		(unsigned long long) my.pmtime.start.tv_sec,
		(unsigned int) my.pmtime.start.tv_usec);
}
