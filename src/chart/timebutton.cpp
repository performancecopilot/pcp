/*
 * Copyright (c) 2007, Nathan Scott.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Nathan Scott, nathans At debian DoT org
 */

#include "timebutton.h"

TimeButton::TimeButton(QWidget *parent, const char *name)
	: QPushButton(parent, name)
{
    setFocusPolicy(QWidget::NoFocus);
    playlive_pixmap = QPixmap::fromMimeSource("play_live.png");
    stoplive_pixmap = QPixmap::fromMimeSource("stop_live.png");
    playrecord_pixmap = QPixmap::fromMimeSource("play_record.png");
    stoprecord_pixmap = QPixmap::fromMimeSource("stop_record.png");
    playarchive_pixmap = QPixmap::fromMimeSource("play_archive.png");
    stoparchive_pixmap = QPixmap::fromMimeSource("stop_archive.png");
    backarchive_pixmap = QPixmap::fromMimeSource("back_archive.png");
    stepfwdarchive_pixmap = QPixmap::fromMimeSource("stepfwd_archive.png");
    stepbackarchive_pixmap = QPixmap::fromMimeSource("stepback_archive.png");
    fastfwdarchive_pixmap = QPixmap::fromMimeSource("fastfwd_archive.png");
    fastbackarchive_pixmap = QPixmap::fromMimeSource("fastback_archive.png");
}

void TimeButton::setButtonState(enum TimeButtonState newstate)
{
    switch(newstate) {
    case BUTTON_PLAYLIVE:
	setPixmap(playlive_pixmap);
	break;
    case BUTTON_STOPLIVE:
	setPixmap(stoplive_pixmap);
	break;
    case BUTTON_PLAYRECORD:
	setPixmap(playrecord_pixmap);
	break;
    case BUTTON_STOPRECORD:
	setPixmap(stoprecord_pixmap);
	break;
    case BUTTON_PLAYARCHIVE:
	setPixmap(playarchive_pixmap);
	break;
    case BUTTON_STOPARCHIVE:
	setPixmap(stoparchive_pixmap);
	break;
    case BUTTON_BACKARCHIVE:
	setPixmap(backarchive_pixmap);
	break;
    case BUTTON_STEPFWDARCHIVE:
	setPixmap(stepfwdarchive_pixmap);
	break;
    case BUTTON_STEPBACKARCHIVE:
	setPixmap(stepbackarchive_pixmap);
	break;
    case BUTTON_FASTFWDARCHIVE:
	setPixmap(fastfwdarchive_pixmap);
	break;
    case BUTTON_FASTBACKARCHIVE:
	setPixmap(fastbackarchive_pixmap);
	break;
    default:
	abort();
    }
}
