/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 */


#include <stdlib.h>
#include <syslog.h>
#if defined(sgi)
#include <ulocks.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/Label.h>

#include <Inventor/SoPickedPoint.h>
#include <Inventor/actions/SoBoxHighlightRenderAction.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/sensors/SoTimerSensor.h>

#ifdef PCP_DEBUG
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#endif

#include <Vk/VkApp.h>
#include <Vk/VkInput.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkHelpPane.h>
#include <Vk/VkResource.h>

#ifdef IRIX5_3
#include "VkPixmap.h"
#else
#include <Vk/VkPixmap.h>
#endif

#if defined(sgi)
#include <helpapi/HelpBroker.h>
#endif

#include "tv.h"
#include "Inv.h"
#include "App.h"
#include "Metric.h"
#include "View.h"
#include "Window.h"
#include "Form.h"
#include "ModList.h"
#include "Launch.h"

#define _VCRMODE(x) (x & __PM_MODE_MASK)

static int	theAflag = 0;		// 0 ctime, 1 +offset, 2 -offset
static char	*theAtime = (char *)0;	// tm from -A flag
static int      theSflag = 0;		// 0 ctime, 1 +offset, 2 -offset
static char	*theStime = (char *)0;	// tm from -S flag
static int      theOflag = 0;		// 0 ctime, 1 +offset, 2 -offset
static char	*theOtime = (char *)0;	// tm from -S flag
static int      theTflag = 0;		// 0 ctime, 1 +offset, 2 -offset
static char	*theTtime = (char *)0;	// tm from -T flag
static int      thezflag = 0;		// for -z

INV_View	*theView = NULL;

INV_View::~INV_View()
{
}

INV_View::INV_View(int argc, char **argv, const OMC_String &flags, 
		   INV_Record::ToolConfigCB toolCB, 
		   INV_Record::LogConfigCB logCB,
		   OMC_Bool sourceFlag, 
		   OMC_Bool hostFlag,
		   OMC_Bool modConfig)
: INV_Window(theAppName.ptr(), NULL, 0),		
  _sts(0),
  _argc(argc),
  _argv(argv),
  _argFlags(flags),
  _sourceFlag(sourceFlag),
  _hostFlag(hostFlag),
  _checkConfigFlag(OMC_false),
  _pmnsFile(),
  _launchVersion(),
  _root(new SoSeparator),
  _drawStyle(new SoDrawStyle),
  _text(),
  _prevText("foo"),
  _timeMode(0),
  _timeFD(-1),
  _timePort(),
  _timeZone(),
  _record(toolCB, modConfig, logCB, recordStateCB, (void *)this)
{
    int	c;

//
// Initialise
//

    memset(&_timeState, 0, sizeof(_timeState));
    setTitle("title");

//
// Build Scene Graph
//

    _root->ref();

    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    camera->orientation.setValue(SbVec3f(1, 0, 0), -M_PI/6.0);
    _root->addChild(camera);

    _drawStyle->style.setValue(SoDrawStyle::FILLED);
    _root->addChild(_drawStyle);

//
// Parse args for namespace
//

    for (c = 0; c < _argc; c++)
	if (strcmp(_argv[c], "-n") == 0)
	    break;

    if (c < _argc) {
	if (c < _argc - 1) {
	    _pmnsFile = _argv[c+1];
	    _sts = pmLoadNameSpace(_pmnsFile.ptr());
	    if (_sts < 0)
	    	pmprintf("%s: Error: Loading namespace \"%s\": %s\n",
			 pmProgname, _pmnsFile.ptr(), pmErrStr(_sts));
	}
	else
	    pmprintf("%s: Error: -n expects a file name\n", pmProgname);
    }
}

SoXtViewer *
INV_View::viewer()
{
    return _form->_viewer;
}

