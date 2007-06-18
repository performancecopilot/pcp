/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 *                      Nathan Scott, nathans At debian DoT org
 */
#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <qvbox.h>
#include <qhbox.h>

#include "tab.h"
#include "view.h"
#include "timecontrol.h"
#include "kmchart.h"
#include "source.h"

#define VERSION		"0.5.0"
#define min(a,b)	((a)<(b)?(a):(b))
#define max(a,b)	((a)>(b)?(a):(b))

#define DEFAULT_SAMPLE_INTERVAL	2 /* seconds */

#define DEFAULT_VISIBLE_POINTS	24
#define DEFAULT_SAMPLE_POINTS	24
#define MAXIMUM_POINTS		300

typedef struct {
	// Samples
	int		sampleHistory;
	bool		sampleHistoryModified;
	int		visibleHistory;
	bool		visibleHistoryModified;
	// Colors
	QValueList<QColor> defaultColors;
	QStringList	defaultColorNames;
	bool		defaultColorsModified;
	QColor		chartBackground;
	QString		chartBackgroundName;
	bool		chartBackgroundModified;
	QColor		chartHighlight;
	QString		chartHighlightName;
	bool		chartHighlightModified;
	// Styles
	QString		styleName;
	QStyle		*style;
	QStyle		*defaultStyle;
	bool		styleModified;
} Settings;

extern Settings		settings;
extern void		readSettings();
extern void		writeSettings();

extern int		Cflag;

extern Tab		*activeTab;
extern Tab		**tabs;
extern int		ntabs;

extern Source		*activeSources;
extern Source		*liveSources;
extern Source		*archiveSources;

extern PMC_Group	*activeGroup;
extern PMC_Group	*liveGroup;
extern PMC_Group	*archiveGroup;

class KmChart;
class TimeControl;
extern KmChart		*kmchart;
extern TimeControl	*kmtime;

typedef enum { Msec, Sec, Min, Hour, Day, Week } delta_units;

extern double secondsFromTV(struct timeval *tv);
extern double secondsToUnits(double value, delta_units units);

extern double tosec(struct timeval);
extern double torange(struct timeval, int);
extern void fromsec(double, struct timeval *);
extern char *timestring(double);

extern void nomem(void);

#endif	/* MAIN_H */
