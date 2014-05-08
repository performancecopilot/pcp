/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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
#ifndef CONSOLE_H
#define CONSOLE_H

#include "ui_console.h"
#include "pmchart.h"

class Console : public QDialog, public Ui::Console
{
    Q_OBJECT

public:
    Console(struct timeval);
    void post(const char *p, ...);
    void post(int level, const char *p, ...);
    bool logLevel(int level = PmChart::DebugApp);

private:
    struct {
	int level;
	double origin;
    } my;
};

extern Console *console;

#endif	// CONSOLE_H