OMC_Bool
INV_View::view(OMC_Bool showAxis, 
	       float xAxis, float yAxis, float zAxis, float angle, 
	       float scale)
{
    char	*sval;

    if (theModList->length() == 0) {
	INV_warningMsg(_POS_, "No modulated objects in scene");
    }

//
// Setup record mode
//

    if (theSource.isLive() == OMC_false)
	hideRecordButton();

//
// Set up and connect to time controls
//

    if (timeConnect(OMC_false) < 0) {
	return OMC_false;
	/*NOTREACHED*/
    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_View::view: time controls started" << endl;
#endif

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	cerr << "INV_View::view: Calling pmTimeConnect with:" << endl;
	cerr << "start = " << _timeState.start.tv_sec << '.'
	     << _timeState.start.tv_usec << ", finish = "
	     << _timeState.finish.tv_sec << '.' << _timeState.finish.tv_usec
	     << ", fetch = " << _timeState.position.tv_sec << '.'
	     << _timeState.position.tv_usec << ", delta = "
	     << _timeState.delta << endl;
    }
#endif

    VkInput *cmdInput = new VkInput();
    cmdInput->addCallback(VkInput::inputCallback, 
			  (VkCallbackFunction)&INV_View::timeCommandCB,
			  &_timeState);
    cmdInput->attach(_timeFD, XtInputReadMask);

    if (theSource.isLive() == 0 && checkConfigOnly() == 0) {
    	pmTimeShowDialog(1);
    }

//
// Set up remainder of scene graph
//

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_View::view: initialising viewer" << endl;
#endif

    _root->addChild(theModList->root());

    _form->_viewer->setSceneGraph(_root);
    _form->_viewer->setAutoRedraw(True);
    _form->_viewer->setTitle(pmProgname);
    if (showAxis)
	_form->_viewer->setFeedbackVisibility(True);

    SbBool smooth = TRUE;
    int passes = 1;
    sval = VkGetResource("antiAliasSmooth", XmRString);
    if (sval != NULL && strcmp(sval, "default") != 0 && strcasecmp(sval, "false") == 0)
	smooth = FALSE;
    sval = VkGetResource("antiAliasPasses", XmRString);
    if (sval != NULL && strcmp(sval, "default"))
	passes = atoi(sval);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_View::view: antialiasing set to smooth = "
	     << (smooth == TRUE ? "true" : "false")
	     << ", passes = " << passes << endl;
#endif

    if (passes > 1)
	_form->_viewer->setAntialiasing(smooth, atoi(sval));

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_View::view: displaying window" << endl;
#endif

    _form->_viewer->viewAll();

    if (angle != 0.0 || scale != 0.0) {
	SoTransform *tran = new SoTransform();
	if (angle != 0.0)
	    tran->rotation.setValue(SbVec3f(xAxis, yAxis, zAxis), 
				    angle);
	if (scale != 0.0)
	    tran->scaleFactor.setValue(scale, scale, scale);

	theModList->root()->insertChild(tran, 0);
    }

    INV_View::render((RenderOptions)(INV_View::inventor | 
				     INV_View::metricLabel), 0);
    _form->_viewer->saveHomePosition();

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0 || 
	pmDebug & DBG_TRACE_APPL1 ||
	pmDebug & DBG_TRACE_APPL2) {

	OMC_String fileName = pmProgname;
	fileName.append(".iv");
	SoOutput outfile;
	outfile.openFile(fileName.ptr());
	SoWriteAction write(&outfile);
	write.apply(_root);
	outfile.closeFile();

	cerr << "INV_View: view: Inventor ASCII file dumped to "
	     << fileName << endl;
    }
#endif

    if (!checkConfigOnly())
	show();

    return OMC_true;
}

