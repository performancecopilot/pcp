/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2013, Red Hat Inc.
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
#include <QtCore/QString>
#include <QtGui/QMessageBox>
#include <QtGui/QColor>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include "main.h"
#include "openviewdialog.h"
#include "saveviewdialog.h"

/*
 * View file parsing routines and global variables follow.  These are
 * currently not part of any class, and may need to be reworked a bit
 * to use more portable (Qt) file IO interfaces (for a Windows port).
 */
static char	_fname[MAXPATHLEN];
static uint	_line;
static uint	_errors;
static uint	_width;
static uint	_height;
static uint	_points;
static uint	_xpos;
static uint	_ypos;

#define MAXWDSZ 256

// parser states
#define S_BEGIN		0
#define S_VERSION	1
#define S_TOP		2
#define S_CHART		3
#define S_PLOT		4

// config file styles (mode)
#define M_UNKNOWN	0
#define M_PMCHART	1
#define M_KMCHART	2

// global attributes
#define G_WIDTH		1
#define G_HEIGHT	2
#define G_POINTS	3
#define G_XPOS		4
#define G_YPOS		5

// host mode
#define H_DYNAMIC	1
#define H_LITERAL	2

// error severity
#define E_INFO	0
#define E_CRIT	1
#define E_WARN	2

// version numbers we're willing and able to support
#define P1_1	101
#define P1_2	102
#define P2_0	200
#define P2_1	201
#define K1	1

// instance / matching / not-matching
#define IM_NONE		0
#define IM_INST		1
#define IM_MATCH	2
#define IM_NOT_MATCH	3

const char *_style[] = { "None", "Line", "Bar", "Stack", "Area", "Util" };
#define stylestr(x) _style[(int)x]

static void err(int severity, int do_where, QString msg)
{
    if (do_where) {
	QString	where = QString();
	where.sprintf("%s[%d] ", _fname, _line);
	msg.prepend(where);
    }
    if (Cflag) {
	if (severity == E_CRIT)
	    msg.prepend("Error: ");
	else if (severity == E_WARN)
	    msg.prepend("Warning: ");
	// else do nothing for E_INFO
	msg.append("\n");
	fflush(stderr);
	pmprintf("%s", (const char *)msg.toAscii());
	pmflush();
    }
    else {
	if (severity == E_CRIT)
	    QMessageBox::critical(pmchart, pmProgname,  msg);
	else if (severity == E_WARN)
	    QMessageBox::warning(pmchart, pmProgname,  msg);
	else
	    QMessageBox::information(pmchart, pmProgname,  msg);
    }
    _errors++;
}

static char *getwd(FILE *f)
{
    static char	buf[MAXWDSZ];
    static int	lastc = 0;
    char	*p;
    int		c;
    int		quote = 0;

    if ((char)lastc == '\n') {
eol:
	buf[0] = '\n';
	buf[1] = '\0';
	_line++;
	lastc = 0;
	goto done;
    }

    // skip to first non-white space
    p = buf;
    while ((c = fgetc(f)) != EOF) {
	if ((char)c == '\n')
	    goto eol;
	if (!isspace((char)c)) {
	    // got one
	    if ((char)c == '"') {
		quote = 1;
	    }
	    else {
		*p++ = c;
	    }
	    break;
	}
    }
    if (feof(f)) return NULL;

    for ( ; p < &buf[MAXWDSZ]; ) {
	if ((c = fgetc(f)) == EOF) break;
	if ((char)c == '\n') {
	    lastc = c;
	    break;
	}
	if (quote == 0 && isspace((char)c)) break;
	if (quote && (char)c == '"') break;
	*p++ = c;
    }

    if (p == &buf[MAXWDSZ]) {
	QString	msg = QString();
	p[-1] = '\0';
	msg.sprintf("Word truncated after %d characters!\n\"%20.20s ... %20.20s\"", (int)sizeof(buf)-1, buf, &p[-21]);
	err(E_CRIT, true, msg);
    }
    else
	*p = '\0';

done:
    if ((pmDebug & DBG_TRACE_APPL0) && (pmDebug & DBG_TRACE_APPL2)) {
	if (buf[0] == '\n')
	    fprintf(stderr, "openView getwd=EOL\n");
	else
	    fprintf(stderr, "openView getwd=\"%s\"\n", buf);
    }

    return buf;
}

static void eol(FILE *f)
{
    char	*w;

    while ((w = getwd(f)) != NULL && w[0] != '\n') {
	QString	msg = QString("Syntax error: unexpected word \"");
	msg.append(w);
	msg.append("\"");
	err(E_CRIT, true, msg);
    }
}

static void skip2eol(FILE *f)
{
    char	*w;

    while ((w = getwd(f)) != NULL && w[0] != '\n') {
	;
    }
}

