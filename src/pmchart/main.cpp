/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007-2009, Aconex.  All Rights Reserved.
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
#include <QtCore/QSettings>
#include <QtGui/QStatusBar>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include "main.h"
#include "openviewdialog.h"

#define DESPERATE 0

int Cflag;
int Hflag;
int Lflag;
int Wflag;
char *outfile;
char *outgeometry;
QFont *globalFont;
Settings globalSettings;

// Globals used to provide single instances of classes used across pmchart
GroupControl *liveGroup;	// one metrics class group for all hosts
GroupControl *archiveGroup;	// one metrics class group for all archives
GroupControl *activeGroup;	// currently active metric fetchgroup
TimeControl *pmtime;		// one timecontrol class for pmtime
PmChart *pmchart;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ALIGN,
    PMOPT_ARCHIVE,
    PMOPT_DEBUG,
    PMOPT_HOST,
    PMOPT_HOSTSFILE,
    PMOPT_NAMESPACE,
    PMOPT_SPECLOCAL,
    PMOPT_LOCALPMDA,
    PMOPT_ORIGIN,
    PMOPT_GUIPORT,
    PMOPT_START,
    PMOPT_SAMPLES,
    PMOPT_FINISH,
    PMOPT_INTERVAL,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Display options"),
    { "view", 1, 'c', "VIEW", "chart view(s) to load on startup" },
    { "check", 0, 'C', 0, "parse views, report any errors and exit" },
    { "font-size", 1, 'F', "SIZE", "use font of given size" },
    { "font-family", 1, 'f', "FONT", "use font family" },
    { "geometry", 1, 'g', "WxH", "image geometry Width x Height" },
    { "output", 1, 'o', "FILE", "export image to FILE (type from suffix)" },
    { "samples", 1, 's', "N", "buffer up N points of sample history" },
    { "visible", 1, 'v', "N", "display N points of visible history" },
    { "white", 0, 'W', 0, "export images using an opaque (white) background" },
    PMAPI_OPTIONS_END
};

//
// a : b for struct timevals ... <0 for a<b, ==0 for a==b, >0 for a>b
//
int tcmp(struct timeval *a, struct timeval *b)
{
    int res = (int)(a->tv_sec - b->tv_sec);
    if (res == 0)
	res = (int)(a->tv_usec - b->tv_usec);
    return res;
}

// create a time range in seconds from (delta x points)
double torange(struct timeval t, int points)
{
    return __pmtimevalToReal(&t) * points;
}

// debugging, display seconds-since-epoch in human readable format
char *timeString(double seconds)
{
    static char string[32];
    time_t secs = (time_t)seconds;
    char *s;

    s = pmCtime(&secs, string);
    s[strlen(s)-1] = '\0';
    return s;
}

// return a string containing hour and milliseconds
char *timeHiResString(double time)
{
    static char s[16];
    char m[8];
    time_t secs = (time_t)time;
    struct tm t;

    sprintf(m, "%.3f", time - floor(time));
    pmLocaltime(&secs, &t);
    sprintf(s, "%02d:%02d:%02d.%s", t.tm_hour, t.tm_min, t.tm_sec, m+2);
    s[strlen(s)-1] = '\0';
    return s;
}

void nomem(void)
{
    // no point trying to report anything ... dump core is the best bet
    abort();
}

void setupEnvironment(void)
{
    char *value;
    QString confirm = pmGetConfig("PCP_BIN_DIR");
    confirm.prepend("PCP_XCONFIRM_PROG=");
    confirm.append(QChar(__pmPathSeparator()));
    confirm.append("pmquery");
    if ((value = strdup((const char *)confirm.toAscii())) != NULL)
	putenv(value);
    if (getenv("PCP_STDERR") == NULL &&	// do not overwrite, for QA
	((value = strdup("PCP_STDERR=DISPLAY")) != NULL))
	putenv(value);

    QCoreApplication::setOrganizationName("PCP");
    QCoreApplication::setApplicationName(pmProgname);
}