int
INV_View::timeConnect(OMC_Bool reconnect)
{
    char	*str;

    if (reconnect == OMC_true) {
	_timePort = "";
	_timeState.showdialog = 1;
	close(_timeFD);
	int sts = pmTimeDisconnect();
	if (sts < 0)
	    INV_warningMsg(_POS_, "Unable to disconnect time controls: %s",
			   pmErrStr(sts));
    }
    else 
	_timeState.showdialog = 0;

    if (_timePort.length() == 0) {
	reconnect = OMC_true;
	_timePort = "/tmp/pmview.XXXXXX";
	str = mktemp(_timePort.ptr());
	if (str == (char *)0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		cerr << "INV_View::timeConnect: mktemp failed" << endl;
#endif
	    _timePort = "/tmp/pmview.";
	    _timePort.appendInt((int)getpid());
	}
    }

//
// Check that the given time port actually exists and is a socket.
//

    else {
	struct stat statbuf;
	if ((_sts = stat(_timePort.ptr(), &statbuf)) < 0) {
	    _sts = -errno;
	    INV_errorMsg(_POS_, "cannot access time control port \"%s\": %s",
			 _timePort.ptr(), pmErrStr(_sts));
        }
        else if ((statbuf.st_mode & S_IFSOCK) != S_IFSOCK) {
	    INV_errorMsg(_POS_, "time control port \"%s\" is not a socket",
			 _timePort.ptr());
	    _sts = -1;
	}
    }

    if (_sts < 0)
	return _sts;

    if (theSource.isLive()) {
	if (reconnect == OMC_true)
	    _timeMode = PM_TCTL_MODE_NEWMASTER | PM_TCTL_MODE_HOST;
	else
	    _timeMode = PM_TCTL_MODE_MASTER | PM_TCTL_MODE_HOST;
    }
    else {
	if (reconnect == OMC_true)
	    _timeMode = PM_TCTL_MODE_NEWMASTER | PM_TCTL_MODE_ARCHIVE;
	else
	    _timeMode = PM_TCTL_MODE_MASTER | PM_TCTL_MODE_ARCHIVE;
    }

    _pmtvTimeVal l_ival(_interval);
    l_ival.getXTB(_timeState.delta, _timeState.vcrmode);
    
    _timeFD = pmTimeConnect(_timeMode, _timePort.ptr(), &_timeState);
    if (_timeFD < 0) {
	INV_errorMsg(_POS_, "Connecting to time controls: %s",
		     pmErrStr(_timeFD));
	_sts = _timeFD;
	return _sts;
    }

    _sts = theSource.sendTimezones();
    if (_sts < 0) {
	INV_errorMsg(_POS_, "Sending timezones to time controls: %s",
		     pmErrStr(_sts));
    }

    return _sts;
}

void
INV_View::render(RenderOptions options, time_t theTime)
{
    _form->_viewer->setAutoRedraw(False);

    if (options & INV_View::fetch)
	OMC_Metric::fetch();

    if (options & INV_View::metrics)
	theModList->refresh(OMC_true);

    if (options & INV_View::inventor)
	_form->_viewer->render();

    if (options & INV_View::metricLabel) {
	theModList->infoText(_text);
	if (_text != _prevText) {
	    _prevText = _text;
	    if (_text.length() == 0)
		XmTextSetString(_form->_label, "\n");
	    else {
		XmTextSetString(_form->_label, _text.ptr());
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL1)
		    cerr << "INV_View::render: metricLabel text \"" << _text.ptr() << "\"" << endl;
#endif
	    }
	}
#ifdef PCP_DEBUG
	else
	if ((pmDebug & DBG_TRACE_APPL1) && _text.length() > 0)
	    cerr << "INV_View::render: metricLabel text is UNCHANGED from \"" << _text.ptr() << "\"" << endl;
#endif
    }

    if (options & INV_View::timeLabel) {
	char buf[32];
	char tzbuf[32];
        sprintf(buf, "%.24s", pmCtime(&theTime, tzbuf));
	XmTextSetString(_form->_timeLabel, buf);
    }
    
    _form->_viewer->setAutoRedraw(True);
}

#define absDelta(d) ((d < 0) ? -d : d)

void
INV_View::updateCB(void *obj, SoSensor *)
{
    INV_View *me = (INV_View *)obj;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_View::updateCB: updating" << endl;
#endif

    me->render((RenderOptions)(INV_View::fetch | INV_View::metrics | 
			       INV_View::inventor | INV_View::metricLabel), 0);
}

int
INV_View::launch(const OMC_Args& args)
{
    char	*defsourcename = NULL;
    char	*defsourcetype = NULL;
    char	defaulttype[4];
    char	starttime[32];
    char	endtime[32];
    char	offset[32];
    char	tzbuf[32];
    time_t	t;
    pid_t	pid;
    int		fds[2];
    int		sts;
    OMC_String	source;
    INV_Launch	launch(_launchVersion);

    theModList->launch(launch);

    t = _timeState.start.tv_sec;
    sprintf(starttime, "@%.24s", pmCtime(&t, tzbuf));
    t = _timeState.position.tv_sec;
    sprintf(offset, "@%.24s", pmCtime(&t, tzbuf));

    if (theSource.isLive())
	endtime[0] = '\0';
    else {
	t = _timeState.finish.tv_sec;
	sprintf(endtime, "@%.24s", pmCtime(&t, tzbuf));
    }

    if (theSource.useDefault() >= 0) {
	OMC_Context	*context = theSource.which();

	source = context->source();
	if (context->type() == PM_CONTEXT_HOST) {
	    defsourcename = source.ptr();
	    strcpy(defaulttype, "h");
	    defsourcetype = &defaulttype[0];
	}
	else if (context->type() == PM_CONTEXT_ARCHIVE) {
	    defsourcename = source.ptr();
	    strcpy(defaulttype, "a");
	    defsourcetype = &defaulttype[0];
	}
    }

    launch.setDefaultOptions((int)((_timeState.delta / 1000.0) + 0.5), 
			     pmDebug, _pmnsFile.ptr(), pmTimeGetPort(),
			     starttime, endtime, offset, _timeState.tz,
			     defsourcetype, defsourcename,
			     theModList->selections());

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
    	cerr << endl << "INV_View::launch for " << args[0] << endl;
    	cerr << launch << endl;
    }