static void xpect(const char *want, const char *got)
{
    QString     msg = QString("Syntax error: expecting \"");
    msg.append(want);
    msg.append("\", found ");
    if (got == NULL)
	msg.append("End-of-File");
    else if (got[0] == '\n')
	msg.append("End-of-Line");
    else {
	msg.append("\"");
	msg.append(got);
	msg.append("\"");
    }
    err(E_CRIT, true, msg);
}

static QColor colorSpec(QString colorName, int *sequence)
{
    if (colorName == "#-cycle")
	return nextColor("#-cycle", sequence);
    if (ColorScheme::lookupScheme(colorName) == true)
	return nextColor(colorName, sequence);
    QColor color = ColorScheme::colorSpec(colorName);
    if (!color.isValid()) {
	QString errmsg;
	errmsg.append("Invalid color name: ");
	errmsg.append(colorName);
	err(E_CRIT, true, errmsg);
	color = Qt::white;
    }
    return color;
}

void OpenViewDialog::globals(int *w, int *h, int *pts, int *x, int *y)
{
    // Note: we use global variables here so that all views specified
    // on the command line get input into these values (the maximum
    // observed value is always used), without clobbering each other.
    //
    *w = _width;
    *h = _height;
    *pts = _points;
    *x = _xpos;
    *y = _ypos;
}

