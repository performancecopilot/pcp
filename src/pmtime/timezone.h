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
#ifndef TIMEZONE_H
#define TIMEZONE_H

#include <string.h>
#include <QtGui/QAction>

class TimeZone
{
public:
    TimeZone(char *name, char *label, QAction *action, int handle)
    {
	my.name = name;
	my.label = label;
	my.action = action;
	my.handle = handle;
    }

    ~TimeZone()
    {
	if (my.name) free(my.name);
	if (my.label) free(my.label);
	if (my.action) delete my.action;
    }

    char *tz(void) { return my.name; }
    char *tzlabel(void) { return my.label; }
    int handle(void) { return my.handle; }
    QAction *action(void) { return my.action; }

private:
    struct {
	char *name;
	char *label;
	int handle;
	QAction *action;
    } my;
};

#endif	// TIMEZONE_H