#endif

    sts = pipe(fds);
    if (sts)
	return -errno;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	cerr << "INV_View::launch: About to launch " << args[0] << endl;
    }
#endif

    if ((pid = fork()) < 0) {
	close(fds[0]);
	close(fds[1]);
	return -errno;
    }
    else if (pid == 0) {      // first child
	if ((pid = fork()) < 0) {
	    close(fds[0]);
	    close(fds[1]);
	    return -errno;
	}
	else if (pid > 0)
	    exit(0);
	
	// this is the second child - its parent is becomes init as soon as
	// its real parent exits via the statement above.
	// we now continue executing, knowing that when we exit init will
	// reap our status.
	
	close(fds[1]);
	sts = dup2(fds[0], STDIN_FILENO);
	if (sts < 0) {
	    __pmNotifyErr(LOG_ERR, "INV_View::launch: cannot duplicate file des: %s",
			  strerror(errno));
	    exit(1);
	}
	
	setsid();
	execv(args[0], args.argv());
	__pmNotifyErr(LOG_ERR, "INV_View::launch: cannot launch %s: %s",
		      args[0], strerror(errno));
	exit(127);
    }
    else if (pid == (pid_t)-1) {
	close(fds[0]);
	close(fds[1]);
	return -errno;
    }
    
    close(fds[0]);
    launch.output(fds[1]);
    close(fds[1]);

    return 0;
}

void
INV_View::selectionCB(INV_ModList *, OMC_Bool redraw)
{
    if (redraw) {
	theView->render((RenderOptions)(INV_View::metricLabel | 
					INV_View::inventor), 0);
    }
    else
	theView->render(INV_View::metricLabel, 0);
}

//
// If the VCR direction changes, or the position has changed, then prefetch
// to get the rate metric right. However, don't prefetch before the real
// start of the archives. 
//

void
INV_View::changeDir()
{
    static int		isLive = theSource.isLive();
    struct timeval	newPos;
    int			dir;
    int			l_vcrmode = _VCRMODE(_timeState.vcrmode);
    
    if (l_vcrmode == PM_TCTL_VCRMODE_STOP)
	dir = 0;
    else if (_timeState.delta < 0)
	dir = -1;
    else
	dir = 1;

    const char** map = pmTimeGetStatePixmap(l_vcrmode, dir, 
					    isLive, _record.active());
    VkSetHighlightingPixmap(_form->_vcr, (char **)map);

    if (isLive)
	return;

    newPos.tv_sec = _timeState.position.tv_sec - 
	(time_t)(_timeState.delta / 1000.0);
    newPos.tv_usec = _timeState.position.tv_usec -
	(time_t)((_timeState.delta % 1000) * 1000);

    if (newPos.tv_usec < 0) {
	newPos.tv_sec--;
	newPos.tv_usec += 10000000;
    }

    if (newPos.tv_usec > 9999999) {
	newPos.tv_sec++;
	newPos.tv_usec -= 10000000;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_TIMECONTROL) {
	char tzbuf[32];
	time_t t = newPos.tv_sec;
	cerr << "changeDir: " << __pmTimeStr(&(_timeState))
	     << ", new pos = " << pmCtime(&t, tzbuf)
	     << endl;
    }
#endif

    OMC_Metric::setArchiveMode((_timeState.vcrmode & 0xffff0000) | PM_MODE_INTERP, &newPos, 
			       _timeState.delta);
    OMC_Metric::fetch();
}

