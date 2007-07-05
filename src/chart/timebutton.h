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
#ifndef TIMEBUTTON_H
#define TIMEBUTTON_H

#include <qpushbutton.h>

enum TimeButtonState {
    BUTTON_TIMELESS = 1,
    BUTTON_PLAYLIVE = 2,
    BUTTON_STOPLIVE = 3,
    BUTTON_PLAYRECORD = 4,
    BUTTON_STOPRECORD = 5,
    BUTTON_PLAYARCHIVE = 6,
    BUTTON_STOPARCHIVE = 7,
    BUTTON_BACKARCHIVE = 8,
    BUTTON_STEPFWDARCHIVE = 9,
    BUTTON_STEPBACKARCHIVE = 10,
    BUTTON_FASTFWDARCHIVE = 11,
    BUTTON_FASTBACKARCHIVE = 12,
};

class TimeButton : public QPushButton
{
    Q_OBJECT
public:
    TimeButton(QWidget * = 0, const char *name = 0);
    void setButtonState(enum TimeButtonState newstate);

private:
    QPixmap playlive_pixmap;
    QPixmap stoplive_pixmap;
    QPixmap playrecord_pixmap;
    QPixmap stoprecord_pixmap;
    QPixmap playarchive_pixmap;
    QPixmap stoparchive_pixmap;
    QPixmap backarchive_pixmap;
    QPixmap stepfwdarchive_pixmap;
    QPixmap stepbackarchive_pixmap;
    QPixmap fastfwdarchive_pixmap;
    QPixmap fastbackarchive_pixmap;
};

#endif	/* TIMEBUTTON_H */
