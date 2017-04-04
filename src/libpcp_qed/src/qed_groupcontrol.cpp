/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2007-2009, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include "qed_groupcontrol.h"
#include "qed_console.h"
#include "qed_app.h"

QedGroupControl::QedGroupControl()
{
    my.realDelta = 0;
    my.realPosition = 0;
    my.timeState = StartState;
    my.buttonState = QedTimeButton::Timeless;
    my.pmtimeState = QmcTime::StoppedState;
    memset(&my.delta, 0, sizeof(struct timeval));
    memset(&my.position, 0, sizeof(struct timeval));
}

void QedGroupControl::init(struct timeval *interval, struct timeval *position)
{
    if (isArchiveSource()) {
	my.pmtimeState = QmcTime::StoppedState;
	my.buttonState = QedTimeButton::StoppedArchive;
    }
    else {
	my.pmtimeState = QmcTime::ForwardState;
	my.buttonState = QedTimeButton::ForwardLive;
    }
    my.delta = *interval;
    my.position = *position;
    my.realDelta = QedApp::timevalToSeconds(*interval);
    my.realPosition = QedApp::timevalToSeconds(*position);
}

QmcTime::State QedGroupControl::pmtimeState(void)
{
    return my.pmtimeState;
}

char *QedGroupControl::timeState()
{
    static char buf[16];

    switch (my.timeState) {
    case StartState:	strcpy(buf, "Start"); break;
    case ForwardState:	strcpy(buf, "Forward"); break;
    case BackwardState:	strcpy(buf, "Backward"); break;
    case EndLogState:	strcpy(buf, "EndLog"); break;
    case StandbyState:	strcpy(buf, "Standby"); break;
    default:		strcpy(buf, "Dodgey"); break;
    }
    return buf;
}

//
// Setup the initial data needed after opening a view.
// All of the work is in archive mode; in live mode we have
// not yet got any historical data that we can display...
//
void QedGroupControl::setupWorldView(struct timeval *interval,
	struct timeval *position, struct timeval *start, struct timeval *end)
{
    if (isArchiveSource() == false)
	return;

    QmcTime::Packet packet;
    packet.source = QmcTime::ArchiveSource;
    packet.state = QmcTime::ForwardState;
    packet.mode = QmcTime::NormalMode;
    memcpy(&packet.delta, interval, sizeof(packet.delta));
    memcpy(&packet.position, position, sizeof(packet.position));
    memcpy(&packet.start, start, sizeof(packet.start));
    memcpy(&packet.end, end, sizeof(packet.end));
    adjustWorldView(&packet, true);
}

//
// Received a Set or a VCRMode requiring us to adjust our state
// and possibly rethink everything.  This can result from a time
// control position change, delta change, direction change, etc.
//
void QedGroupControl::adjustWorldView(QmcTime::Packet *packet, bool vcrMode)
{
    my.delta = packet->delta;
    my.position = packet->position;
    my.realDelta = QedApp::timevalToSeconds(packet->delta);
    my.realPosition = QedApp::timevalToSeconds(packet->position);

    console->post("QedGroupControl::adjustWorldView: "
		  "delta=%.2f position=%.2f (%s) state=%s",
		my.realDelta, my.realPosition,
		QedApp::timeString(my.realPosition), timeState());

    QmcTime::State state = packet->state;
    if (isArchiveSource()) {
	if (packet->state == QmcTime::ForwardState)
	    adjustArchiveWorldViewForward(packet, vcrMode);
	else if (packet->state == QmcTime::BackwardState)
	    adjustArchiveWorldViewBackward(packet, vcrMode);
	else
	    adjustArchiveWorldViewStopped(packet, vcrMode);
    }
    else if (state != QmcTime::StoppedState)
	adjustLiveWorldViewForward(packet);
    else
	adjustLiveWorldViewStopped(packet);
}

void QedGroupControl::adjustLiveWorldViewStopped(QmcTime::Packet *packet)
{
    if (isActive(packet)) {
	newButtonState(packet->state, packet->mode, isRecording(packet));
	updateTimeButton();
    }
}

void QedGroupControl::adjustLiveWorldViewForward(QmcTime::Packet *packet)
{
    console->post("QedGroupControl::adjustLiveWorldViewForward");

    if (isActive(packet))
	newButtonState(packet->state, packet->mode, isRecording(packet));
}