void
INV_View::timeCommandCB(VkCallbackObject *obj, void *, void *)
{
    enum Dir { forward, backward, unknown };

    static int			entered = 0;
    static int			isLive = theSource.isLive();
    static int			lastState = PM_TCTL_VCRMODE_STOP;
    static int			lastDir = unknown;

    VkInput			*channel = (VkInput *)obj;
    int				newDir;
    int				cmd;
    int				dir;

    if (entered) {
        // already processing a command!
	cerr << "INV_View::timeCommandCB: entered = 1" << endl;
        return;
    }

    entered = 1;

    cmd = theSource.sendBounds(theView->_timeState);
    if (cmd < 0) {
	INV_warningMsg(_POS_, "Unable to update time control bounds: %s\n",
		       pmErrStr(cmd));
    }

    cmd = pmTimeRecv(&(theView->_timeState));

    if (cmd < 0) {
	cerr << "Time Control dialog terminated: " << pmErrStr(cmd) 
	     << endl << "Sorry." << endl;
	theApp->terminate(1);
    }

    if (theView->_timeState.delta < 0)
	newDir = backward;
    else
	newDir = forward;

    switch (cmd) {
    case PM_TCTL_SET:		// new position and delta
	if (_VCRMODE(theView->_timeState.vcrmode) != PM_TCTL_VCRMODE_DRAG && !isLive) {
	    theView->changeDir();
	    if (lastDir == newDir)
		theView->render(INV_View::all, 
				theView->_timeState.position.tv_sec);
	    else
		theView->render((INV_View::RenderOptions)
				(INV_View::fetch | 
				 INV_View::metrics |
				 INV_View::timeLabel), 
				theView->_timeState.position.tv_sec);
	}
	else
	    theView->render(INV_View::timeLabel, 
			    theView->_timeState.position.tv_sec);
	break;

    case PM_TCTL_STEP:		// new position
	if (lastDir != newDir)
	    theView->changeDir();
	theView->render(INV_View::all, 
			theView->_timeState.position.tv_sec);
	pmTimeSendAck(&theView->_timeState.position);
	break;

    case PM_TCTL_SKIP:		// new position, but with no fetch
	theView->render(INV_View::timeLabel, 
			theView->_timeState.position.tv_sec);
	break;

    case PM_TCTL_VCRMODE:            // new indicator state
    {
	int l_vcrmode = _VCRMODE(theView->_timeState.vcrmode);

	if (l_vcrmode == PM_TCTL_VCRMODE_STOP)
	    dir = 0;
	else if (theView->_timeState.delta < 0)
	    dir = -1;
	else
	    dir = 1;

	const char** map = pmTimeGetStatePixmap(l_vcrmode, 
						dir, isLive,
						theView->_record.active());
	VkSetHighlightingPixmap(theView->_form->_vcr, (char **)map);

	if (lastState == PM_TCTL_VCRMODE_DRAG && 
	    l_vcrmode != PM_TCTL_VCRMODE_DRAG) {
	    theView->changeDir();
	    theView->showView();
	}
	else if ( lastState != PM_TCTL_VCRMODE_DRAG &&
		 l_vcrmode == PM_TCTL_VCRMODE_DRAG) {
	    theView->hideView();
	    theView->render(INV_View::inventor, 
			    theView->_timeState.position.tv_sec);
	}
	else if (lastDir != newDir)
	    theView->changeDir();
	break;
    }

    case PM_TCTL_TZ:
	pmNewZone(theView->_timeState.tz);
	theView->render(INV_View::timeLabel, 
			theView->_timeState.position.tv_sec);
	break;
	
    case PM_TCTL_SHOWDIALOG:
	break;

    default:
	cerr << "INV_View::timeCommandCB: Unknown?" << endl;
	break;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_TIMECONTROL) 
	cerr << "INV_View::timeCommandCB: cmd = " << __pmTimeCmdStr(cmd)
	     << endl << __pmTimeStr(&(theView->_timeState)) << endl;
#endif

    lastState = _VCRMODE(theView->_timeState.vcrmode);
    lastDir = newDir;

    entered = 0;
}

//
// Parse Command Line
//