void writeSettings(void)
{
    QSettings userSettings;

    userSettings.beginGroup(pmProgname);
    if (globalSettings.chartDeltaModified) {
	globalSettings.chartDeltaModified = false;
	userSettings.setValue("chartDelta", globalSettings.chartDelta);
    }
    if (globalSettings.loggerDeltaModified) {
	globalSettings.loggerDeltaModified = false;
	userSettings.setValue("loggerDelta", globalSettings.loggerDelta);
    }
    if (globalSettings.sampleHistoryModified) {
	globalSettings.sampleHistoryModified = false;
	userSettings.setValue("sampleHistory", globalSettings.sampleHistory);
    }
    if (globalSettings.visibleHistoryModified) {
	globalSettings.visibleHistoryModified = false;
	userSettings.setValue("visibleHistory", globalSettings.visibleHistory);
    }
    if (globalSettings.defaultSchemeModified) {
	globalSettings.defaultSchemeModified = false;
	userSettings.setValue("defaultColorScheme",
				globalSettings.defaultScheme.colorNames());
    }
    if (globalSettings.colorSchemesModified) {
	globalSettings.colorSchemesModified = false;
	userSettings.beginWriteArray("schemes");
	for (int i = 0; i < globalSettings.colorSchemes.size(); i++) {
	    userSettings.setArrayIndex(i);
	    userSettings.setValue("name",
				globalSettings.colorSchemes[i].name());
	    userSettings.setValue("colors",
				globalSettings.colorSchemes[i].colorNames());
	}
	userSettings.endArray();
    }
    if (globalSettings.chartBackgroundModified) {
	globalSettings.chartBackgroundModified = false;
	userSettings.setValue("chartBackgroundColor",
				globalSettings.chartBackgroundName);
    }
    if (globalSettings.chartHighlightModified) {
	globalSettings.chartHighlightModified = false;
	userSettings.setValue("chartHighlightColor",
				globalSettings.chartHighlightName);
    }
    if (globalSettings.initialToolbarModified) {
	globalSettings.initialToolbarModified = false;
	userSettings.setValue("initialToolbar", globalSettings.initialToolbar);
    }
    if (globalSettings.nativeToolbarModified) {
	globalSettings.nativeToolbarModified = false;
	userSettings.setValue("nativeToolbar", globalSettings.nativeToolbar);
    }
    if (globalSettings.toolbarLocationModified) {
	globalSettings.toolbarLocationModified = false;
	userSettings.setValue("toolbarLocation",
				globalSettings.toolbarLocation);
    }
    if (globalSettings.toolbarActionsModified) {
	globalSettings.toolbarActionsModified = false;
	userSettings.setValue("toolbarActions", globalSettings.toolbarActions);
    }
    if (globalSettings.fontFamilyModified) {
	globalSettings.fontFamilyModified = false;
	if (globalSettings.fontFamily != QString(PmChart::defaultFontFamily()))
	    userSettings.setValue("fontFamily", globalSettings.fontFamily);
	else
	    userSettings.remove("fontFamily");
    }
    if (globalSettings.fontStyleModified) {
	globalSettings.fontStyleModified = false;
	if (globalSettings.fontStyle != QString("Normal"))
	    userSettings.setValue("fontStyle", globalSettings.fontStyle);
	else
	    userSettings.remove("fontStyle");
    }
    if (globalSettings.fontSizeModified) {
	globalSettings.fontSizeModified = false;
	if (globalSettings.fontSize != PmChart::defaultFontSize())
	    userSettings.setValue("fontSize", globalSettings.fontSize);
	else
	    userSettings.remove("fontSize");
    }
    if (globalSettings.savedHostsModified) {
	globalSettings.savedHostsModified = false;
	if (globalSettings.savedHosts.isEmpty() == false)
	    userSettings.setValue("savedHosts", globalSettings.savedHosts);
	else
	    userSettings.remove("savedHosts");
    }

    userSettings.endGroup();
}

