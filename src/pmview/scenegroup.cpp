/*
 * Copyright (c) 2009, Aconex.  All Rights Reserved.
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
#include "main.h"
#include "pmview.h"
#include "console.h"
#include "scenegroup.h"
#include "timecontrol.h"

SceneGroup::SceneGroup() : GroupControl()
{
}

SceneGroup::~SceneGroup()
{
}

void SceneGroup::init(struct timeval *interval, struct timeval *position)
{
    GroupControl::init(interval, position);
}

bool SceneGroup::isArchiveSource(void)
{
    // Note: We purposefully are not using QmcGroup::mode() here, as we
    // may not have initialised any contexts yet.  In such a case, live
    // mode is always returned (default, from the QmcGroup constructor).

    return this == archiveGroup;
}

bool SceneGroup::isActive(PmTime::Packet *packet)
{
    return (((activeGroup == archiveGroup) &&
	     (packet->source == PmTime::ArchiveSource)) ||
	    ((activeGroup == liveGroup) && 
	     (packet->source == PmTime::HostSource)));
}

bool SceneGroup::isRecording(PmTime::Packet *packet)
{
    (void)packet;
    return pmview->isViewRecording();
}

void SceneGroup::setButtonState(TimeButton::State state)
{
    pmview->setButtonState(state);
}

void SceneGroup::updateTimeAxis(void)
{
    QString tz, otz, unused;

    if (numContexts() > 0 || isArchiveSource() == false) {
	if (numContexts() > 0)
	    defaultTZ(unused, otz);
	else
	    otz = QmcSource::localHost;
	tz = otz;
	pmview->setDateLabel((int)timePosition(), tz);
    } else {
	pmview->setDateLabel(tr("[No open archives]"));
    }

    if (console->logLevel(App::DebugProtocol)) {
	console->post(App::DebugProtocol,
		"SceneGroup::updateTimeAxis: tz=%s",
		(const char *)tz.toAscii());
    }
}

void SceneGroup::updateTimeButton(void)
{
    pmview->setButtonState(buttonState());
}

void SceneGroup::setupWorldView()
{
    activeGroup->GroupControl::setupWorldView(
			pmtime->archiveInterval(), pmtime->archivePosition(),
			pmtime->archiveStart(), pmtime->archiveEnd());
}

void SceneGroup::adjustLiveWorldViewForward(PmTime::Packet *packet)
{
    double position = timePosition();

    console->post("Fetching data at %s", App::timeString(position));
    fetch();

    setTimeState(packet->state == PmTime::StoppedState ?
			StandbyState : ForwardState);

    GroupControl::adjustLiveWorldViewForward(packet);
    pmview->render(PmView::inventor, 0);
}

void SceneGroup::adjustArchiveWorldViewForward(PmTime::Packet *packet, bool setup)
{
    console->post("SceneGroup::adjustArchiveWorldViewForward");
    setTimeState(ForwardState);

    int setmode = PM_MODE_INTERP;
    int delta = packet->delta.tv_sec;
    if (packet->delta.tv_usec == 0) {
	setmode |= PM_XTB_SET(PM_TIME_SEC);
    } else {
	delta = delta * 1000 + packet->delta.tv_usec / 1000;
	setmode |= PM_XTB_SET(PM_TIME_MSEC);
    }

    struct timeval timeval;
    double position = timePosition();
    App::timevalFromSeconds(timePosition(), &timeval);
    setArchiveMode(setmode, &timeval, delta);
    console->post("Fetching data at %s", App::timeString(position));
    fetch();

    GroupControl::adjustArchiveWorldViewForward(packet, setup);
    pmview->render(PmView::inventor, 0);
}

void SceneGroup::adjustArchiveWorldViewBackward(PmTime::Packet *packet, bool setup)
{
    console->post("SceneGroup::adjustArchiveWorldViewBackward");
    setTimeState(BackwardState);

    int setmode = PM_MODE_INTERP;
    int delta = packet->delta.tv_sec;
    if (packet->delta.tv_usec == 0) {
	setmode |= PM_XTB_SET(PM_TIME_SEC);
    } else {
	delta = delta * 1000 + packet->delta.tv_usec / 1000;
	setmode |= PM_XTB_SET(PM_TIME_MSEC);
    }

    struct timeval timeval;
    double position = timePosition();
    App::timevalFromSeconds(timePosition(), &timeval);
    setArchiveMode(setmode, &timeval, delta);
    console->post("Fetching data at %s", App::timeString(position));
    fetch();

    GroupControl::adjustArchiveWorldViewBackward(packet, setup);
    pmview->render(PmView::inventor, 0);
}

//
// Fetch all metric values across all scenes, and update the status bar.
//
void SceneGroup::adjustStep(PmTime::Packet *packet)
{
    (void)packet;	// no-op in pmview
}

void SceneGroup::step(PmTime::Packet *packet)
{
    GroupControl::step(packet);
    pmview->render(PmView::inventor, 0);
}

void SceneGroup::setTimezone(PmTime::Packet *packet, char *tz)
{
    GroupControl::setTimezone(packet, tz);
    if (isActive(packet))
	updateTimeAxis();
}
