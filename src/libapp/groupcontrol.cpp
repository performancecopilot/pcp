/*
 * Copyright (c) 2007-2009, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#include "groupcontrol.h"
#include "console.h"
#include "app.h"

GroupControl::GroupControl()
{
    my.realDelta = 0;
    my.realPosition = 0;
    my.timeState = StartState;
    my.buttonState = TimeButton::Timeless;
    my.pmtimeState = PmTime::StoppedState;
    memset(&my.delta, 0, sizeof(struct timeval));
    memset(&my.position, 0, sizeof(struct timeval));
}

void GroupControl::init(struct timeval *interval, struct timeval *position)
{
    if (isArchiveSource()) {
	my.pmtimeState = PmTime::StoppedState;
	my.buttonState = TimeButton::StoppedArchive;
    }
    else {
	my.pmtimeState = PmTime::ForwardState;
	my.buttonState = TimeButton::ForwardLive;
    }
    my.delta = *interval;
    my.position = *position;
    my.realDelta = App::timevalToSeconds(*interval);
    my.realPosition = App::timevalToSeconds(*position);
}

PmTime::State GroupControl::pmtimeState(void)
{
    return my.pmtimeState;
}

char *GroupControl::timeState()
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
void GroupControl::setupWorldView(struct timeval *interval,
	struct timeval *position, struct timeval *start, struct timeval *end)
{
    if (isArchiveSource() == false)
	return;

    PmTime::Packet packet;
    packet.source = PmTime::ArchiveSource;
    packet.state = PmTime::ForwardState;
    packet.mode = PmTime::NormalMode;
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
void GroupControl::adjustWorldView(PmTime::Packet *packet, bool vcrMode)
{
    my.delta = packet->delta;
    my.position = packet->position;
    my.realDelta = App::timevalToSeconds(packet->delta);
    my.realPosition = App::timevalToSeconds(packet->position);

    console->post("GroupControl::adjustWorldView: "
		  "delta=%.2f position=%.2f (%s) state=%s",
		my.realDelta, my.realPosition,
		App::timeString(my.realPosition), timeState());

    PmTime::State state = packet->state;
    if (isArchiveSource()) {
	if (packet->state == PmTime::ForwardState)
	    adjustArchiveWorldViewForward(packet, vcrMode);
	else if (packet->state == PmTime::BackwardState)
	    adjustArchiveWorldViewBackward(packet, vcrMode);
	else
	    adjustArchiveWorldViewStopped(packet, vcrMode);
    }
    else if (state != PmTime::StoppedState)
	adjustLiveWorldViewForward(packet);
    else
	adjustLiveWorldViewStopped(packet);
}

void GroupControl::adjustLiveWorldViewStopped(PmTime::Packet *packet)
{
    if (isActive(packet)) {
	newButtonState(packet->state, packet->mode, isRecording(packet));
	updateTimeButton();
    }
}

void GroupControl::adjustLiveWorldViewForward(PmTime::Packet *packet)
{
    console->post("GroupControl::adjustLiveWorldViewForward");

    if (isActive(packet))
	newButtonState(packet->state, packet->mode, isRecording(packet));
}

void GroupControl::adjustArchiveWorldViewForward(PmTime::Packet *packet, bool setup)
{
    console->post("GroupControl::adjustArchiveWorldViewForward");

    if (setup)
	packet->state = PmTime::StoppedState;
    if (isActive(packet))
	newButtonState(packet->state, packet->mode, isRecording(packet));
}

void GroupControl::adjustArchiveWorldViewBackward(PmTime::Packet *packet, bool setup)
{
    console->post("GroupControl::adjustArchiveWorldViewBackward");

    if (setup)
	packet->state = PmTime::StoppedState;
    if (isActive(packet))
	newButtonState(packet->state, packet->mode, isRecording(packet));
}

void GroupControl::adjustArchiveWorldViewStopped(PmTime::Packet *packet, bool needFetch)
{
    if (needFetch) {	// stopped, but VCR reposition event occurred
	adjustArchiveWorldViewForward(packet, needFetch);
	return;
    }
    my.timeState = StandbyState;
    packet->state = PmTime::StoppedState;
    newButtonState(packet->state, packet->mode, isRecording(packet));
    updateTimeButton();
}

bool GroupControl::fuzzyTimeMatch(double a, double b, double tolerance)
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
    return GroupControl::fuzzyTimeMatch(o + interval, n, interval/20.0) == false;
}

void GroupControl::step(PmTime::Packet *packet)
{
    double stepPosition = App::timevalToSeconds(packet->position);

    console->post(App::DebugProtocol,
	"GroupControl::step: stepping to time %.2f, delta=%.2f, state=%s",
	stepPosition, my.realDelta, timeState());

    if ((packet->source == PmTime::ArchiveSource &&
	((packet->state == PmTime::ForwardState &&
		my.timeState != ForwardState) ||
	 (packet->state == PmTime::BackwardState &&
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

void GroupControl::VCRMode(PmTime::Packet *packet, bool dragMode)
{
    if (!dragMode)
	adjustWorldView(packet, true);
}

void GroupControl::setTimezone(PmTime::Packet *packet, char *tz)
{
    console->post(App::DebugProtocol, "GroupControl::setTimezone %s", tz);
    useTZ(QString(tz));
    (void)packet;
}

void GroupControl::newButtonState(PmTime::State s, PmTime::Mode m, bool record)
{
    if (isArchiveSource() == false) {
	if (s == PmTime::StoppedState)
	    my.buttonState = record ?
			TimeButton::StoppedRecord : TimeButton::StoppedLive;
	else
	    my.buttonState = record ?
			TimeButton::ForwardRecord : TimeButton::ForwardLive;
    }
    else if (m == PmTime::StepMode) {
	if (s == PmTime::ForwardState)
	    my.buttonState = TimeButton::StepForwardArchive;
	else if (s == PmTime::BackwardState)
	    my.buttonState = TimeButton::StepBackwardArchive;
	else
	    my.buttonState = TimeButton::StoppedArchive;
    }
    else if (m == PmTime::FastMode) {
	if (s == PmTime::ForwardState)
	    my.buttonState = TimeButton::FastForwardArchive;
	else if (s == PmTime::BackwardState)
	    my.buttonState = TimeButton::FastBackwardArchive;
	else
	    my.buttonState = TimeButton::StoppedArchive;
    }
    else if (s == PmTime::ForwardState)
	my.buttonState = TimeButton::ForwardArchive;
    else if (s == PmTime::BackwardState)
	my.buttonState = TimeButton::BackwardArchive;
    else
	my.buttonState = TimeButton::StoppedArchive;
}
