/*
 * Copyright (c) 2014 Red Hat.
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

static const char *options = "A:a:Cc:D:f:F:g:h:H:Lo:n:O:p:s:S:T:t:Vv:WzZ:?";

static void usage(void)
{
    pmprintf("Usage: %s [options] [sources]\n\n"
"Options:\n"
"  -A align      align sample times on natural boundaries\n"
"  -a archive    add PCP log archive to metrics source list\n"
"  -c configfile initial view to load\n"
"  -C            with -c, parse config, report any errors and exit\n"
"  -CC           like -C, but also connect to pmcd to check semantics\n"
"  -F fontsize   use font of given size [default: %d]\n"
"  -f font       use font family [default: %s]\n"
"  -g geometry   image geometry Width x Height (WxH)\n"
"  -H hostfile   setup a list of saved hosts for sourcing metrics\n"
"  -h host       add host to list of live metrics sources\n"
"  -L            directly fetch metrics from localhost, PMCD is not used\n"
"  -n pmnsfile   use an alternative PMNS\n"
"  -O offset     initial offset into the time window\n"
"  -o outfile    export image to outfile\n"
"  -p port       port name for connection to existing time control\n"
"  -S starttime  start of the time window\n"
"  -s samples    sample history [default: %d points]\n"
"  -T endtime    end of the time window\n"
"  -t interval   sample interval [default: %d seconds]\n"
"  -V            display pmchart version number and exit\n"
"  -v visible    visible history [default: %d points]\n"
"  -W            export images using an opaque (white) background\n"
"  -Z timezone   set reporting timezone\n"
"  -z            set reporting timezone to local time of metrics source\n",
	pmProgname,
	PmChart::defaultFontSize(),
	PmChart::defaultFontFamily(),
	PmChart::defaultSampleHistory(),
	(int)PmChart::defaultChartDelta(),
	PmChart::defaultVisibleHistory());
    pmflush();
    exit(1);
}

// a := a + b for struct timevals
void tadd(struct timeval *a, struct timeval *b)
{
    a->tv_usec += b->tv_usec;
    if (a->tv_usec > 1000000) {
	a->tv_usec -= 1000000;
	a->tv_sec++;
    }
    a->tv_sec += b->tv_sec;
}

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

// convert timeval to seconds
double tosec(struct timeval t)
{
    return t.tv_sec + (t.tv_usec / 1000000.0);
}

// create a time range in seconds from (delta x points)
double torange(struct timeval t, int points)
{
    return tosec(t) * points;
}

// conversion from seconds (double precision) to struct timeval
void fromsec(double value, struct timeval *tv)
{
    tv->tv_sec = (time_t)value;
    tv->tv_usec = (long)(((value - (double)tv->tv_sec) * 1000000.0));
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
    QString confirm = pmGetConfig("PCP_BIN_DIR");
    confirm.prepend("PCP_XCONFIRM_PROG=");
    confirm.append(QChar(__pmPathSeparator()));
    confirm.append("pmquery");
    putenv(strdup((const char *)confirm.toAscii()));
    if (getenv("PCP_STDERR") == NULL)	// do not overwrite, for QA
	putenv(strdup("PCP_STDERR=DISPLAY"));

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

void readSettings(void)
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

void readSchemes(void)
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

void setupViewGlobals()
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

int
main(int argc, char ** argv)
{
    int			c, sts;
    int			errflg = 0;
    char		*endnum, *msg;
    char		*pmnsfile = NULL;	/* local namespace file */
    char		*Sflag = NULL;		/* argument of -S flag */
    char		*Tflag = NULL;		/* argument of -T flag */
    char		*Aflag = NULL;		/* argument of -A flag */
    char		*Oflag = NULL;		/* argument of -O flag */
    char		*tz = NULL;		/* for -Z timezone */
    int			zflag = 0;		/* for -z (source zone) */
    int			sh = -1;		/* sample history length */
    int			vh = -1;		/* visible history length */
    int			port = -1;		/* pmtime port number */
    struct timeval	delta;
    struct timeval	origin;
    struct timeval	logStartTime;
    struct timeval	logEndTime;
    struct timeval	realStartTime;
    struct timeval	realEndTime;
    struct timeval	position;
    Tab			*tab;
    QStringList		hosts;
    QStringList		archives;
    QStringList		configs;
    QString		tzLabel;
    QString		tzString;

    __pmtimevalNow(&origin);
    __pmSetProgname(argv[0]);
    QApplication a(argc, argv);
    setupEnvironment();
    readSettings();

    fromsec(globalSettings.chartDelta, &delta);

    tab = new Tab;
    liveGroup = new GroupControl();
    archiveGroup = new GroupControl();

    while ((c = getopt(argc, argv, options)) != EOF) {
	switch (c) {

	case 'A':	/* sample alignment */
	    Aflag = optarg;
	    break;

	case 'a':
	    archives.append(optarg);
	    break;

	case 'C':
	    Cflag++;
	    break;

	case 'c':
	    configs.append(optarg);
	    break;

	case 'D':
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, optarg);
		errflg++;
	    } else
		pmDebug |= sts;
	    break;

	case 'f':
	    globalSettings.fontFamily = optarg;
	    break;

	case 'F':
	    sts = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || c < 0) {
		pmprintf("%s: -F requires a numeric argument\n", pmProgname);
		errflg++;
	    } else {
		globalSettings.fontSize = sts;
	    }
	    break;

	case 'g':
	    outgeometry = optarg;
	    break;

	case 'h':
	    hosts.append(optarg);
	    break;

	case 'H': {
	    QFile file(optarg);
	    if (!file.open(QIODevice::ReadOnly)) {
		pmprintf("Cannot open hosts files %s: %s", optarg,
			 (const char *)file.errorString().toAscii());
		errflg++;
	    } else {
		QTextStream in(&file);
		while (!in.atEnd()) {
		    globalSettings.savedHosts.append(in.readLine().trimmed());
		    globalSettings.savedHostsModified = true;
		}
		file.close();
	    }
	    break;
	}