bool OpenViewDialog::openView(const char *path)
{
    Tab			*tab;
    Chart		*cp = NULL;
    int			ct = pmchart->tabWidget()->currentIndex();
    int			m;
    ColorScheme		scheme;
    FILE		*f;
    int			is_popen = 0;
    char		*w;
    int			state = S_BEGIN;
    int			mode = M_UNKNOWN;
//  int			h_mode;
    int			version;
    QString		errmsg;
    QRegExp		regex;
    int			sep = __pmPathSeparator();
    int			sts = 0;

    if (strcmp(path, "-") == 0) {
	f = stdin;
	strcpy(_fname, "stdin");
    }
    else if (path[0] == '/') {
	strcpy(_fname, path);
	if ((f = fopen(_fname, "r")) == NULL)
	    goto noview;
    }
    else {
	QString homepath = QDir::toNativeSeparators(QDir::homePath());

	strcpy(_fname, path);
	if ((f = fopen(_fname, "r")) == NULL) {
	    // not found, start the great hunt
	    // try user's pmchart dir ...
	    snprintf(_fname, sizeof(_fname),
			"%s%c" ".pcp%c" "pmchart%c" "%s",
			(const char *)homepath.toAscii(), sep, sep, sep, path);
	    if ((f = fopen(_fname, "r")) == NULL) {
		// try system pmchart dir
		snprintf(_fname, sizeof(_fname),
			    "%s%c" "config%c" "pmchart%c" "%s",
			    pmGetConfig("PCP_VAR_DIR"), sep, sep, sep, path);
		if ((f = fopen(_fname, "r")) == NULL) {
		    // try user's kmchart dir
		    snprintf(_fname, sizeof(_fname),
				"%s%c" ".pcp%c" "kmchart%c" "%s",
				(const char *)homepath.toAscii(),
				sep, sep, sep, path);
		    if ((f = fopen(_fname, "r")) == NULL) {
			// try system kmchart dir
			snprintf(_fname, sizeof(_fname),
				    "%s%c" "config%c" "kmchart%c" "%s",
				    pmGetConfig("PCP_VAR_DIR"),
				    sep, sep, sep, path);
			if ((f = fopen(_fname, "r")) == NULL) {
			    snprintf(_fname, sizeof(_fname),
					"%s%c" "config%c" "pmchart%c" "%s",
					pmGetConfig("PCP_VAR_DIR"),
					sep, sep, sep, path);
			    goto noview;
			}
		    }
		}
	    }
	}
	// check for executable and popen() as needed
	//
	if (fgetc(f) == '#' && fgetc(f) == '!') {
	    char	cmd[MAXPATHLEN];
	    sprintf(cmd, "%s", _fname);
	    fclose(f);
	    if ((f = popen(cmd, "r")) == NULL)
		goto nopipe;
	    is_popen = 1;
	}
	else {
	    rewind(f);
	}
    }

    _line = 1;
    _errors = 0;
    console->post(PmChart::DebugView, "Load View: %s", _fname);

    while ((w = getwd(f)) != NULL) {
	if (state == S_BEGIN) {
	    // expect #pmchart
	    if (strcasecmp(w, "#pmchart") == 0)
		mode = M_PMCHART;
	    else if (strcasecmp(w, "#kmchart") == 0)
		mode = M_KMCHART;
	    else {
		xpect("#pmchart\" or \"#kmchart", w);
		goto abandon;
	    }
	    eol(f);
	    state = S_VERSION;
	    continue;
	}

	if (w[0] == '\n')
	    // skip empty lines and comments
	    continue;

	if (w[0] == '#') {
	    // and comments
	    skip2eol(f);
	    continue;
	}

	if (state == S_VERSION) {
	    // expect version X.X host [dynamic|static]
	    if (strcasecmp(w, "version") != 0) {
		xpect("version", w);
		goto abandon;
	    }
	    w = getwd(f);
	    if (w == NULL || w[0] == '\n') {
		xpect("<version number>", w);
		goto abandon;
	    }
	    version = 0;
	    if (mode == M_PMCHART) {
		if (strcmp(w, "2.1") == 0)
		    version = P2_1;
		else if (strcmp(w, "2.0") == 0)
		    version = P2_0;
		else if (strcmp(w, "1.1") == 0)
		    version = P1_1;
		else if (strcmp(w, "1.2") == 0)
		    version = P1_2;
	    }
	    else if (mode == M_KMCHART) {
		if (strcmp(w, "1") == 0)
		    version = K1;
	    }
	    if (version == 0) {
		xpect("<version number>", w);
		goto abandon;
	    }
	    w = getwd(f);
	    if (w == NULL || w[0] == '\n') {
		if (mode == M_KMCHART) {
		    // host [literal|dynamic] is optional for new pmchart
//		    h_mode = H_DYNAMIC;
		    state = S_TOP;
		    continue;
		}
		else {
		    xpect("host", w);
		    goto abandon;
		}
	    }
	    if (strcasecmp(w, "host") != 0) {
		xpect("host", w);
		goto abandon;
	    }
	    w = getwd(f);
	    if (w != NULL && strcasecmp(w, "literal") == 0) {
// NYI:		h_mode = H_LITERAL;
	    }
	    else if (w != NULL && strcasecmp(w, "dynamic") == 0) {
// NYI:		h_mode = H_DYNAMIC;
	    }
	    else {
		xpect("literal\" or \"dynamic", w);
		goto abandon;
	    }
	    eol(f);
	    state = S_TOP;
	}

	else if (state == S_TOP) {
	    if (strcasecmp(w, "chart") == 0) {
new_chart:
		char		*title = NULL;
		Chart::Style	style = Chart::NoStyle;
		int		autoscale = 1;
		char		*endnum;
		double		ymin = 0;
		double		ymax = 0;
		int		legend = 1;
		int		antialias = 1;

		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("title\" or \"style", w);
		    goto abort_chart;
		}
		if (strcasecmp(w, "title") == 0) {
		    // optional title "<title>"
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("<title>", w);
			goto abort_chart;
		    }
		    if ((title = strdup(w)) == NULL) nomem();
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("style", w);
			goto abort_chart;
		    }
		}
		if (strcasecmp(w, "style") == 0) {
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("<chart style>", w);
			goto abort_chart;
		    }
		    if (strcasecmp(w, "plot") == 0)
			style = Chart::LineStyle;
		    else if (strcasecmp(w, "line") == 0)
			style = Chart::LineStyle;
		    else if (strcasecmp(w, "bar") == 0)
			style = Chart::BarStyle;
		    else if (strcasecmp(w, "stacking") == 0)
			style = Chart::StackStyle;
		    else if (strcasecmp(w, "area") == 0)
			style = Chart::AreaStyle;
		    else if (strcasecmp(w, "utilization") == 0)
			style = Chart::UtilisationStyle;
		    else if (strcasecmp(w, "event") == 0)
			style = Chart::EventStyle;
		    else {
			xpect("<chart style>", w);
			goto abort_chart;
		    }
		}

		// down to the optional bits
		//	- scale
		//	- legend
		if ((w = getwd(f)) == NULL || w[0] == '\n')
		    goto done_chart;
		if (strcasecmp(w, "scale") == 0) {
		    // scale [from] ymin [to] ymax
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("from or <ymin>", w);
			goto abort_chart;
		    }
		    if (strcasecmp(w, "from") == 0) {
			if ((w = getwd(f)) == NULL || w[0] == '\n') {
			    xpect("<ymin>", w);
			    goto abort_chart;
			}
		    }
		    ymin = strtod(w, &endnum);
		    if (*endnum != '\0') {
			xpect("<ymin>", w);
			goto abort_chart;
		    }
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("to or <ymax>", w);
			goto abort_chart;
		    }
		    if (strcasecmp(w, "to") == 0) {
			if ((w = getwd(f)) == NULL || w[0] == '\n') {
			    xpect("<ymax>", w);
			    goto abort_chart;
			}
		    }
		    ymax = strtod(w, &endnum);
		    if (*endnum != '\0') {
			xpect("<ymax>", w);
			goto abort_chart;
		    }
		    autoscale = 0;
		    if ((w = getwd(f)) == NULL || w[0] == '\n')
			goto done_chart;
		}
		if (strcasecmp(w, "legend") == 0) {
		    // optional legend on|off
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("on\" or \"off", w);
			goto abort_chart;
		    }
		    if (strcasecmp(w, "on") == 0)
			legend = 1;
		    else if (strcasecmp(w, "off") == 0)
			legend = 0;
		    else {
			xpect("on\" or \"off", w);
			goto abort_chart;
		    }
		    if ((w = getwd(f)) == NULL || w[0] == '\n')
			goto done_chart;
		}
		if (strcasecmp(w, "antialiasing") == 0) {
		    // optional antialiasing on|off
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("on\" or \"off", w);
			goto abort_chart;
		    }
		    if (strcasecmp(w, "on") == 0)
			antialias = 1;
		    else if (strcasecmp(w, "off") == 0)
			antialias = 0;
		    else {
			xpect("on\" or \"off", w);
			goto abort_chart;
		    }
		    if ((w = getwd(f)) == NULL || w[0] == '\n')
			goto done_chart;
		}