void checkHistory(int samples, int visible)
{
    // sanity checking on sample sizes
    if (samples < PmChart::minimumPoints()) {
	globalSettings.sampleHistory = PmChart::minimumPoints();
	globalSettings.sampleHistoryModified = true;
    }
    if (samples > PmChart::maximumPoints()) {
	globalSettings.sampleHistory = PmChart::maximumPoints();
	globalSettings.sampleHistoryModified = true;
    }
    if (visible < PmChart::minimumPoints()) {
	globalSettings.visibleHistory = PmChart::minimumPoints();
	globalSettings.visibleHistoryModified = true;
    }
    if (visible > PmChart::maximumPoints()) {
	globalSettings.visibleHistory = PmChart::maximumPoints();
	globalSettings.visibleHistoryModified = true;
    }
    if (samples < visible) {
	globalSettings.sampleHistory = globalSettings.visibleHistory;
	globalSettings.sampleHistoryModified = true;
    }
}

static void readSettings(void)
{
    QSettings userSettings;
    userSettings.beginGroup(pmProgname);

    //
    // Parameters related to sampling
    //
    globalSettings.chartDeltaModified = false;
    globalSettings.chartDelta = userSettings.value("chartDelta",
				PmChart::defaultChartDelta()).toDouble();
    globalSettings.loggerDeltaModified = false;
    globalSettings.loggerDelta = userSettings.value("loggerDelta",
				PmChart::defaultLoggerDelta()).toDouble();
    globalSettings.sampleHistoryModified = false;
    globalSettings.sampleHistory = userSettings.value("sampleHistory",
				PmChart::defaultSampleHistory()).toInt();
    globalSettings.visibleHistoryModified = false;
    globalSettings.visibleHistory = userSettings.value("visibleHistory",
				PmChart::defaultVisibleHistory()).toInt();
    checkHistory(globalSettings.sampleHistory, globalSettings.visibleHistory);
    if (globalSettings.sampleHistoryModified) {
	userSettings.setValue("samplePoints", globalSettings.sampleHistory);
	globalSettings.sampleHistoryModified = false;
    }
    if (globalSettings.visibleHistoryModified) {
	userSettings.setValue("visiblePoints", globalSettings.visibleHistory);
	globalSettings.visibleHistoryModified = false;
    }

    //
    // Everything colour (scheme) related
    //
    QStringList colorList;
    globalSettings.defaultSchemeModified = false;
    if (userSettings.contains("defaultColorScheme") == true)
	colorList = userSettings.value("defaultColorScheme").toStringList();
    else
	colorList
	    << "#ffff00" << "#0000ff" << "#ff0000" << "#008000" << "#ee82ee"
	    << "#aa5500" << "#666666" << "#aaff00" << "#aa00ff" << "#aaaa7f";
    globalSettings.defaultScheme.setName("#-cycle");
    globalSettings.defaultScheme.setModified(false);
    globalSettings.defaultScheme.setColorNames(colorList);

    int size = userSettings.beginReadArray("schemes");
    for (int i = 0; i < size; i++) {
	userSettings.setArrayIndex(i);
	ColorScheme scheme;
	scheme.setName(userSettings.value("name").toString());
	scheme.setModified(false);
	scheme.setColorNames(userSettings.value("colors").toStringList());
	globalSettings.colorSchemes.append(scheme);
    }
    userSettings.endArray();

    //
    // Everything (else) colour related
    //
    globalSettings.chartBackgroundModified = false;
    globalSettings.chartBackgroundName = userSettings.value(
		"chartBackgroundColor", "#6ca2c9").toString();
    globalSettings.chartBackground = QColor(globalSettings.chartBackgroundName);

    globalSettings.chartHighlightModified = false;
    globalSettings.chartHighlightName = userSettings.value(
		"chartHighlightColor", "blue").toString();
    globalSettings.chartHighlight = QColor(globalSettings.chartHighlightName);

    //
    // Toolbar user preferences
    //
    globalSettings.initialToolbarModified = false;
    globalSettings.initialToolbar = userSettings.value(
					"initialToolbar", 1).toInt();
    globalSettings.nativeToolbarModified = false;
    globalSettings.nativeToolbar = userSettings.value(
					"nativeToolbar", 1).toInt();
    globalSettings.toolbarLocationModified = false;
    globalSettings.toolbarLocation = userSettings.value(
					"toolbarLocation", 0).toInt();
    QStringList actionList;
    globalSettings.toolbarActionsModified = false;
    if (userSettings.contains("toolbarActions") == true)
	globalSettings.toolbarActions =
			userSettings.value("toolbarActions").toStringList();
    // else: (defaults come from the pmchart.ui interface specification)

    //
    // Font preferences
    //
    globalSettings.fontFamilyModified = false;
    globalSettings.fontFamily = userSettings.value(
		"fontFamily", PmChart::defaultFontFamily()).toString();
    globalSettings.fontStyleModified = false;
	QString fontStyle;
    globalSettings.fontStyle = userSettings.value(
		"fontStyle", "Normal").toString();
    globalSettings.fontSizeModified = false;
    globalSettings.fontSize = userSettings.value(
		"fontSize", PmChart::defaultFontSize()).toInt();

    //
    // Saved Hosts list preferences
    //
    globalSettings.savedHostsModified = false;
    if (userSettings.contains("savedHosts") == true)
	globalSettings.savedHosts =
			userSettings.value("savedHosts").toStringList();

    userSettings.endGroup();
}