int
INV_View::parseArgs()
{
    OMC_Bool	    otherFlag = OMC_false;
    int		    c;
    char            *msg;
    static int      first = 1;

    if (first) {
	/*
	 * can get called multiple times from main() so make initialization
	 * one-trip, so previous -t interval is not clobbered on a subsequent
	 * call
	 */
	_interval.tv_sec = 2;
	_interval.tv_usec = 0;
	first = 0;
    }

    while(otherFlag == OMC_false &&
	  (c = getopt(_argc, _argv, _argFlags.ptr())) != EOF) {
	
	switch (c) {
	case 'A':	// alignment
            if (theAflag) {
	    	pmprintf("%s: Error: At most one -A option allowed\n",
			 pmProgname);
                _sts--;
            }
	    theAtime = optarg;
	    theAflag = 1;
            break;

	case 'a':	// archive name
	    if (_sourceFlag == OMC_false &&
		theSource.numContexts() > 0) {
	    	pmprintf("%s: Error: Only one metric source (-a or -h) may be used\n",
			 pmProgname);
		_sts--;
	    }
	    // Only one source allowed
	    else if (_sourceFlag == OMC_false) {
		if (theSource.use(PM_CONTEXT_ARCHIVE, optarg) < 0)
			_sts--;
	    }
	    // Accept multiple sources
	    else {
		char *endnum = (char *)0;
		endnum = strtok(optarg, ", \t");
		while (endnum) {
		    if (theSource.use(PM_CONTEXT_ARCHIVE, endnum) < 0)
			_sts--;
		    endnum = strtok(NULL, ", \t");
		}
	    }
	    break;

	case 'C':	// check config file and exit
	    _checkConfigFlag = OMC_true;
	    break;

	case 'D':	// pmDebug flag - already parsed in INV_setup
	    break;

	case 'h':	// contact PMCD on this hostname
	    if (_sourceFlag == OMC_false &&
		theSource.numContexts() > 0) {
	    	pmprintf("%s: Error: Only one metric source (-a or -h) may be used\n",
			 pmProgname);
		_sts--;
	    }
	    else if (_hostFlag == OMC_false &&
	    	     theSource.numContexts() > 0) {
	    	pmprintf("%s: Error: Only one host (-h) may be specified\n",
			 pmProgname);
		_sts--;
	    }
	    else if (theSource.use(PM_CONTEXT_HOST, optarg) < 0)
		    _sts--;
		break;

	case 'n':	// alternative namespace, ignore
	    break;

	case 'O':
            if (theOflag) {
	    	pmprintf("%s: Error: At most one -O option allowed\n",
			 pmProgname);
                _sts--;
            }
	    theOtime = optarg;
	    theOflag = 1;
            break;

	case 'p':	// use existing time control
	    _timePort = optarg;
	    break;

	case 'S':
            if (theSflag) {
	    	pmprintf("%s: Error: At most one -S option allowed\n",
			 pmProgname);
                _sts--;
            }
	    theStime = optarg;
	    theSflag = 1;
            break;

        case 't':	// sampling interval
            if (pmParseInterval(optarg, &_interval, &msg) < 0) {
		pmprintf("%s", msg);
	    	pmprintf("%s: Error: Unable to parse -t option\n", pmProgname);
                free(msg);
                _sts--;
            }
            break;

        case 'T':	// run time
            if (theTflag) {
	    	pmprintf("%s: Error: At most one -T option allowed\n", 
			 pmProgname);
                _sts--;
            }
	    theTtime = optarg;
	    theTflag = 1;
            break;

	case 'x':	// pmlaunch version
	    if (strcmp(optarg, "1.0") == 0)
		_launchVersion = optarg;
	    else if (strcmp(optarg, "2.0") != 0) {
		pmprintf("%s: Error: unknown pmlaunch version\n", pmProgname);
		_sts--;
	    }
	    break;

        case 'z':	// timezone from host
            if (_timeZone.length() > 0) {
	    	pmprintf("%s: Error: At most one of -Z and/or -z option allowed\n",
			 pmProgname);
                _sts--;
            }
            thezflag++;
            break;

        case 'Z':	// $TZ timezone
            if (thezflag) {
	    	pmprintf("%s: Error: At most one of -Z and/or -z option allowed\n",
			 pmProgname);
                _sts--;
            }
            _timeZone = optarg;
            break;

	case '?':
	    _sts--;
	    break;

	default:
	    otherFlag = OMC_true;
	    break;
	}
    }

    return c;
}

