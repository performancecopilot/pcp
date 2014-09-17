/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#include "colorscheme.h"
#include "qed_console.h"
#include "timecontrol.h"
#include "qed_fileiconprovider.h"
#include "pmchart.h"

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

	// Default Colors
	QColor chartBackground;
	QString chartBackgroundName;
	bool chartBackgroundModified;
	QColor chartHighlight;
	QString chartHighlightName;
	bool chartHighlightModified;
	// Color Schemes
	ColorScheme defaultScheme;
	bool defaultSchemeModified;
	QList<ColorScheme> colorSchemes;
	bool colorSchemesModified;

	// Toolbar
	int initialToolbar;
	bool initialToolbarModified;
	int nativeToolbar;
	bool nativeToolbarModified;
	int toolbarLocation;
	int toolbarLocationModified;
	QStringList toolbarActions;
	bool toolbarActionsModified;

	// Font
	QString fontFamily;
	bool fontFamilyModified;
	QString fontStyle;
	bool fontStyleModified;
	int fontSize;
	bool fontSizeModified;

	// Saved Hosts
	QStringList savedHosts;
	bool savedHostsModified;
} Settings;

extern Settings globalSettings;
extern void writeSettings();
extern QColor nextColor(QString, int *);

extern int Cflag;
extern int Lflag;
extern int Wflag;
extern char *outfile;
extern char *outgeometry;

extern QFont *globalFont;

extern GroupControl *activeGroup;
extern GroupControl *liveGroup;
extern GroupControl *archiveGroup;

class PmChart;
extern PmChart *pmchart;

class TimeControl;
extern TimeControl *pmtime;

extern double torange(struct timeval, int);
extern char *timeString(double);
extern char *timeHiResString(double);
extern void nomem(void);

/*
 * number of Y pixels to move the time axis up when exporting to
 * an image or printing
 */
#define TIMEAXISFUDGE 0

#endif	// MAIN_H
