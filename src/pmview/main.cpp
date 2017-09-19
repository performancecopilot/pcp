/*
 * Copyright (c) 2009, Aconex.  All Rights Reserved.
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
#include <QSettings>
#include <QTextStream>
#include <QMessageBox>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include "qed_fileiconprovider.h"
#include "qed_timecontrol.h"
#include "qed_console.h"
#include "scenegroup.h"
#include "pcpcolor.h"
#include "modlist.h"
#include "viewobj.h"
#include "main.h"

#include <sys/stat.h>
#include <iostream>
using namespace std;

int Cflag;
int Lflag;
int Wflag;
char *outgeometry;
Settings globalSettings;

float theScale = 1.0;
const int theBufferLen = 2048;
char theBuffer[theBufferLen];

// Globals used to provide single instances of classes used across pmview
SceneGroup *liveGroup;	// one metrics class group for all hosts
SceneGroup *archiveGroup;	// one metrics class group for all archives
SceneGroup *activeGroup;	// currently active metric fetchgroup
QedTimeControl *pmtime;		// one timecontrol class for pmtime
PmView *pmview;

static const char *options = "A:a:Cc:D:g:h:Ln:O:p:S:T:t:VzZ:?";

static void usage(void)
{
    pmprintf("Usage: %s [options] [sources]\n\n"
"Options:\n"
"  -A align      align sample times on natural boundaries\n"
"  -a archive    add PCP log archive to metrics source list\n"
"  -c configfile initial view to load\n"
"  -C            with -c, parse config, report any errors and exit\n"
"  -CC           like -C, but also connect to pmcd to check semantics\n"
"  -g geometry   image geometry Width x Height (WxH)\n"
"  -h host       add host to list of live metrics sources\n"
"  -L            directly fetch metrics from localhost, PMCD is not used\n"
"  -O offset     initial offset into the time window\n"
"  -p port       port name for connection to existing time control\n"
"  -S starttime  start of the time window\n"
"  -T endtime    end of the time window\n"
"  -t interval   sample interval [default: %d seconds]\n"
"  -V            display pmview version number and exit\n"
"  -Z timezone   set reporting timezone\n"
"  -z            set reporting timezone to local time of metrics source\n",
	pmProgname, (int)PmView::defaultViewDelta());
    pmflush();
    exit(1);
}

int warningMsg(const char *file, int line, const char *msg, ...)
{
    int sts = QMessageBox::Ok;
    va_list arg;
    va_start(arg, msg);

    int pos = pmsprintf(theBuffer, theBufferLen, "%s: Warning: ", pmProgname);
    pos += vsnprintf(theBuffer + pos, theBufferLen - pos, msg, arg);
    pmsprintf(theBuffer + pos, theBufferLen - pos, "\n");

    if (pmDebug) {
	QTextStream cerr(stderr);
	cerr << file << ":" << line << ": " << theBuffer << endl;
    }

    sts = QMessageBox::warning(pmview, "Warning", theBuffer, sts, sts);
    va_end(arg);

    return sts;
}

double QmcTime::secondsToUnits(double value, QmcTime::DeltaUnits units)
{
    switch (units) {
    case Milliseconds:
	value = value * 1000.0;
	break;
    case Minutes:
	value = value / 60.0;
	break;
    case Hours:
	value = value / (60.0 * 60.0);
	break;
    case Days:
	value = value / (60.0 * 60.0 * 24.0);
	break;
    case Weeks:
	value = value / (60.0 * 60.0 * 24.0 * 7.0);
	break;
    case Seconds:
    default:
	break;
    }
    return value;
}

double QmcTime::deltaValue(QString delta, QmcTime::DeltaUnits units)
{
    return QmcTime::secondsToUnits(delta.trimmed().toDouble(), units);
}

QString QmcTime::deltaString(double value, QmcTime::DeltaUnits units)
{
    QString delta;

    value = QmcTime::secondsToUnits(value, units);
    if ((double)(int)value == value)
	delta.sprintf("%.2f", value);
    else
	delta.sprintf("%.6f", value);
    return delta;
}

void writeSettings(void)
{
    QSettings userSettings;

    userSettings.beginGroup(pmProgname);
    if (globalSettings.viewDeltaModified) {
	globalSettings.viewDeltaModified = false;
	userSettings.setValue("viewDelta", globalSettings.viewDelta);
    }
    if (globalSettings.loggerDeltaModified) {
	globalSettings.loggerDeltaModified = false;
	userSettings.setValue("loggerDelta", globalSettings.loggerDelta);
    }
    if (globalSettings.viewBackgroundModified) {
	globalSettings.viewBackgroundModified = false;
	userSettings.setValue("viewBackgroundColor",
				globalSettings.viewBackgroundName);
    }
    if (globalSettings.viewHighlightModified) {
	globalSettings.viewHighlightModified = false;
	userSettings.setValue("viewHighlightColor",
				globalSettings.viewHighlightName);
    }
    if (globalSettings.gridBackgroundModified) {
	globalSettings.gridBackgroundModified = false;
	userSettings.setValue("gridBackgroundColor",
				globalSettings.gridBackgroundName);
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
    userSettings.endGroup();
}

void readSettings(void)
{
    QSettings userSettings;
    userSettings.beginGroup(pmProgname);

    //
    // Parameters related to sampling
    //
    globalSettings.viewDeltaModified = false;
    globalSettings.viewDelta = userSettings.value("viewDelta",
				PmView::defaultViewDelta()).toDouble();
    globalSettings.loggerDeltaModified = false;
    globalSettings.loggerDelta = userSettings.value("loggerDelta",
				PmView::defaultLoggerDelta()).toDouble();

    //
    // Everything colour related
    //
    globalSettings.viewBackgroundModified = false;
    globalSettings.viewBackgroundName = userSettings.value(
		"viewBackgroundColor", "black").toString();
    globalSettings.viewBackground = QColor(globalSettings.viewBackgroundName);

    globalSettings.viewHighlightModified = false;
    globalSettings.viewHighlightName = userSettings.value(
		"viewHighlightColor", "white").toString();
    globalSettings.viewHighlight = QColor(globalSettings.viewHighlightName);

    globalSettings.gridBackgroundModified = false;
    globalSettings.gridBackgroundName = userSettings.value(
		"gridBackgroundColor", "rgbi:0.15/0.15/0.15").toString();
    globalSettings.gridBackground = QColor(globalSettings.gridBackgroundName);

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
    // else: (defaults come from the pmview.ui interface specification)

    userSettings.endGroup();
}

int
genInventor(void)
{
    int sts = 0;
    char *configfile;
    QTextStream cerr(stderr);

    if (theConfigName.length()) {
	configfile = strdup((const char *)theConfigName.toLatin1());
	if (!(yyin = fopen(configfile, "r"))) {
	    pmprintf(
		"%s: Error: Unable to open configuration file \"%s\": %s\n",
		pmProgname, configfile, strerror(errno));
	    return -1;
	}
	theAltConfigName = theConfigName;
    } else {
	mode_t	cur_umask;
	char	*tmpdir = pmGetConfig("PCP_TMPFILE_DIR");
	int	fd = -1;

#if HAVE_MKSTEMP
	configfile = (char *)malloc(MAXPATHLEN+1);
	if (configfile == NULL) goto fail;
	pmsprintf(configfile, MAXPATHLEN, "%s/pcp-XXXXXX", tmpdir);
	cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	fd = mkstemp(configfile);
	umask(cur_umask);
#else
	configfile = tempnam(tmpdir, "pcp-");
	if (configfile == NULL) goto fail;
	cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	fd = open(configfile, O_RDWR|O_APPEND|O_CREAT|O_EXCL, 0600);
	umask(cur_umask);
#endif /* HAVE_MKSTEMP */

	if (fd < 0) goto fail;
	if (!(theAltConfig = fdopen(fd, "a")))