void QedGroupControl::adjustArchiveWorldViewForward(QmcTime::Packet *packet, bool setup)
{
    console->post("QedGroupControl::adjustArchiveWorldViewForward");

    if (setup)
	packet->state = QmcTime::StoppedState;
    if (isActive(packet))
	newButtonState(packet->state, packet->mode, isRecording(packet));
}

void QedGroupControl::adjustArchiveWorldViewBackward(QmcTime::Packet *packet, bool setup)
{
    console->post("QedGroupControl::adjustArchiveWorldViewBackward");

    if (setup)
	packet->state = QmcTime::StoppedState;
    if (isActive(packet))
	newButtonState(packet->state, packet->mode, isRecording(packet));
}

void QedGroupControl::adjustArchiveWorldViewStopped(QmcTime::Packet *packet, bool needFetch)
{
    if (needFetch) {	// stopped, but VCR reposition event occurred
	adjustArchiveWorldViewForward(packet, needFetch);
	return;
    }
    my.timeState = StandbyState;
    packet->state = QmcTime::StoppedState;
    newButtonState(packet->state, packet->mode, isRecording(packet));
    updateTimeButton();
}

bool QedGroupControl::fuzzyTimeMatch(double a, double b, double tolerance)
{
    // a matches b if the difference is within 1% of the delta (tolerance)
    return (a == b ||
	   (b > a && a + tolerance > b) ||
	   (b < a && a - tolerance < b));
}

//
// Catch the situation where we get a larger than expected increase
// in position.  This happens when we restart after a stop in live
// mode (both with and without a change in the delta).
//
static bool sideStep(double n, double o, double interval)
{
    // tolerance set to 5% of the sample interval:
    return QedGroupControl::fuzzyTimeMatch(o + interval, n, interval/20.0) == false;
}

void QedGroupControl::step(QmcTime::Packet *packet)
{
    double stepPosition = QedApp::timevalToSeconds(packet->position);

    console->post(QedApp::DebugProtocol,
	"GroupControl::step: stepping to time %.2f, delta=%.2f, state=%s",
	stepPosition, my.realDelta, timeState());

    if ((packet->source == QmcTime::ArchiveSource &&
	((packet->state == QmcTime::ForwardState &&
		my.timeState != ForwardState) ||
	 (packet->state == QmcTime::BackwardState &&
		my.timeState != BackwardState))) ||
	 sideStep(stepPosition, my.realPosition, my.realDelta))
	return adjustWorldView(packet, false);

    my.pmtimeState = packet->state;
    my.position = packet->position;
    my.realPosition = stepPosition;

    adjustStep(packet);
    fetch();

    if (isActive(packet))
	newButtonState(packet->state, packet->mode, isRecording(packet));
}

void QedGroupControl::VCRMode(QmcTime::Packet *packet, bool dragMode)
{
    if (!dragMode)
	adjustWorldView(packet, true);
}

void QedGroupControl::setTimezone(QmcTime::Packet *packet, char *tz)
{
    console->post(QedApp::DebugProtocol, "GroupControl::setTimezone %s", tz);
    useTZ(QString(tz));
    (void)packet;
}

void QedGroupControl::newButtonState(QmcTime::State s, QmcTime::Mode m, bool record)
{
    if (isArchiveSource() == false) {
	if (s == QmcTime::StoppedState)
	    my.buttonState = record ?
		QedTimeButton::StoppedRecord : QedTimeButton::StoppedLive;
	else
	    my.buttonState = record ?
		QedTimeButton::ForwardRecord : QedTimeButton::ForwardLive;
    }
    else if (m == QmcTime::StepMode) {
	if (s == QmcTime::ForwardState)
	    my.buttonState = QedTimeButton::StepForwardArchive;
	else if (s == QmcTime::BackwardState)
	    my.buttonState = QedTimeButton::StepBackwardArchive;
	else
	    my.buttonState = QedTimeButton::StoppedArchive;
    }
    else if (m == QmcTime::FastMode) {
	if (s == QmcTime::ForwardState)
	    my.buttonState = QedTimeButton::FastForwardArchive;
	else if (s == QmcTime::BackwardState)
	    my.buttonState = QedTimeButton::FastBackwardArchive;
	else
	    my.buttonState = QedTimeButton::StoppedArchive;
    }
    else if (s == QmcTime::ForwardState)
	my.buttonState = QedTimeButton::ForwardArchive;
    else if (s == QmcTime::BackwardState)
	my.buttonState = QedTimeButton::BackwardArchive;
    else
	my.buttonState = QedTimeButton::StoppedArchive;
}