#ifdef PM_USE_CONTEXT_LOCAL
	case 'L':		/* local context */
	    Lflag = 1;
	    break;
#endif

	case 'n':		/* alternative PMNS */
	    pmnsfile = optarg;
	    break;

	case 'o':		/* output image file */
	    outfile = optarg;
	    break;

	case 'O':		/* sample offset */
	    Oflag = optarg;
	    break;

	case 'p':		/* existing pmtime port */
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || c < 0) {
		pmprintf("%s: -p requires a numeric argument\n", pmProgname);
		errflg++;
	    }
	    break;

	case 's':		/* sample history */
	    sh = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || sh < 1) {
		pmprintf("%s: -s requires a numeric argument, larger than 1\n",
			 pmProgname);
		errflg++;
	    }
	    break;

	case 'S':		/* start run time */
	    Sflag = optarg;
	    break;

	case 't':		/* sampling interval */
	    if (pmParseInterval(optarg, &delta, &msg) < 0) {
		pmprintf("%s: cannot parse interval\n%s", pmProgname, msg);
		free(msg);
		errflg++;
	    }
	    break;

	case 'T':		/* run time */
	    Tflag = optarg;
	    break;

	case 'W':		/* white image background */
	    Wflag = 1;
	    break;

	case 'v':		/* vertical history */
	    vh = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || vh < 1) {
		pmprintf("%s: -v requires a numeric argument, larger than 1\n",
			 pmProgname);
		errflg++;
	    }
	    break;

	case 'V':		/* version */
	    printf("%s %s\n", pmProgname, pmGetConfig("PCP_VERSION"));
	    exit(0);

	case 'z':		/* timezone from host */
	    if (tz != NULL) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflg++;
	    }
	    zflag++;
	    break;

	case 'Z':		/* $TZ timezone */
	    if (zflag) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflg++;
	    }
	    tz = optarg;
	    break;

	case '?':
	default:
	    usage();
	}
    }

    if (archives.size() > 0)
	while (optind < argc)
	    archives.append(argv[optind++]);
    else {
	hosts = globalSettings.savedHosts + hosts;
	while (optind < argc)
	    hosts.append(argv[optind++]);
    }

    if (optind != argc)
	errflg++;
    if (errflg)
	usage();

    console = new Console(origin);

    //
    // Deal with user requested sample/visible points globalSettings.  These
    // (command line) override the QSettings values, for this instance
    // of pmchart.  They should not be written though, unless requested
    // later via the Settings dialog.
    //
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

    if (pmnsfile && (sts = pmLoadNameSpace(pmnsfile)) < 0) {
	pmprintf("%s: %s\n", pmProgname, pmErrStr(sts));
	pmflush();
	exit(1);
    }

    // Create all of the sources
    if (Lflag)
	liveGroup->use(PM_CONTEXT_LOCAL, QmcSource::localHost);
    for (c = 0; c < hosts.size(); c++) {
	if (liveGroup->use(PM_CONTEXT_HOST, hosts[c]) < 0)
	    hosts.removeAt(c--);
    }
    for (c = 0; c < archives.size(); c++) {
	if (archiveGroup->use(PM_CONTEXT_ARCHIVE, archives[c]) < 0)
	    archives.removeAt(c--);
    }
    if (!Lflag && hosts.size() == 0 && archives.size() == 0)
	liveGroup->createLocalContext();
    pmflush();
    console->post("Metric group setup complete (%d hosts, %d archives)",
			hosts.size(), archives.size());

    if (zflag) {
	if (archives.size() > 0)
	    archiveGroup->useTZ();
	if (hosts.size() > 0)
	    liveGroup->useTZ();
    }
    else if (tz != NULL) {
	if (archives.size() > 0)
	    archiveGroup->useTZ(QString(tz));
	if (hosts.size() > 0)
	    liveGroup->useTZ(QString(tz));
	if ((sts = pmNewZone(tz)) < 0) {
	    pmprintf("%s: cannot set timezone to \"%s\": %s\n",
		    pmProgname, (char *)tz, pmErrStr(sts));
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
    if (archives.size() > 0) {
	archiveGroup->defaultTZ(tzLabel, tzString);
	archiveGroup->updateBounds();
	logStartTime = archiveGroup->logStart();
	logEndTime = archiveGroup->logEnd();
	if ((sts = pmParseTimeWindow(Sflag, Tflag, Aflag, Oflag,
				     &logStartTime, &logEndTime,
				     &realStartTime, &realEndTime,
				     &position, &msg)) < 0) {
	    pmprintf("Cannot parse archive time window\n%s\n", msg);
	    free(msg);
	    usage();
	}
	// move position to account for initial visible points
	if (tcmp(&position, &realStartTime) <= 0)
	    for (c = 0; c < globalSettings.visibleHistory - 2; c++)
		tadd(&position, &delta);
	if (tcmp(&position, &realEndTime) > 0)
	    position = realEndTime;
    }
    else {
	liveGroup->defaultTZ(tzLabel, tzString);
	__pmtimevalNow(&logStartTime);
	logEndTime.tv_sec = logEndTime.tv_usec = INT_MAX;
	if ((sts = pmParseTimeWindow(Sflag, Tflag, Aflag, Oflag,
					&logStartTime, &logEndTime,
					&realStartTime, &realEndTime,
					&position, &msg)) < 0) {
	    pmprintf("Cannot parse live time window\n%s\n", msg);
	    free(msg);
	    usage();
	}
    }
    console->post("Timezones and time window setup complete");

    globalFont = new QFont(globalSettings.fontFamily, globalSettings.fontSize);
    if (globalSettings.fontStyle.contains("Italic"))
	globalFont->setItalic(true);
    if (globalSettings.fontStyle.contains("Bold"))
	globalFont->setBold(true);

    fileIconProvider = new FileIconProvider();
    pmchart = new PmChart;
    pmtime = new TimeControl;
    console->post("Phase1 user interface constructors complete");

    // Start pmtime process for time management
    pmtime->init(port, archives.size() == 0, &delta, &position,
		 &realStartTime, &realEndTime, tzString, tzLabel);

    pmchart->init();
    liveGroup->init(globalSettings.sampleHistory,
			globalSettings.visibleHistory,
			pmtime->liveInterval(), pmtime->livePosition());
    archiveGroup->init(globalSettings.sampleHistory,
			globalSettings.visibleHistory,
			pmtime->archiveInterval(), pmtime->archivePosition());

    //
    // We setup the pmchart tab list late, so we don't have to deal
    // with kmtime messages reaching the Tabs until we're all setup.
    //
    if (archives.size() == 0)
	tab->init(pmchart->tabWidget(), liveGroup, "Live");
    else
	tab->init(pmchart->tabWidget(), archiveGroup, "Archive");
    pmchart->tabWidget()->insertTab(tab);
    pmchart->setActiveTab(0, true);
    console->post("Phase2 user interface setup complete");

    readSchemes();
    for (c = 0; c < configs.size(); c++)
	if (!OpenViewDialog::openView((const char *)configs[c].toAscii()))
	    errflg++;
    if (errflg)
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