done_chart:
		if (pmDebug & DBG_TRACE_APPL2) {
		    fprintf(stderr, "openView: new chart: style=%s",
				    stylestr(style));
		    if (title != NULL)
			fprintf(stderr, " title=\"%s\"", title);
		    if (autoscale)
			fprintf(stderr, " autoscale=yes");
		    else
			fprintf(stderr, " ymin=%.1f ymax=%.1f", ymin, ymax);
		    if (legend)
			fprintf(stderr, " legend=yes");
		    if (!antialias)
			fprintf(stderr, " antialias=no");
		    fputc('\n', stderr);
		}
		if (Cflag == 0 || Cflag == 2) {
		    tab = pmchart->activeTab();
		    cp = new Chart(tab, tab->splitter());
		    cp->setStyle(style);
		    cp->setScheme(scheme.name());
		    if (title != NULL)
			cp->changeTitle(title, mode == M_KMCHART);
		    if (legend == 0)
			cp->setLegendVisible(false);
		    if (antialias == 0)
			cp->setAntiAliasing(false);
		    cp->setScale(autoscale, ymin, ymax);
		    activeGroup->addGadget(cp);
		    tab->addGadget(cp);
		}
		state = S_CHART;
		if (title != NULL) free(title);
		continue;

abort_chart:
		// unrecoverable error in the chart clause of the view
		// specification, abandon this one
		if (title != NULL) free(title);
		goto abandon;
	    }
	    else if (strcasecmp(w, "global") == 0) {
		char *endnum;
		uint value, attr;

		// Global window attributes (geometry and visible points)
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("width\", \"height\", \"xpos\", \"ypos\", or \"points", w);
		    goto abandon;
		}
		else if (strcasecmp(w, "width") == 0)
		    attr = G_WIDTH;
		else if (strcasecmp(w, "height") == 0)
		    attr = G_HEIGHT;
		else if (strcasecmp(w, "points") == 0)
		    attr = G_POINTS;
		else if (strcasecmp(w, "xpos") == 0)
		    attr = G_XPOS;
		else if (strcasecmp(w, "ypos") == 0)
		    attr = G_YPOS;
		else {
		    xpect("width\", \"height\", \"xpos\", \"ypos\", or \"points", w);
		    goto abandon;
		}
		w = getwd(f);
		if (w == NULL || w[0] == '\n') {
		    xpect("<global attribute value>", w);
		    goto abandon;
		}
		value = (uint)strtoul(w, &endnum, 0);
		if (*endnum != '\0') {
		    xpect("<global attribute value>", w);
		    goto abandon;
		}
		switch (attr) {
		    case G_WIDTH:
			_width = qMax(_width, value);
			break;
		    case G_HEIGHT:
			_height = qMax(_height, value);
			break;
		    case G_POINTS:
			_points = qMax(_points, value);
			break;
		    case G_XPOS:
			_xpos = qMax(_xpos, value);
			break;
		    case G_YPOS:
			_ypos = qMax(_ypos, value);
			break;
		}
		eol(f);
	    }
	    else if (strcasecmp(w, "scheme") == 0) {
		//
		// scheme <name> <color> <color>...
		// provides finer-grained control over the color selections
		// for an individual chart.  The default color scheme is
		// named #-cycle.  A scheme can be used in place of a direct
		// color name specification, and the color for a plot is
		// then defined as the next unused color from that scheme.
		//
		w = getwd(f);
		if (w == NULL || w[0] == '\n') {
		    xpect("<color scheme value>", w);
		    goto abandon;
		}
		else if (strcmp(w, "#-cycle") == 0) {
		    xpect("<non-default color scheme name>", w);
		    goto abandon;
		}
		if (ColorScheme::lookupScheme(w) == true) {
		    // duplicate - ignore (probably using a seen view again)
		    skip2eol(f);
		    continue;
		}
		scheme.setName(QString(w));
		scheme.clear();
		w = getwd(f);
		while (w && w[0] != '\n') {
		    scheme.addColor(QString(w));
		    w = getwd(f);
		}
		if (scheme.size() < 2) {
		    xpect("<list of color names>", w);
		    goto abandon;
		}
		globalSettings.colorSchemes.append(scheme);
	    }
	    else if (strcasecmp(w, "view") == 0 || strcasecmp(w, "tab") == 0) {
new_tab:
		QString label, host;
		int samples = globalSettings.sampleHistory;
		int points = globalSettings.visibleHistory;
		char *endnum;

		w = getwd(f);
		if (w == NULL || w[0] == '\n') {
		    xpect("<tab label>", w);
		    goto abandon;
		}
		label = w;

		w = getwd(f);
		if (w == NULL || w[0] == '\n')
		    goto done_tab;

		// default "host" specification for the tab is optional
		if (strcasecmp(w, "host") == 0) {
		    w = getwd(f);
		    if (w == NULL || w[0] == '\n') {
			xpect("<host>", w);
			goto abandon;
		    }
		    host = w;
		    w = getwd(f);
		    if (w == NULL || w[0] == '\n')
			goto done_tab;
		}
		if (strcasecmp(w, "points") != 0) {
		    xpect("<tab points>", w);
		    goto abandon;
		}
		w = getwd(f);
		if (w)
	 	    points = (uint)strtoul(w, &endnum, 0);
		if (w == NULL || w[0] == '\n' || *endnum != '\0') {
		    xpect("<tab points value>", w);
		    goto abandon;
		}

		w = getwd(f);
		if (w == NULL || w[0] == '\n')
		    goto done_tab;
		if (strcasecmp(w, "samples") != 0) {
		    xpect("<tab samples>", w);
		    goto abandon;
		}
		w = getwd(f);
		if (w)
		    samples = (uint)strtoul(w, &endnum, 0);
		if (w == NULL || w[0] == '\n' || *endnum != '\0') {
		    xpect("<tab samples value>", w);
		    goto abandon;
		}

done_tab:
		tab = pmchart->activeTab();
		bool isArchive = tab->isArchiveSource();

		if (host != QString::null) {
		    if (isArchive)
			archiveGroup->use(PM_CONTEXT_ARCHIVE, host);
		    else
			liveGroup->use(PM_CONTEXT_HOST, host);
		}

		if (tab->gadgetCount() == 0) {	// edit the initial tab
		    TabWidget *tabWidget = pmchart->tabWidget();
		    tabWidget->setTabText(tabWidget->currentIndex(), label);
		}
		else {		// create a completely new tab from scratch
		    tab = new Tab;
		    if (isArchive)
			tab->init(pmchart->tabWidget(), archiveGroup, label);
		    else
			tab->init(pmchart->tabWidget(), liveGroup, label);
		    pmchart->addActiveTab(tab);
		}
		activeGroup->setSampleHistory(samples);
		activeGroup->setVisibleHistory(points);
	    }
	    else {
		xpect("chart\", \"global\", \"scheme\" or \"tab", w);
		goto abandon;
	    }
	}

	else if (state == S_CHART) {
	    int		optional;
	    char	*legend = NULL;
	    char	*color = NULL;
	    char	*host = NULL;
	    int		inst_match_type = IM_NONE;
	    int		numinst = -1;
	    int		nextinst = -1;
	    int		*instlist = NULL;
	    char	**namelist = NULL;
	    pmMetricSpec pms;
	    int		abort = 1;	// default @ skip

	    memset(&pms, 0, sizeof(pms));
	    if (strcasecmp(w, "view") == 0 || strcasecmp(w, "tab") == 0) {
		// new tab
		state = S_TOP;
		goto new_tab;
	    }
	    if (strcasecmp(w, "chart") == 0) {
		// new chart
		state = S_TOP;
		goto new_chart;
	    }
	    if (strcasecmp(w, "plot") == 0) {
		optional = 0;
	    }
	    else if (strcasecmp(w, "optional-plot") == 0) {
		optional = 1;
	    }
	    else {
		xpect("plot\" or \"optional-plot", w);
		goto skip;
	    }
	    if ((w = getwd(f)) == NULL || w[0] == '\n') {
		xpect("title\" or \"color", w);
		goto skip;
	    }
	    if (strcasecmp(w, "title") == 0 ||
	        (mode == M_KMCHART && strcasecmp(w, "legend") == 0)) {
		// optional title "<title>" or
		// (for new pmchart) legend "<title>"
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("<legend title>", w);
		    goto skip;
		}
		if ((legend = strdup(w)) == NULL) nomem();
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("color", w);
		    goto skip;
		}
	    }
	    // color <color> is mandatory for pmchart, optional for
	    // new pmchart (where the default is color #-cycle)
	    if (strcasecmp(w, "color") == 0 || strcasecmp(w, "colour") == 0) {
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("<color>", w);
		    goto skip;
		}
		if ((color = strdup(w)) == NULL) nomem();
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("host", w);
		    goto skip;
		}
	    }
	    else if (mode == M_PMCHART) {
		xpect("color", w);
		goto skip;
	    }
	    // host <host> is mandatory for pmchart, optional for
	    // new pmchart (where the default is host *)
	    if (strcasecmp(w, "host") == 0) {
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("<host>", w);
		    goto skip;
		}
		if (strcmp(w, "*") == 0)
		    host = NULL;	// just like the new pmchart default
		else {
		    if ((host = strdup(w)) == NULL) nomem();
		}
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("metric", w);
		    goto skip;
		}
	    }
	    else if (mode == M_PMCHART) {
		xpect("host", w);
		goto skip;
	    }
	    // metric is mandatory
	    if (strcasecmp(w, "metric") == 0) {
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("<metric>", w);
		    goto skip;
		}
		if ((pms.metric = strdup(w)) == NULL) nomem();
	    }
	    else {
		xpect("metric", w);
		goto skip;
	    }
	    pms.ninst = 0;
	    pms.inst[0] = NULL;
	    if ((w = getwd(f)) != NULL && w[0] != '\n') {
		// optional parts
		//	instance ...
		//	matching ...
		//	not-matching ...
		if (strcasecmp(w, "instance") == 0) {
		    inst_match_type = IM_INST;
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("<instance>", w);
			goto skip;
		    }
		    pms.ninst = 1;
		    if ((pms.inst[0] = strdup(w)) == NULL) nomem();
		}
		else if (strcasecmp(w, "matching") == 0) {
		    inst_match_type = IM_MATCH;
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("<pattern>", w);
			goto skip;
		    }
		    pms.ninst = 1;
		    pms.inst[0] = strdup(w);
		}
		else if (strcasecmp(w, "not-matching") == 0) {
		    inst_match_type = IM_NOT_MATCH;
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("<pattern>", w);
			goto skip;
		    }
		    pms.ninst = 1;
		    pms.inst[0] = strdup(w);
		}
		else {
		    xpect("instance\" or \"matching\" or \"not-matching", w);
		    goto skip;
		}
		if (mode == M_PMCHART) {
		    // pmchart has this lame "instance extends to end
		    // of line" syntax ... sigh
		    while ((w = getwd(f)) != NULL && w[0] != '\n') {
			pms.inst[0] = (char *)realloc(pms.inst[0], strlen(pms.inst[0]) + strlen(w) + 2);
			if (pms.inst[0] == NULL) nomem();
			// if more than one space in the input, touch luck!
			strcat(pms.inst[0], " ");
			strcat(pms.inst[0], w);
		    }
		    if (pms.inst[0] != NULL) {
			pms.ninst = 1;
		    }
		}
		else {
		    // expect end of line after instance/pattern
		    // (pmchart uses quotes to make instance a single
		    // lexical element in the line)
		    eol(f);
		}
	    }

	    abort = 0;
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "openView: new %s", optional ? "optional-plot" : "plot");
		if (legend != NULL) fprintf(stderr, " legend=\"%s\"", legend);
		if (color != NULL) fprintf(stderr, " color=%s", color);
		if (host != NULL) fprintf(stderr, " host=%s", host);
		fprintf(stderr, " metric=%s", pms.metric);
		if (pms.ninst == 1) {
		    fprintf(stderr, " inst=%s", pms.inst[0]);
		    if (inst_match_type == IM_NONE)
			fprintf(stderr, " match=none (botch?)");
		    else if (inst_match_type == IM_INST)
			fprintf(stderr, " match=instance");
		    else if (inst_match_type == IM_MATCH)
			fprintf(stderr, " match=matching");
		    else if (inst_match_type == IM_NOT_MATCH)
			fprintf(stderr, " match=not-matching");
		}
		fputc('\n', stderr);
	    }
	    if (Cflag == 0 || Cflag == 2) {
		pms.isarch = (activeGroup == archiveGroup);
		if (host != NULL) {
		    // host literal, add to the list of sources
		    if (activeGroup == archiveGroup) {
			QString hostname = host;
			if (archiveGroup->use(PM_CONTEXT_HOST, hostname) < 0) {
			    QString msg;
			    msg.sprintf("\nHost \"%s\" cannot be matched to an open archive for metric %s",
				host, pms.metric);
			    errmsg.append(msg);
			    goto skip;
			}
			QmcSource source = archiveGroup->context()->source();
			pms.source = source.sourceAscii();
		    }
		    else {
			pms.source = strdup(host);
		    }
		}
		else {
		    // no explicit host, use current default source
		    QmcSource source = activeGroup->context()->source();
		    pms.source = strdup((const char *)
					source.source().toAscii());
		}
		// expand instances when not specified for metrics
		// with instance domains and all instances required,
		// or matching or not-matching instances required
		//
		if (inst_match_type != IM_INST) {
		    pmID	pmid;
		    pmDesc	desc;

		    // if pmLookupName() or pmLookupDesc() fail, we'll
		    // notice in addPlot() and report the error below,
		    // so no need to do anything special here
		    //
		    if (pmLookupName(1, &pms.metric, &pmid) < 0)
			goto try_plot;
		    if (pmLookupDesc(pmid, &desc) < 0)
			goto try_plot;
		    if (desc.indom == PM_INDOM_NULL) {
			if (inst_match_type == IM_MATCH ||
			    inst_match_type == IM_NOT_MATCH) {
			    // a bit embarrassing
			    QString	msg = QString();
			    msg.sprintf("\nMetric \"%s\" for\n%s %s: no instance domain, cannot handle matching specification",
				pms.metric, pms.isarch ? "archive" : "host",
				pms.source);
			    errmsg.append(msg);
			    goto skip;
			}
			goto try_plot;
		    }

		    if (pms.isarch)
			numinst = pmGetInDomArchive(desc.indom, &instlist, &namelist);
		    else
			numinst = pmGetInDom(desc.indom, &instlist, &namelist);
		    if (numinst < 0) {
			QString	msg = QString();
			msg.sprintf("\nMetric \"%s\" for\n%s %s: empty instance domain",
			    pms.metric, pms.isarch ? "archive" : "host",
			    pms.source);
			errmsg.append(msg);
			goto skip;
		    }
		    if (inst_match_type != IM_NONE) {
			regex.setPattern(pms.inst[0]);
			if (!regex.isValid()) {
			    errmsg = "Invalid regular expression:\n  ";
			    errmsg.append(pms.inst[0]);
			    errmsg.append("\n\n");
			    errmsg.append(regex.errorString());
			    goto skip;
			}
		    }
		    pms.ninst = numinst ? 1 : 0;
		    if (pms.inst[0] != NULL) {
			free(pms.inst[0]);
			pms.inst[0] = NULL;
		    }
		}

try_plot:
		if (numinst > 0) {
		    pms.inst[0] = NULL;
		    for (nextinst++ ; nextinst < numinst; nextinst++) {
			sts = 0;
			if (inst_match_type == IM_MATCH ||
			    inst_match_type == IM_NOT_MATCH) {
			    sts = regex.indexIn(QString(namelist[nextinst]));
			}
			switch (inst_match_type) {
			    case IM_MATCH:
				if (sts != -1)
				    pms.inst[0] = namelist[nextinst];
				break;
			    case IM_NOT_MATCH:
				if (sts == -1)
				    pms.inst[0] = namelist[nextinst];
				break;
			    case IM_NONE:
				pms.inst[0] = namelist[nextinst];
				break;
			}
			if (pms.inst[0] != NULL)
			    break;
		    }
		    if (nextinst == numinst)
			goto skip;
		}
		m = cp->addItem(&pms, QString(legend));
		if (m < 0) {
		    if (!optional) {
			QString	msg;
			if (pms.inst[0] != NULL)
			    msg.sprintf("\nFailed to plot metric \"%s[%s]\" for\n%s %s:\n",
				pms.metric, pms.inst[0],
				pms.isarch ? "archive" : "host",
				pms.source);
			else
			    msg.sprintf("\nFailed to plot metric \"%s\" for\n%s %s:\n",
				pms.metric, pms.isarch ? "archive" : "host",
				pms.source);
			    if (m == PM_ERR_CONV) {
				msg.append("Units for this metric are not compatible with other plots in this chart");
			    }
			    else
				msg.append(pmErrStr(m));
			errmsg.append(msg);
		    }
		}
		else if (color != NULL && strcmp(color, "#-cycle") != 0) {
		    int seq = cp->sequence();
		    cp->setStroke(m, cp->style(), colorSpec(color, &seq));
		    cp->setSequence(seq);
		}
		if (numinst > 0)
		    // more instances still to be processed for this metric
		    goto try_plot;

	    }

