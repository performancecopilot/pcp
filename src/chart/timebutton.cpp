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
#include "timebutton.h"
#include "console.h"
#include "pmchart.h"

TimeButton::TimeButton(QWidget *parent) : QToolButton(parent)
{
    my.state = Timeless;
    setIconSize(QSize(52, 52));
    setFocusPolicy(Qt::NoFocus);
    console->post(PmChart::DebugUi, "Loading resource :/play_live.png");
    my.forwardLiveIcon = QIcon(":/play_live.png");
    console->post(PmChart::DebugUi, "Loading resource :/stop_live.png");
    my.stoppedLiveIcon = QIcon(":/stop_live.png");
    console->post(PmChart::DebugUi, "Loading resource :/play_record.png");
    my.forwardRecordIcon = QIcon(":/play_record.png");
    console->post(PmChart::DebugUi, "Loading resource :/stop_record.png");
    my.stoppedRecordIcon = QIcon(":/stop_record.png");
    console->post(PmChart::DebugUi, "Loading resource :/play_archive.png");
    my.forwardArchiveIcon = QIcon(":/play_archive.png");
    console->post(PmChart::DebugUi, "Loading resource :/stop_archive.png");
    my.stoppedArchiveIcon = QIcon(":/stop_archive.png");
    console->post(PmChart::DebugUi, "Loading resource :/back_archive.png");
    my.backwardArchiveIcon = QIcon(":/back_archive.png");
    console->post(PmChart::DebugUi, "Loading resource :/stepfwd_archive.png");
    my.stepForwardArchiveIcon = QIcon(":/stepfwd_archive.png");
    console->post(PmChart::DebugUi, "Loading resource :/stepback_archive.png");
    my.stepBackwardArchiveIcon = QIcon(":/stepback_archive.png");
    console->post(PmChart::DebugUi, "Loading resource :/fastfwd_archive.png");
    my.fastForwardArchiveIcon = QIcon(":/fastfwd_archive.png");
    console->post(PmChart::DebugUi, "Loading resource :/fastback_archive.png");
    my.fastBackwardArchiveIcon = QIcon(":/fastback_archive.png");
    console->post(PmChart::DebugUi, "Time button resources loaded");
}

void TimeButton::setButtonState(TimeButton::State state)
{
    if (my.state == state)
	return;
    switch (state) {
    case TimeButton::ForwardLive:
	setIcon(my.forwardLiveIcon);
	break;
    case TimeButton::StoppedLive:
	setIcon(my.stoppedLiveIcon);
	break;
    case TimeButton::ForwardRecord:
	setIcon(my.forwardRecordIcon);
	break;
    case TimeButton::StoppedRecord:
	setIcon(my.stoppedRecordIcon);
	break;
    case TimeButton::ForwardArchive:
	setIcon(my.forwardArchiveIcon);
	break;
    case TimeButton::StoppedArchive:
	setIcon(my.stoppedArchiveIcon);
	break;
    case TimeButton::BackwardArchive:
	setIcon(my.backwardArchiveIcon);
	break;
    case TimeButton::StepForwardArchive:
	setIcon(my.stepForwardArchiveIcon);
	break;
    case TimeButton::StepBackwardArchive:
	setIcon(my.stepBackwardArchiveIcon);
	break;
    case TimeButton::FastForwardArchive:
	setIcon(my.fastForwardArchiveIcon);
	break;
    case TimeButton::FastBackwardArchive:
	setIcon(my.fastBackwardArchiveIcon);
	break;
    default:
	abort();
    }
    my.state = state;
}
