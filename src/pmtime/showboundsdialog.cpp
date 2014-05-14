/*
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#include "showboundsdialog.h"
#include <QtGui/QMessageBox>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "pmtime.h"

ShowBounds::ShowBounds(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
}

void ShowBounds::init(
	struct timeval *absoluteStart, struct timeval *currentStart,
	struct timeval *absoluteEnd, struct timeval *currentEnd)
{
    my.absoluteStart = absoluteStart;
    my.currentStart = currentStart;
    my.absoluteEnd = absoluteEnd;
    my.currentEnd = currentEnd;
    reset();
}

void ShowBounds::reset()
{
    my.localAbsoluteStart = PmTime::secondsFromTimeval(my.absoluteStart);
    my.localCurrentStart = PmTime::secondsFromTimeval(my.currentStart);
    my.localAbsoluteEnd = PmTime::secondsFromTimeval(my.absoluteEnd);
    my.localCurrentEnd = PmTime::secondsFromTimeval(my.currentEnd);

#ifdef DESPERATE
    console->post(PmTime::DebugProtocol, "ShowBounds::reset START: "
	"end=%u.%u(%.3f) start=%u.%u(%.3f) lend=%u.%u(%.3f) lstart=%u.%u(%.3f)",
	my.absoluteEnd->tv_sec, my.absoluteEnd->tv_usec, my.localAbsoluteEnd,
	my.absoluteStart->tv_sec, my.absoluteStart->tv_usec,
	my.localAbsoluteStart,
	my.currentEnd->tv_sec, my.currentEnd->tv_usec, my.localCurrentEnd,
	my.currentStart->tv_sec, my.currentStart->tv_usec,
	my.localCurrentStart);
#endif

    displayStartSlider();
    displayEndSlider();
    displayStartText();
    displayEndText();

#ifdef DESPERATE
    console->post(PmTime::DebugProtocol, "ShowBounds::reset ENDED: "
	"end=%u.%u(%.3f) start=%u.%u(%.3f) lend=%u.%u(%.3f) lstart=%u.%u(%.3f)",
	my.absoluteEnd->tv_sec, my.absoluteEnd->tv_usec, my.localAbsoluteEnd,
	my.absoluteStart->tv_sec, my.absoluteStart->tv_usec,
	my.localAbsoluteStart,
	my.currentEnd->tv_sec, my.currentEnd->tv_usec, my.localCurrentEnd,
	my.currentStart->tv_sec, my.currentStart->tv_usec,
	my.localCurrentStart);
#endif
}

void ShowBounds::displayStartSlider()
{
    sliderStart->blockSignals(true);
    sliderStart->setRange(my.localAbsoluteStart, my.localAbsoluteEnd);
    sliderStart->setValue(my.localCurrentStart);
    sliderStart->blockSignals(false);
}

void ShowBounds::displayEndSlider()
{
    sliderEnd->blockSignals(true);
    sliderEnd->setRange(my.localAbsoluteStart, my.localAbsoluteEnd);
    sliderEnd->setValue(my.localCurrentEnd);
    sliderEnd->blockSignals(false);
}

void ShowBounds::displayStartText()
{
    time_t clock = (time_t)my.localCurrentStart;
    char ctimebuf[32];

    pmCtime(&clock, ctimebuf);
    lineEditStart->setText(tr(ctimebuf).simplified());

    console->post("ShowBounds::displayStartText clock=%.3f - %s",
			my.localCurrentStart,
			(const char *)lineEditStart->text().toAscii());
}

void ShowBounds::displayEndText()
{
    time_t clock = (time_t)my.localCurrentEnd;
    char ctimebuf[32];

    pmCtime(&clock, ctimebuf);
    lineEditEnd->setText(tr(ctimebuf).simplified());
}

void ShowBounds::changedStart(double value)
{
    if (value != my.localCurrentStart) {
	console->post("ShowBounds::changedStart: %.3f -> %.3f",
				my.localCurrentStart, value);
	my.localCurrentStart = value;
	displayStartSlider();
	displayStartText();
	if (my.localCurrentStart > my.localCurrentEnd)
	    sliderEnd->setValue(value);
    }
}

void ShowBounds::changedEnd(double value)
{
    if (value != my.localCurrentEnd) {
	console->post("ShowBounds::changedEnd %.3f -> %.3f", __func__,
				my.localCurrentEnd, value);
	my.localCurrentEnd = value;
	displayEndSlider();
	displayEndText();
	if (my.localCurrentStart > my.localCurrentEnd)
	    sliderStart->setValue(value);
    }
}

//
// This routine just verifies that input from the user is correct,
// before allowing the window to be closed.  If theres invalid date
// strings in start/end text boxes, we must deny the "OK" press.
// Actual work of updating pmtime is done in flush().
//
void ShowBounds::accept()
{
    struct timeval current, start, end;
    QString error, input;
    char *msg;

    console->post("ShowBounds::accept: OK pressed");

    PmTime::secondsToTimeval(my.localAbsoluteStart, &start);
    PmTime::secondsToTimeval(my.localAbsoluteEnd, &end);

    if (lineEditStart->isModified()) {
	input = lineEditStart->text().simplified();
	if (input.length() == 0) {
	    error.sprintf("Start time has not been set.\n");
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    return;
	}
	if (input[0] != '@')
	    input.prepend("@");
	if (__pmParseTime(input.toAscii(), &start, &end, &current, &msg) < 0) {
	    error.sprintf("Invalid start date/time:\n\n%s\n", msg);
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    free(msg);
	    return;
	} else if (PmTime::timevalCompare(&current, &start) < 0 ||
		   PmTime::timevalCompare(&current, &end) > 0) {
	    error.sprintf("Start time is outside archive boundaries\n");
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    return;
	}
	my.localCurrentStart = PmTime::secondsFromTimeval(&current);
	console->post("ShowBounds::accept start=%.2f (abs=%.2f-%.2f)",
			my.localCurrentStart, my.localAbsoluteStart,
			my.localAbsoluteEnd);
    }

    if (lineEditEnd->isModified()) {
	input = lineEditEnd->text().simplified();
	if (input.length() == 0) {
	    error.sprintf("End time has not been set.\n");
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    return;
	}
	if (input[0] != '@')
	    input.prepend("@");
	if (__pmParseTime(input.toAscii(), &start, &end, &current, &msg) < 0) {
	    error.sprintf("Invalid end date/time:\n%s\n\n", msg);
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    free(msg);
	    return;
	} else if (PmTime::timevalCompare(&current, &start) < 0 ||
		   PmTime::timevalCompare(&current, &end) > 0) {
	    error.sprintf("End time is outside the archive boundaries\n");
	    QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	    return;
	}
	my.localCurrentEnd = PmTime::secondsFromTimeval(&current);
	console->post("ShowBounds::accept end=%.2f (abs=%.2f-%.2f)",
		my.localCurrentEnd, my.localAbsoluteStart, my.localAbsoluteEnd);
    }

    if (my.localCurrentStart > my.localCurrentEnd) {
	error.sprintf("Start time must be less than end time.\n");
	QMessageBox::warning(0, tr("Warning"), error, tr("Quit"));
	return;
    }

    emit boundsChanged();
    done(0);
}

void ShowBounds::reject()
{
    done(1);
}

//
// Inform parent pmtime window of accepted changes (start/end),
// via the pointers-to-struct-timevals we were initially given.
//
void ShowBounds::flush()
{
    struct timeval start, end;

    PmTime::secondsToTimeval(my.localCurrentStart, &start);
    PmTime::secondsToTimeval(my.localCurrentEnd, &end);

    console->post("ShowBounds::flush updating bounds to %.2f->%.2f",
			PmTime::secondsFromTimeval(&start),
			PmTime::secondsFromTimeval(&end));

    if (PmTime::timevalCompare(&start, my.currentStart) != 0)
	*my.currentStart = start;
    if (PmTime::timevalCompare(&end, my.currentEnd) != 0)
	*my.currentEnd = end;
}
