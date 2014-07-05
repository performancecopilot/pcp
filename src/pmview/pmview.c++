/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
 */


#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>

#include "pmapi.h"

#include <Xm/Xm.h>
#include <Vk/VkApp.h>
#include <Vk/VkErrorDialog.h>

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtViewer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>

#ifdef PCP_DEBUG
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCube.h>
#endif

#include "BarObj.h"
#include "GridObj.h"
#include "StackObj.h"
#include "LabelObj.h"
#include "ModList.h"
#include "DefaultObj.h"

#include "App.h"
#include "View.h"
#include "ModList.h"
#include "Record.h"
#include "PCPColor.h"

#ifndef APPNAME
#define APPNAME	"PmView+"
#endif

void usage();

OMC_String	theConfigName;
OMC_String	theAltConfigName;
char		*theRecordConfig = NULL;
char		*theRecAddConfig = NULL;
FILE		*theConfigFile = (FILE *)0;
FILE		*theAltConfig = (FILE *)0;
float		theGlobalScale = 1.2;

char	**frontend_argv;
int	frontend_argc;

// Hack to force C++ compiler to generate OMC_ArgList. *sigh*
static OMC_Args	_args;

static XrmOptionDescRec theCmdLineOptions[] = {
    { "-A", "*pmAlignment",	XrmoptionSkipArg,	NULL },
    { "-a", "*pmArchive",	XrmoptionSkipArg,	NULL },
    { "-c", "*pmConfigFile",	XrmoptionSkipArg,	NULL },
    { "-D", "*pmDebug",		XrmoptionSkipArg,      	NULL },
    { "-h", "*pmHost",		XrmoptionSkipArg,	NULL },
    { "-l", "*pmDetail",	XrmoptionSkipArg,	NULL },
    { "-n", "*pmNamespace",	XrmoptionSkipArg,	NULL },
    { "-O", "*pmOffset",	XrmoptionSkipArg,	NULL },
    { "-p", "*pmPortName",	XrmoptionSkipArg,	NULL },
    { "-R", "*pmLogConfig",	XrmoptionSkipArg,	NULL },
    { "-r", "*pmAddConfig",	XrmoptionSkipArg,	NULL },
    { "-S", "*pmStart",		XrmoptionSkipArg,	NULL },
    { "-t", "*pmInterval",	XrmoptionSkipArg,	NULL },
    { "-T", "*pmEnd",		XrmoptionSkipArg,	NULL },
    { "-V", "*pmVersion",	XrmoptionSkipArg,	NULL },
    { "-Z", "*pmTimezone",	XrmoptionSkipArg,	NULL },
    { "-title", "*title",	XrmoptionSepArg,	NULL },
};

int
parseArgs()
{
    int		c;
    int		sts = 0;

    while ((c = theView->parseArgs()) != EOF) {
	switch (c) {
	case 'c':
	    theConfigName = optarg;
	    break;

	case 'R':
	    if (theRecordConfig != NULL || theRecAddConfig != NULL) {
		pmprintf("%s: Error: At most one of -R and/or -r option allowed\n",
			 pmProgname);
		sts--;
	    }
	    else
		theRecordConfig = optarg;
	    break;

	case 'r':
	    if (theRecordConfig != NULL || theRecAddConfig != NULL) {
		pmprintf("%s: Error: At most one of -R and/or -r option allowed\n",
			 pmProgname);
		sts--;
	    }
	    else
		theRecAddConfig = optarg;
	    break;
	default:
	    sts--;
	}
    }

    if (sts == 0) {
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "parseaArgs: Successful, configName = " << theConfigName
		 << endl;
#endif
	return theView->status();
    }
    else
	return sts;
}

/*ARGSUSED*/
static OMC_Bool
genConfig(FILE *fp)
{
    ifstream infile(theAltConfigName.ptr());
    if (!infile) {
	return OMC_false;
	/*NOTREACHED*/
    }

    while (infile.getline(theBuffer, theBufferLen))
	fprintf(fp, "%s\n", theBuffer);

    return OMC_true;
}

/*ARGSUSED*/
static
void
cleanup(int status)
{
    if (theAltConfigName.length() > 0 && theAltConfig ) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "cleanup: removing " << theAltConfigName << endl;
#endif
        unlink(theAltConfigName.ptr());
	theAltConfigName = "";
	theAltConfig = NULL;
    }
}


static int
genInventor()
{
    extern ViewObj	*rootObj;
    extern int errorCount;
    extern int yyparse(void);

    int		sts = 0;

    if (theConfigName.length()) {
	extern FILE	*yyin;

	if ( (yyin  = fopen(theConfigName.ptr(), "r")) == NULL ) {
	    pmprintf(
		"%s: Error: Unable to open configuration file \"%s\": %s\n",
		pmProgname, theConfigName.ptr(), strerror(errno));
	    return -1;
	}
	theAltConfigName = theConfigName;
    } else {
        theAltConfigName = mktemp(strdup("/tmp/pmview.XXXXXX"));

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "genInventor: Copy of configuration saved to "
		 << theAltConfigName << endl;
#endif

	if ((theAltConfig = fopen(theAltConfigName.ptr(), "w")) == NULL) {
            pmprintf("%s: Warning: Unable to save configuration for "
		     "recording to \"%s\": %s\n",
                     pmProgname, theAltConfigName.ptr(), strerror(errno));
        }
    }

    yyparse();

    if ( theAltConfig )
	fclose (theAltConfig);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	cerr << pmProgname << ": " << errorCount << " errors detected in "
	     << theConfigName << endl;
    }