skip:
	    if (legend != NULL) free(legend);
	    if (color != NULL) free(color);
	    if (host != NULL) free(host);
	    if (instlist != NULL) free(instlist);
	    if (namelist != NULL) free(namelist);
	    if (pms.source != NULL) free(pms.source);
	    if (pms.metric != NULL) free(pms.metric);
	    if (pms.inst[0] != NULL) free(pms.inst[0]);

	    if (abort)
		goto abandon;

	    continue;
	}
	
	else {
	    QString	msg = QString();
	    msg.sprintf("Botch, state=%d", state);
	    err(E_CRIT, true, msg);
	    goto abandon;
	}

	continue;

abandon:
	// giving up on the whole view specification
	break;

    }

    if (!errmsg.isEmpty()) {
	err(E_CRIT, true, errmsg);
    }

    if (f != stdin) {
	if (is_popen)
	    pclose(f);
	else
	    fclose(f);
    }

    if (_errors)
	return false;

    if (ct != pmchart->tabWidget()->currentIndex())	// new Tabs added
	pmchart->setActiveTab(ct, true);

    if ((Cflag == 0 || Cflag == 2) && cp != NULL) {
	activeGroup->setupWorldView();
	pmchart->activeTab()->showGadgets();
    }
    return true;

noview:
    errmsg = QString("Cannot open view file \"");
    errmsg.append(_fname);
    errmsg.append("\"\n");
    errmsg.append(strerror(errno));
    err(E_CRIT, false, errmsg);
    return false;

