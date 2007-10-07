/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

#include "tab.h"
#include "console.h"
#include "timecontrol.h"
#include "fileiconprovider.h"
#include "kmchart.h"
#include "version.h"

typedef struct {
	// Sampling
	double chartDelta;
	bool chartDeltaModified;
	double loggerDelta;
	bool loggerDeltaModified;
	int sampleHistory;
	bool sampleHistoryModified;
	int visibleHistory;
	bool visibleHistoryModified;

	// Colors
	QList<QColor> defaultColors;
	QStringList defaultColorNames;
	bool defaultColorsModified;
	QColor chartBackground;
	QString chartBackgroundName;
	bool chartBackgroundModified;
	QColor chartHighlight;
	QString chartHighlightName;
	bool chartHighlightModified;

	// Toolbar
	int initialToolbar;
	bool initialToolbarModified;
	int toolbarLocation;
	int toolbarLocationModified;
	QStringList toolbarActions;
	bool toolbarActionsModified;
} Settings;

extern Settings globalSettings;
extern void readSettings();
extern void writeSettings();

extern int Cflag;
extern QFont globalFont;

extern Tab *activeTab;
extern QList<Tab*> tabs;

extern QmcGroup	*activeGroup;
extern QmcGroup *liveGroup;
extern QmcGroup *archiveGroup;

class KmChart;
extern KmChart *kmchart;

class TimeControl;
extern TimeControl *kmtime;

extern double tosec(struct timeval);
extern double torange(struct timeval, int);
extern void fromsec(double, struct timeval *);
extern char *timeString(double);

extern void nomem(void);

#endif	// MAIN_H
