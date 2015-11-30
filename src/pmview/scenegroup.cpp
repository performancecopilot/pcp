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
#include "scenegroup.h"
#include "qed_console.h"
#include "qed_timecontrol.h"

SceneGroup::SceneGroup() : QedGroupControl()
{
}

SceneGroup::~SceneGroup()
{
}

void SceneGroup::init(struct timeval *interval, struct timeval *position)
{
    QedGroupControl::init(interval, position);
}

bool SceneGroup::isArchiveSource(void)
{
    // Note: We purposefully are not using QmcGroup::mode() here, as we
    // may not have initialised any contexts yet.  In such a case, live
    // mode is always returned (default, from the QmcGroup constructor).

    return this == archiveGroup;
}

bool SceneGroup::isActive(QmcTime::Packet *packet)
{
    return (((activeGroup == archiveGroup) &&
	     (packet->source == QmcTime::ArchiveSource)) ||
	    ((activeGroup == liveGroup) && 
	     (packet->source == QmcTime::HostSource)));
}

bool SceneGroup::isRecording(QmcTime::Packet *packet)
{
    (void)packet;
    return pmview->isViewRecording();
}

void SceneGroup::setButtonState(QedTimeButton::State state)
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

    if (console->logLevel(QedApp::DebugProtocol)) {
	console->post(QedApp::DebugProtocol,
		"SceneGroup::updateTimeAxis: tz=%s",
		(const char *)tz.toLatin1());
    }
}

void SceneGroup::updateTimeButton(void)
{
    pmview->setButtonState(buttonState());
}

void SceneGroup::setupWorldView()
{
    activeGroup->QedGroupControl::setupWorldView(
			pmtime->archiveInterval(), pmtime->archivePosition(),
			pmtime->archiveStart(), pmtime->archiveEnd());
}

void SceneGroup::adjustLiveWorldViewForward(QmcTime::Packet *packet)
{
    double position = timePosition();

    console->post("Fetching data at %s", QedApp::timeString(position));
    fetch();

    setTimeState(packet->state == QmcTime::StoppedState ?
			StandbyState : ForwardState);

    QedGroupControl::adjustLiveWorldViewForward(packet);
    pmview->render(PmView::inventor, 0);
}

void SceneGroup::adjustArchiveWorldViewForward(QmcTime::Packet *packet, bool setup)
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
    QedApp::timevalFromSeconds(timePosition(), &timeval);
    setArchiveMode(setmode, &timeval, delta);
    console->post("Fetching data at %s", QedApp::timeString(position));
    fetch();

    QedGroupControl::adjustArchiveWorldViewForward(packet, setup);
    pmview->render(PmView::inventor, 0);
}

void SceneGroup::adjustArchiveWorldViewBackward(QmcTime::Packet *packet, bool setup)
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
    QedApp::timevalFromSeconds(timePosition(), &timeval);
    setArchiveMode(setmode, &timeval, delta);
    console->post("Fetching data at %s", QedApp::timeString(position));
    fetch();

    QedGroupControl::adjustArchiveWorldViewBackward(packet, setup);
    pmview->render(PmView::inventor, 0);
}

//
// Fetch all metric values across all scenes, and update the status bar.
//
void SceneGroup::adjustStep(QmcTime::Packet *packet)
{
    (void)packet;	// no-op in pmview
}

void SceneGroup::step(QmcTime::Packet *packet)
{
    QedGroupControl::step(packet);
    pmview->render(PmView::inventor, 0);
}

void SceneGroup::setTimezone(QmcTime::Packet *packet, char *tz)
{
    QedGroupControl::setTimezone(packet, tz);
    if (isActive(packet))
	updateTimeAxis();
}