nopipe:
    errmsg.sprintf("Cannot execute \"%s\"\n%s", _fname, strerror(errno));
    err(E_CRIT, false, errmsg);
    return false;
}

void SaveViewDialog::setGlobals(int width, int height, int points, int x, int y)
{
    _width = width;
    _height = height;
    _points = points;
    _xpos = x;
    _ypos = y;
}

static void saveScheme(FILE *f, QString scheme)
{
    ColorScheme	*cs = ColorScheme::findScheme(scheme);
    int		m;

    if (cs) {
	fprintf(f, "scheme %s", (const char *)cs->name().toAscii());
	for (m = 0; m < cs->size(); m++)
	    fprintf(f, " %s", (const char *)cs->colorName(m).toAscii());
	fprintf(f, "\n\n");
    }
}

void SaveViewDialog::saveChart(FILE *f, Chart *cp, bool hostDynamic)
{
    const char	*s;
    double	ymin, ymax;
    bool	autoscale;

    fprintf(f, "chart");
    if (cp->title() != QString::null)
	fprintf(f, " title \"%s\"", (const char*)cp->title().toAscii());
    switch (cp->style()) {
	case Chart::LineStyle:
	    s = "plot";
	    break;
	case Chart::BarStyle:
	    s = "bar";
	    break;
	case Chart::StackStyle:
	    s ="stacking";
	    break;
	case Chart::AreaStyle:
	    s = "area";
	    break;
	case Chart::UtilisationStyle:
	    s = "utilization";
	    break;
	case Chart::EventStyle:
	    s = "event";
	    break;
	case Chart::NoStyle:
	default:
	    s = "none";
	    break;
    }
    fprintf(f, " style %s", s);
    if (cp->style() != Chart::UtilisationStyle) {
	cp->scale(&autoscale, &ymin, &ymax);
	if (!autoscale)
	    fprintf(f, " scale %f %f", ymin, ymax);
    }
    if (!cp->legendVisible())
	fprintf(f, " legend off");
    if (!cp->antiAliasing())
	fprintf(f, " antialiasing off");
    fputc('\n', f);
    for (int m = 0; m < cp->metricCount(); m++) {
	QString legend;
	if (cp->activeItem(m) == false)
	    continue;
	fprintf(f, "\tplot");
	legend = cp->legend(m);
	if (legend != QString::null)
	    fprintf(f, " legend \"%s\"", (const char *)legend.toAscii());
	fprintf(f, " color %s", (const char *)cp->color(m).name().toAscii());
	if (hostDynamic == false)
	    fprintf(f, " host %s", (const char *)
			cp->metricContext(m)->source().host().toAscii());
        fprintf(f, " metric %s", (const char *)
                cp->metricName(m).toAscii());
	if (cp->metricPtr(m)->explicitInsts())
            fprintf(f, " instance \"%s\"", (const char*)cp->metricInstance(m).toAscii());
	fputc('\n', f);
    }
}