fail:
            pmprintf("%s: Warning: Unable to save configuration for "
		     "recording to \"%s\": %s\n",
		    pmProgname, configfile, strerror(errno));
	else if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "genInventor: Copy of configuration saved to "
		 << configfile << endl;

	theAltConfigName = configfile;
    }

    yyparse();

    if (theAltConfig)
	fclose(theAltConfig);

    if (pmDebug & DBG_TRACE_APPL0) {
	cerr << pmProgname << ": " << errorCount << " errors detected in "
	     << theConfigName << endl;
    }

    sts = -errorCount;

    if (rootObj != NULL) {
	rootObj->setTran(0, 0, rootObj->width(), rootObj->depth());
	
	SoSeparator *sep = new SoSeparator;
	SoTranslation *tran = new SoTranslation();
	tran->translation.setValue(rootObj->width() / -2.0, 0.0,
				   rootObj->depth() / -2.0);
	sep->addChild(tran);

	if (pmDebug & DBG_TRACE_APPL0 ||
	    pmDebug & DBG_TRACE_APPL1 ||
	    pmDebug & DBG_TRACE_APPL2) {
	    SoBaseColor *col = new SoBaseColor;
	    col->rgb.setValue(1.0, 0.0, 0.0);
	    sep->addChild(col);
	    SoCube * cube  = new SoCube;
	    cube->width = 10;
	    cube->depth = 5.0;
	    cube->height = 25;
	    sep->addChild(cube);
	}

	sep->addChild(rootObj->root());
	theModList->setRoot(sep);
    }

    if ((ViewObj::numModObjects() == 0 || theModList->size() == 0) && 
	 elementalNodeList.getLength() == 0) {
	pmprintf("%s: No valid modulated objects in the scene\n",
		 pmProgname);
	sts--;
    }
    else if (sts < 0) {
	pmprintf("%s: Unrecoverable errors in the configuration file %s\n",
	    pmProgname, (const char *)theConfigName.toLatin1());
    }

    return sts;
}