int
INV_View::parseConfig(INV_View::SetupCB appCB)
{
    OMC_String		tzLabel;
    OMC_String		tzString;
    struct timeval	logStartTime;
    struct timeval	logEndTime;
    char		*msg;

    if (_sts < 0)
	return _sts;

//
// Put the display in the environemt
// This is needed for all children that are forked.
//

    char *displayEnv;
    if ((displayEnv = DisplayString(theApp->display())) != (char *)NULL) {
        char *d = (char *)malloc(strlen(displayEnv) + 16);
        sprintf(d, "DISPLAY=%s", displayEnv);
        putenv(d);
    }

//
// Call application setup routine
//

    _sts = (*appCB)();
    if (_sts < 0)
	return _sts;
    
//
// Set up timezones
//

    _sts = theSource.useDefault();
    if (_sts < 0) {
	if (theSource.defaultDefined())
	    pmprintf("%s: Error: %s: %s\n", pmProgname,
	    	     theSource.which()->host().ptr(), pmErrStr(_sts));
	else
	    pmprintf("%s: Error: localhost: %s\n", pmProgname,
	    	     pmErrStr(_sts));
	return _sts;
    }

    if (thezflag)
	theSource.useTZ();
    else if (_timeZone.length()) {
	_sts = theSource.useTZ(_timeZone);
	if (_sts < 0) {
	    pmprintf("%s: Error: Cannot set timezone to \"%s\": %s\n",
	    	     pmProgname, _timeZone.ptr(), pmErrStr(_sts));
            return _sts;
        }
    }

    theSource.defaultTZ(tzLabel, tzString);
    strncpy(_timeState.tz, tzString.ptr(), 39);
    strncpy(_timeState.tzlabel, tzLabel.ptr(), 39);
    _timeState.tz[39] = '\0';
    _timeState.tzlabel[39] = '\0';

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_View::parseArgs: Default TZ: \""
	     << _timeState.tz << "\" from host " 
	     << _timeState.tzlabel << endl;
#endif

    if (theSource.isLive() && (theAflag || theOflag || theSflag || theTflag)) {
    	pmprintf("%s: -A, -O, -S and -T options are not supported in live mode\n", 
		 pmProgname);
	_sts = -1;
	return _sts;
    }

//
// Determine start and end time bounds
//

    _sts = theSource.updateBounds();
    if (_sts >= 0) {
	logStartTime = theSource.logStart();
	logEndTime = theSource.logEnd();
	if (__pmtimevalToReal(&logEndTime) <= __pmtimevalToReal(&logStartTime)) {
	    logEndTime.tv_sec = INT_MAX;
	    logEndTime.tv_usec = INT_MAX;
	}
    }
    else {
	gettimeofday(&logStartTime, (struct timezone *)0);
	logEndTime.tv_sec = INT_MAX;
	logEndTime.tv_usec = INT_MAX;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	cerr << "INV_View::parseConfig: start = " 
	     << __pmtimevalToReal(&logStartTime) << ", end = " 
	     << __pmtimevalToReal(&logEndTime) 
	     << endl;
    }
#endif

    _sts = pmParseTimeWindow(theStime, theTtime, theAtime, theOtime, 
			     &logStartTime, &logEndTime, &_timeState.start, 
			     &_timeState.finish, &_timeState.position,
			     &msg);

    if (_sts < 0) {
	pmprintf("%s\n", msg);
    }

    return _sts;
}

void
INV_View::record()
{
    uint_t	delta = _timeState.delta;

    assert(theSource.isLive());

    if (delta == 0)
	delta = 1000;

    _record.changeState(delta);
    recordStateCB(this);
}

void
INV_View::recordStateCB(void *data)
{
    INV_View	*me = (INV_View *)data;
    int         dir;
    const char  **map;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_View::recordStateCB: entering" << endl;
#endif

    dir = _VCRMODE(me->_timeState.vcrmode) == PM_TCTL_VCRMODE_STOP ? 0 : 
				(me->_timeState.delta < 0 ? -1 : 1);
    map = pmTimeGetStatePixmap(_VCRMODE(me->_timeState.vcrmode), dir, 
			       theSource.isLive(), 
			       me->_record.active());
    VkSetHighlightingPixmap(me->_form->_vcr, (char**)map);

    me->recordState(me->_record.active());

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_View::recordStateCB: entering" << endl;
#endif

}