bool SaveViewDialog::saveView(QString file, bool hostDynamic,
				bool sizeDynamic, bool allTabs, bool allCharts)
{
    FILE	*f;
    int		c, t;
    Tab		*tab;
    Gadget	*gadget;
    char	*path = strdup((const char *)file.toAscii());
    QStringList	schemes;

    if ((f = fopen(path, "w")) == NULL)
	goto noview;

    fprintf(f, "#kmchart\nversion %d\n\n", K1);
    if (sizeDynamic == false) {
	fprintf(f, "global width %u\n", _width);
	fprintf(f, "global height %u\n", _height);
	fprintf(f, "global points %u\n", _points);
	fprintf(f, "global xpos %u\n", _xpos);
	fprintf(f, "global ypos %u\n", _ypos);
	fprintf(f, "\n");
    }

    for (c = 0; c < pmchart->activeTab()->gadgetCount(); c++) {
	gadget = pmchart->activeTab()->gadget(c);
	if (gadget->scheme() == QString::null ||
	    schemes.contains(gadget->scheme()) == true)
	    continue;
	schemes.append(gadget->scheme());
    }
    for (c = 0; c < schemes.size(); c++)
	saveScheme(f, schemes.at(c));

    if (allTabs) {
	TabWidget *tabWidget = pmchart->tabWidget();
	for (t = 0; t < tabWidget->size(); t++) {
	    tab = tabWidget->at(t);
	    fprintf(f, "\ntab \"%s\"\n\n",
		    (const char *) tabWidget->tabText(t).toAscii());
	    for (c = 0; c < tab->gadgetCount(); c++)
		tab->gadget(c)->save(f, hostDynamic);
	}
    }
    else {
	tab = pmchart->activeTab();
	if (!allCharts)
	    tab->currentGadget()->save(f, hostDynamic);
	else
	    for (c = 0; c < tab->gadgetCount(); c++)
		tab->gadget(c)->save(f, hostDynamic);
    }

    fflush(f);
    fclose(f);
    free (path);
    return true;

noview:
    QString errmsg;
    errmsg.sprintf("Cannot open \"%s\" for writing\n%s", path, strerror(errno));
    err(E_CRIT, false, errmsg);
    free (path);
    return false;
}