int
main(int argc, char **argv)
{
    int			c, sts;
    int			errflg = 0;
    char		*msg;

    QedApp a(argc, argv);
    readSettings();

    liveGroup = new SceneGroup();
    archiveGroup = new SceneGroup();

    while ((c = a.getopts(options)) != EOF) {
	switch (c) {

	case 'C':
	    Cflag++;
	    break;

	case 'g':
	    outgeometry = optarg;
	    break;

	case 'c':
	    theConfigName = optarg;
	    break;

	case '?':
	default:
	    usage();
	}
    }

    a.startconsole();

    if (a.my.archives.size() > 0)
	while (optind < argc)
	    a.my.archives.append(argv[optind++]);
    else
	while (optind < argc)
	    a.my.hosts.append(argv[optind++]);

    if (optind != argc)
	errflg++;
    if (errflg)
	usage();

    if (a.my.pmnsfile && (sts = pmLoadNameSpace(a.my.pmnsfile)) < 0) {
	pmprintf("%s: %s\n", pmProgname, pmErrStr(sts));
	pmflush();
	exit(1);
    }

    // Create all of the sources
    if (a.my.Lflag) {
	liveGroup->use(PM_CONTEXT_LOCAL, QmcSource::localHost);
	Lflag = 1;
    }
    for (c = 0; c < a.my.hosts.size(); c++) {
	if (liveGroup->use(PM_CONTEXT_HOST, a.my.hosts[c]) < 0)
	    a.my.hosts.removeAt(c);
    }
    for (c = 0; c < a.my.archives.size(); c++) {
	if (archiveGroup->use(PM_CONTEXT_ARCHIVE, a.my.archives[c]) < 0)
	    a.my.archives.removeAt(c);
    }
    if (!a.my.Lflag && a.my.hosts.size() == 0 && a.my.archives.size() == 0)
	liveGroup->createLocalContext();
    pmflush();
    console->post("Metric group setup complete (%d hosts, %d archives)",
			a.my.hosts.size(), a.my.archives.size());

    if (a.my.zflag) {
	if (a.my.archives.size() > 0)
	    archiveGroup->useTZ();
	if (a.my.hosts.size() > 0)
	    liveGroup->useTZ();
    }
    else if (a.my.tz != NULL) {
	if (a.my.archives.size() > 0)
	    archiveGroup->useTZ(QString(a.my.tz));
	if (a.my.hosts.size() > 0)
	    liveGroup->useTZ(QString(a.my.tz));
	if ((sts = pmNewZone(a.my.tz)) < 0) {
	    pmprintf("%s: cannot set timezone to \"%s\": %s\n",
		    pmProgname, (char *)a.my.tz, pmErrStr(sts));
	    pmflush();
	    exit(1);
	}
    }

    //
    // Choose which View will be displayed initially - archive/live.
    // If any archives given on command line, we go Archive mode;
    // otherwise Live mode wins.  Our initial pmtime connection is
    // set in that mode too.  Later we'll make a second connection
    // in the other mode (and only "on-demand").
    //
    if (a.my.archives.size() > 0) {
	activeGroup = archiveGroup;
	archiveGroup->defaultTZ(a.my.tzLabel, a.my.tzString);
	archiveGroup->updateBounds();
	a.my.logStartTime = archiveGroup->logStart();
	a.my.logEndTime = archiveGroup->logEnd();
	if ((sts = pmParseTimeWindow(a.my.Sflag, a.my.Tflag,
				     a.my.Aflag, a.my.Oflag,
				     &a.my.logStartTime, &a.my.logEndTime,
				     &a.my.realStartTime, &a.my.realEndTime,
				     &a.my.position, &msg)) < 0) {
	    pmprintf("Cannot parse archive time window\n%s\n", msg);
	    free(msg);
	    usage();
	}
    }
    else {
	activeGroup = liveGroup;
	liveGroup->defaultTZ(a.my.tzLabel, a.my.tzString);
	gettimeofday(&a.my.logStartTime, NULL);
	a.my.logEndTime.tv_sec = a.my.logEndTime.tv_usec = INT_MAX;
	if ((sts = pmParseTimeWindow(a.my.Sflag, a.my.Tflag,
					a.my.Aflag, a.my.Oflag,
					&a.my.logStartTime, &a.my.logEndTime,
					&a.my.realStartTime, &a.my.realEndTime,
					&a.my.position, &msg)) < 0) {
	    pmprintf("Cannot parse live time window\n%s\n", msg);
	    free(msg);
	    usage();
	}
    }
    console->post("Timezones and time window setup complete");

    pmview = new PmView;
    pmtime = new QedTimeControl;
    fileIconProvider = new QedFileIconProvider();
    console->post("Phase1 user interface constructors complete");

    // Start pmtime process for time management
    pmtime->init(a.my.port, a.my.archives.size() == 0, &a.my.delta,
		 &a.my.position, &a.my.realStartTime, &a.my.realEndTime,
		 a.my.tzString, a.my.tzLabel);

    pmview->init();
    liveGroup->init(pmtime->liveInterval(), pmtime->livePosition());
    archiveGroup->init(pmtime->archiveInterval(), pmtime->archivePosition());
    console->post("Phase2 user interface setup complete");

    PCPColor::initClass();
    theModList = new ModList(pmview->viewer(), &PmView::selectionCB, NULL, NULL);
    if (genInventor() < 0) {
	pmflush();
	exit(1);
    }

    if (Cflag)	// done with -c config, quit
	return 0;

    if (pmview->view(false, 0, 1, 0, M_PI / 4.0, theGlobalScale) == false) {
	pmflush();
	exit(1);
    }

    pmview->viewer()->viewAll();
    pmview->enableUi();
    pmview->show();
    console->post("Top level window shown");

    a.connect(&a, SIGNAL(lastWindowClosed()), pmview, SLOT(quit()));
    return a.exec();
}
