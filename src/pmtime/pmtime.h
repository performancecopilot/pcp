/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 2006-2009, Aconex.  All Rights Reserved.
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
#ifndef PMTIME_H
#define PMTIME_H

#include <QMainWindow>
#include <qmc_time.h>

class PmTime : public QMainWindow, public QmcTime
{
    Q_OBJECT

public:
    typedef enum {
	DebugApp	= 0x1,
	DebugProtocol 	= 0x2,
    } DebugOptions;

public:
    PmTime();
    virtual void popup(bool hello_popetts);

public slots:
    virtual void helpManual();
    virtual void helpAbout();
    virtual void helpAboutQt();
    virtual void helpSeeAlso();
    virtual void whatsThis();
    virtual void hideWindow();
    virtual void showConsole();

protected:
    virtual void closeEvent(QCloseEvent * ce);
};

#endif	// PMTIME_H