#endif

    sts = -errorCount;

    if (rootObj != NULL) {
	rootObj->setTran(0, 0, rootObj->width(), rootObj->depth());
	
	SoSeparator *sep = new SoSeparator;
	SoTranslation *tran = new SoTranslation();
	tran->translation.setValue(rootObj->width() / -2.0, 0.0,
				   rootObj->depth() / -2.0);
	sep->addChild(tran);

#ifdef PCP_DEBUG
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
#endif

	sep->addChild(rootObj->root());
	theModList->setRoot(sep);
    }

    if ((ViewObj::numModObjects() == 0 || theModList->length() == 0) && 
	 elementalNodeList.getLength() == 0) {
	pmprintf("%s: No valid modulated objects in the scene\n",
		 pmProgname);
	sts--;
    }
    else if (sts < 0) {
	pmprintf("%s: Unrecoverable errors in the configuration file %s\n",
	    pmProgname, theConfigName.ptr());
    }

    return sts;
}

// Exit properly when interrupted, allows tools like pixie to work properly
#if defined(IRIX5_3)
static void
catch_int(...)
#else
/*ARGSUSED*/
static void
catch_int(int sig)
#endif
{
    pmflush();
    theApp->terminate(1);
    /*NOTREACHED*/
}

int 
main (int argc, char *argv[])
{
    OMC_String	verStr;
    OMC_String	flags = theDefaultFlags;

    if (argc == 2 && strcmp(argv[1], "-?") == 0) {
	/* fast track the Usage message if that is all the punter wants */
	usage();
	pmflush();
	exit(0);
    }

#if defined(__linux__)
    /* avoid stupid locale warnings on Linux */
    if (getenv("LC_ALL") == NULL)
	putenv("LC_ALL=C");
#endif

    flags.append("c:R:r:");

    __pmSetAuthClient();

    if (INV_setup(APPNAME, &argc, argv, theCmdLineOptions, 
		  XtNumber(theCmdLineOptions), cleanup) < 0) {
	usage();
	pmflush();
	exit(1);
    }


    signal(SIGINT, catch_int);

    // Create the top level windows
    theView = new INV_View(argc, argv, flags, genConfig, NULL,
			   OMC_true, OMC_false, OMC_true);

    if (theView->status() < 0) {
	if (theView->checkConfigOnly() == OMC_false)
	    usage();
	pmflush();
	theApp->terminate(1);
    }

    //
    // Initialize custom Inventor Widgets
    //
    PCPColor::initClass();

    theModList = new INV_ModList(theView->viewer(), 
			     	 &INV_View::selectionCB,
				 NULL, NULL);

    if (parseArgs() < 0) {
	if (theView->checkConfigOnly() == OMC_false)
	    usage();
	pmflush();
	theApp->terminate(1);
    }

    if (theRecordConfig != NULL)
	theView->setRecordConfig(theRecordConfig);
    if (theRecAddConfig != NULL)
	theView->setRecAddConfig(theRecAddConfig);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "main: Args and config parsed, about to show scene" << endl;
#endif

    theView->parseConfig(genInventor);
    if (theView->status() < 0) {
    	pmflush();
	theApp->terminate(1);
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	cerr << "main: Modulated Object List:" << endl;
        cerr << *theModList << endl;
    }
#endif

    pmflush();
    if (theView->checkConfigOnly())
	theApp->terminate(0);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "main: Displaying window" << endl;
#endif

    if (theView->view(OMC_false, 0, 1, 0, M_PI / 4.0, theGlobalScale) ==
	OMC_false) {
	pmflush();
	theApp->terminate(1);
    }

    // Setup for launch via desktop on login/logout
    if (frontend_argc > 0) {
	XSetCommand(theApp->display(),
		    XtWindow(theView->baseWidget()),
		    frontend_argv, frontend_argc);
	for (int i=0; i < frontend_argc; i++)
	    free(frontend_argv[i]);
	free(frontend_argv);
    }
    else {
	/* set things up so that no attempt is made to restart either */
	/* pmview or the frontend script, if no cmdline args are found */
	Atom command = XInternAtom(theApp->display(), "WM_COMMAND", True);
	if (command != None) {
	    XDeleteProperty(theApp->display(),
		XtWindow(theView->baseWidget()), command);
	}
    }

    // Specify product information
    verStr.append("Performance Co-Pilot, Version ");
    verStr.append(PCP_VERSION);
    verStr.append("\n\n\nEmail Feedback: pcp-info@sgi.com");
    theApp->setVersionString(verStr.ptr());

    // Forked processes (ie. from the launch menu) should not keep the X
    // file descriptor open
    fcntl(ConnectionNumber(theApp->display()), F_SETFD, 1);

    theView->viewer()->viewAll();

    theApp->run();
}

void
usage()
{
    pmprintf(
"Usage: pmview [options]\n\
\n\
Options:\n\
  -A align                align sample times on natural boundaries\n\
  -a archive[,archive...] metric sources are PCP log archive(s)\n\
  -C                      check configuration file and exit\n\
  -c configfile           configuration file\n\
  -h host                 metrics source is PMCD on host\n\
  -n pmnsfile             use an alternative PMNS\n\
  -O offset               initial offset into the time window\n\
  -p port                 port name for connection to existing time control\n\
  -R logconfig            use an alternative pmlogger(1) config when recording\n\
  -r addconfig            append this pmlogger(1) config when recording\n\
  -S starttime            start of the time window\n\
  -T endtime              end of the time window\n\
  -t interval             sample interval [default 2.0 seconds]\n\
  -x version              use pmlaunch(5) version [default 2.0]\n\
  -Z timezone             set reporting timezone\n\
  -z                      set reporting timezone to local time of metrics source\n\n\
  -display  display-string\n\
  -geometry geometry-string\n\
  -name     name-string\n\
  -xrm      resource [-xrm ...]\n");
}
