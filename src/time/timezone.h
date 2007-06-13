/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Nathan Scott.  All Rights Reserved.
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
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 *                      Nathan Scott, nathans At debian DoT org
 */
#ifndef TIMEZONE_H
#define TIMEZONE_H

#include <string.h>
#include <qaction.h>

class TimeZone
{
public:
    TimeZone(char *tz, char *label, QAction *action, int handle)
    {
	_tz = tz;
	_tzlabel = label;
	_tzaction = action;
	_tzhandle = handle;
    }

    ~TimeZone()
    {
	if (_tz) free(_tz);
	if (_tzlabel) free(_tzlabel);
	if (_tzaction) delete _tzaction;
    }

    char *tz(void) { return _tz; }
    char *tzlabel(void) { return _tzlabel; }
    QAction *action(void) { return _tzaction; }
    int handle(void) { return _tzhandle; }

private:
    char *_tz;
    char *_tzlabel;
    QAction *_tzaction;
    int _tzhandle;
};

#endif /* TIMEZONE_H */