static void readSchemes(void)
{
    QChar sep(__pmPathSeparator());
    QString schemes = pmGetConfig("PCP_VAR_DIR");
    schemes.append(sep).append("config");
    schemes.append(sep).append("pmchart");
    schemes.append(sep).append("Schemes");

    QFileInfo fi(schemes);
    if (fi.exists())
	OpenViewDialog::openView(schemes.toAscii());
}

// Get next color from given scheme or from default colors for #-cycle
QColor nextColor(QString scheme, int *sequence)
{
    QList<QColor> colorList;
    int seq = (*sequence)++;

    for (int i = 0; i < globalSettings.colorSchemes.size(); i++) {
	if (globalSettings.colorSchemes[i].name() == scheme) {
	    colorList = globalSettings.colorSchemes[i].colors();
	    break;
	}
    }
    if (colorList.size() < 2)	// common case
	colorList = globalSettings.defaultScheme.colors();
    if (colorList.size() < 2)	// idiot user!?
	colorList << QColor("yellow") << QColor("blue") << QColor("red")
		  << QColor("green") << QColor("violet");
    seq %= colorList.size();
    return colorList.at(seq);
}

static void setupViewGlobals()
{
    int w, h, points, x, y;

    OpenViewDialog::globals(&w, &h, &points, &x, &y);
    if (w || h) {
	QSize size = pmchart->size().expandedTo(QSize(w, h));
	QSize desk = QApplication::desktop()->availableGeometry().size();
	pmchart->resize(size.boundedTo(desk));
    }
    if (x || y) {
	QPoint pos = pmchart->pos();
	if (x) pos.setX(x);
	if (y) pos.setY(y);
	pmchart->move(pos);
    }
    if (points) {
	if (activeGroup->sampleHistory() < points)
	    activeGroup->setSampleHistory(points);
	activeGroup->setVisibleHistory(points);
    }
}

static int
override(int opt, pmOptions *opts)
{
    (void)opts;
    if (opt == 'g')
        return 1;
    if (opt == 'H')
	Hflag = 1;
    if (opt == 'L')
	Lflag = 1;
    return 0;
}

int
main(int argc, char ** argv)
{
    int			c, sts;
    int			sh = -1;		/* sample history length */
    int			vh = -1;		/* visible history length */
    char		*endnum;
    Tab			*tab;
    struct timeval	logStartTime;
    struct timeval	logEndTime;
    QStringList		configs;
    QString		tzLabel;
    QString		tzString;
    pmOptions		opts;

    memset(&opts, 0, sizeof(opts));
    __pmtimevalNow(&opts.origin);
    __pmSetProgname(argv[0]);
    QApplication a(argc, argv);
    setupEnvironment();
    readSettings();

    opts.flags = PM_OPTFLAG_MULTI | PM_OPTFLAG_MIXED;
    opts.short_options = "A:a:Cc:D:f:F:g:h:H:Ln:o:O:p:s:S:T:t:Vv:WzZ:?";
    opts.long_options = longopts;
    opts.short_usage = "[options] [sources]";
    opts.override = override;


    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'C':
	    Cflag++;
	    break;

	case 'c':
	    configs.append(opts.optarg);
	    break;

	case 'f':
	    globalSettings.fontFamily = opts.optarg;
	    break;

	case 'F':
	    sts = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || c < 0) {
		pmprintf("%s: -F requires a numeric argument\n", pmProgname);
		opts.errors++;
	    } else {
		globalSettings.fontSize = sts;
	    }
	    break;

	case 'g':
	    outgeometry = opts.optarg;
	    break;

	case 'o':		/* output image file */
	    outfile = opts.optarg;
	    break;

	case 'W':		/* white image background */
	    Wflag = 1;
	    break;

	case 'v':		/* visible history */
	    vh = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || vh < 1) {
		pmprintf("%s: -v requires a numeric argument, larger than 1\n",
			 pmProgname);
		opts.errors++;
	    }
	    break;
	}
    }

    /* hosts from a Hosts file are added to the SavedHosts list */
    if (Hflag) {
	for (int i = 0; i < opts.nhosts; i++)
	    globalSettings.savedHosts.append(opts.hosts[i]);
	globalSettings.savedHostsModified = true;
    }

    if (opts.narchives > 0) {
	while (opts.optind < argc)
	    __pmAddOptArchive(&opts, argv[opts.optind++]);
    } else {
	if (!Hflag) {
	    for (c = 0; c < globalSettings.savedHosts.size(); c++) {
		const QString &host = globalSettings.savedHosts.at(c);
		char *name = strdup((const char *)host.toAscii());
		if (name)
		    __pmAddOptHost(&opts, name);
	    }
	}
	while (opts.optind < argc)
	    __pmAddOptHost(&opts, argv[opts.optind++]);
    }

    if (opts.optind != argc)
	opts.errors++;
    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    /* set initial sampling interval from command line, else global setting */
    if (opts.interval.tv_sec == 0 && opts.interval.tv_usec == 0)
	__pmtimevalFromReal(globalSettings.chartDelta, &opts.interval);

    console = new QedConsole(opts.origin);

    //
    // Deal with user requested sample/visible points globalSettings.  These
    // (command line) override the QSettings values, for this instance
    // of pmchart.  They should not be written though, unless requested
    // later via the Settings dialog.
    //
    sh = opts.samples ? opts.samples : -1;
    if (vh != -1 || sh != -1) {
	if (sh == -1)
	    sh = globalSettings.sampleHistory;
	if (vh == -1)
	    vh = globalSettings.visibleHistory;
	checkHistory(sh, vh);
	if (globalSettings.sampleHistoryModified ||
	    globalSettings.visibleHistoryModified) {
	    pmprintf("%s: invalid sample/visible history\n", pmProgname);
	    pmflush();
	    exit(1);
	}
	globalSettings.sampleHistory = sh;
	globalSettings.visibleHistory = vh;
    }
    console->post("Global settings setup complete");

    // Create all of the sources
    liveGroup = new GroupControl();
    archiveGroup = new GroupControl();
    if (Lflag)
	liveGroup->use(PM_CONTEXT_LOCAL, QmcSource::localHost);
    sts = opts.nhosts + opts.narchives;
    for (c = 0; c < opts.nhosts; c++)
	if (liveGroup->use(PM_CONTEXT_HOST, opts.hosts[c]) < 0)
	    sts--;
    for (c = 0; c < opts.narchives; c++)
	if (archiveGroup->use(PM_CONTEXT_ARCHIVE, opts.archives[c]) < 0)
	    sts--;
    if (Lflag == 0 && sts == 0)
	liveGroup->createLocalContext();
    pmflush();
    console->post("Metric group setup complete (%d hosts, %d archives)",
			opts.nhosts, opts.narchives);

    if (opts.tzflag) {
	if (opts.narchives > 0)
	    archiveGroup->useTZ();
	if (opts.nhosts > 0)
	    liveGroup->useTZ();
    }
    else if (opts.timezone != NULL) {
	if (opts.narchives > 0)
	    archiveGroup->useTZ(QString(opts.timezone));
	if (opts.nhosts > 0)
	    liveGroup->useTZ(QString(opts.timezone));
	if ((sts = pmNewZone(opts.timezone)) < 0) {
	    pmprintf("%s: cannot set timezone to \"%s\": %s\n",
		    pmProgname, (char *)opts.timezone, pmErrStr(sts));
	    pmflush();
	    exit(1);
	}
    }

    //
    // Choose which Tab will be displayed initially - archive/live.
    // If any archives given on command line, we go Archive mode;
    // otherwise Live mode wins.  Our initial pmtime connection is
    // set in that mode too.  Later we'll make a second connection
    // in the other mode (and only "on-demand").
    //
    if (opts.narchives > 0) {
	archiveGroup->defaultTZ(tzLabel, tzString);
	archiveGroup->updateBounds();
	logStartTime = archiveGroup->logStart();
	logEndTime = archiveGroup->logEnd();
	if ((sts = pmParseTimeWindow(opts.start_optarg, opts.finish_optarg,
					opts.align_optarg, opts.origin_optarg,
					&logStartTime, &logEndTime, &opts.start,
					&opts.finish, &opts.origin, &endnum)) < 0) {
	    pmprintf("Cannot parse archive time window\n%s\n", endnum);
	    pmUsageMessage(&opts);
	    free(endnum);
	    exit(1);
	}
	// move position to account for initial visible points
	if (tcmp(&opts.origin, &opts.start) <= 0)
	    for (c = 0; c < globalSettings.visibleHistory - 2; c++)
		__pmtimevalAdd(&opts.origin, &opts.interval);
	if (tcmp(&opts.origin, &opts.finish) > 0)
	    opts.origin = opts.finish;
    }
    else {
	liveGroup->defaultTZ(tzLabel, tzString);
	__pmtimevalNow(&logStartTime);
	logEndTime.tv_sec = logEndTime.tv_usec = INT_MAX;
	if ((sts = pmParseTimeWindow(opts.start_optarg, opts.finish_optarg,
					opts.align_optarg, opts.origin_optarg,
					&logStartTime, &logEndTime, &opts.start,
					&opts.finish, &opts.origin, &endnum)) < 0) {
	    pmprintf("Cannot parse live time window\n%s\n", endnum);
	    pmUsageMessage(&opts);
	    free(endnum);
	    exit(1);
	}
    }
    console->post("Timezones and time window setup complete");

    globalFont = new QFont(globalSettings.fontFamily, globalSettings.fontSize);
    if (globalSettings.fontStyle.contains("Italic"))
	globalFont->setItalic(true);
    if (globalSettings.fontStyle.contains("Bold"))
	globalFont->setBold(true);

    tab = new Tab;
    fileIconProvider = new QedFileIconProvider();

    pmchart = new PmChart;
    pmtime = new TimeControl;

    console->post("Phase1 user interface constructors complete");

    // Start pmtime process for time management
    pmtime->init(opts.guiport, opts.narchives == 0, &opts.interval, &opts.origin,
		 &opts.start, &opts.finish, tzString, tzLabel);

    pmchart->init();
    liveGroup->init(globalSettings.sampleHistory,
			globalSettings.visibleHistory,
			pmtime->liveInterval(), pmtime->livePosition());
    archiveGroup->init(globalSettings.sampleHistory,
			globalSettings.visibleHistory,
			pmtime->archiveInterval(), pmtime->archivePosition());

    //
    // We setup the pmchart tab list late, so we don't have to deal
    // with pmtime messages reaching the Tabs until we're all setup.
    //
    if (opts.narchives == 0)
	tab->init(pmchart->tabWidget(), liveGroup, "Live");
    else
	tab->init(pmchart->tabWidget(), archiveGroup, "Archive");
    pmchart->tabWidget()->insertTab(tab);
    pmchart->setActiveTab(0, true);
    console->post("Phase2 user interface setup complete");

    readSchemes();
    for (c = 0; c < configs.size(); c++)
	if (!OpenViewDialog::openView((const char *)configs[c].toAscii()))
	    opts.errors++;
    if (opts.errors)
	exit(1);
    setupViewGlobals();
    pmflush();

    if (Cflag)	// done with -c config, quit
	return 0;

    pmchart->enableUi();
    pmchart->show();
    console->post("Top level window shown");

    a.connect(&a, SIGNAL(lastWindowClosed()), pmchart, SLOT(quit()));
    return a.exec();
}
