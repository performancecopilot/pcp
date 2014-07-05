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


#include <wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <Vk/VkApp.h>
#include <Vk/VkFileSelectionDialog.h>

#include "pmapi.h"
#include "impl.h"
#include "Inv.h"
#include "App.h"
#include "Record.h"
#include "View.h"
#include "ModList.h"
#include "Source.h"

INV_RecordHost::~INV_RecordHost()
{
    _rhp = NULL;
    _record = NULL;
}

INV_RecordHost::INV_RecordHost(const char *host)
: _host(host),
  _rhp(0),
  _record(0),
  _id(0)
{
}

INV_Record::~INV_Record()
{
}

INV_Record::INV_Record(ToolConfigCB toolCB,
		       OMC_Bool modConfig,
		       LogConfigCB logCB,
		       StateCB stateCB,
		       void *stateData,
		       const char *allConfig,
		       const char *addConfig)
: _active(OMC_false),
  _modConfig(modConfig),
  _toolConfigCB(toolCB),
  _logConfigCB(logCB),
  _stateCB(stateCB),
  _stateData(stateData),
  _list(),
  _lastHost(0),
  _numActive(0),
  _allConfig(allConfig),
  _addConfig(addConfig)
{
    assert(modConfig || _logConfigCB);
}

void
INV_Record::add(const char *host, const char *metric)
{
    uint_t		i = 0;
    OMC_String		metricStr = metric;

    assert(_active == OMC_false);

    if (_list.length())
	if (_list[_lastHost]->_host == host)
	    i = _lastHost;
	else
	    for (i = 0; i < _list.length(); i++)
		if (_list[i]->_host == host)
		    break;

    if (i == _list.length()) {
	INV_RecordHost *newHost = new INV_RecordHost(host);
	_list.append(newHost);
    }

    _list[i]->_metrics.append(metricStr);
    _lastHost = i;
}

void
INV_Record::addOnce(const char *host, const char *metric)
{
    uint_t		i = 0;
    OMC_String		metricStr = metric;

    assert(_active == OMC_false);

    if (_list.length())
	if (_list[_lastHost]->_host == host)
	    i = _lastHost;
	else
	    for (i = 0; i < _list.length(); i++)
		if (_list[i]->_host == host)
		    break;

    if (i == _list.length()) {
	INV_RecordHost *newHost = new INV_RecordHost(host);
	_list.append(newHost);
    }

    _list[i]->_once.append(metricStr);
    _lastHost = i;
}

void
INV_Record::changeState(uint_t delta)
{
    FILE		*fp = NULL;
    OMC_String		timeStr;
    const char		*name;
    char		*p;
    char		c;
    int			sts;
    int			i;

    if (_active) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Record::changeState: Deactivating record session"
		 << endl;
#endif

	for (i = 0; i < _list.length(); i++) {
	    if (_list[i]->_id != (XtInputId)NULL)
		XtRemoveInput(_list[i]->_id);
	    _list[i]->_id = (XtInputId)NULL;
	    delete _list[i];
	}
	_list.removeAll();

	if (_numActive != 0) {
	    sts = pmRecordControl(NULL, PM_REC_OFF, NULL);
	    if (sts < 0) {
		INV_errorMsg(_POS_, "Failed to control all pmlogger process(es): %s\nTerminating record session.\n",
			     pmErrStr(sts));
	    }
	    _numActive = 0;
	}

	_active = OMC_false;
	return;
    }

    // Changing to active ...

    theFileSelectionDialog->setTitle("Record Archive Folio");
    theFileSelectionDialog->setFilterPattern("*");

    if (theFileSelectionDialog->postAndWait() == VkDialogManager::OK) {
	name = theFileSelectionDialog->fileName();

	p = (char*)name;
	if (checkspecial(p) < 0) {
	    INV_errorMsg(_POS_, 
			 "Failed to create replay config file:\n\"%s\"\n(contains special characters)",
			 name);
	    return;
	}

	while (*p != '\0' && (p = strchr(p, '/')) != NULL) {
	    /* quicker to mkdir and fail, than check ... */
	    c = *p;
	    *p = '\0';
	    mkdir(name, 0777);
	    *p = c;
	    p++;
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Record::changeState: Setting up pmafm at \""
		 << name << '"' << endl;
#endif

	fp = pmRecordSetup((char*)name, pmProgname,
			   (_toolConfigCB == NULL ? 0 : 1));

	if (fp == NULL) {
	    INV_errorMsg(_POS_,
			 "Failed to create archive folio\n\"%s\":\n%s\n",
			 name, pmErrStr(-errno));
	    _active = OMC_false;
	    return;
	    /*NOTREACHED*/
	}

	theApp->busy();
	sts = 0;

	if (_toolConfigCB != NULL)
	    sts = (*_toolConfigCB)(fp);

	if (sts < 0)
	    goto failed;
#ifdef PCP_DEBUG
	else if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Record::changeState: Tool config done" << endl;
#endif

	if (_modConfig)
	    theModList->record(*this);
	
	if (_logConfigCB != NULL)
	    (*_logConfigCB)(*this);

	if (_list.length() == 0) {
	    INV_errorMsg(_POS_, "Failed to generate pmlogger configs");
	    sts = 1;
	    goto failed;		
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Record::changeState: Log config generated" << endl;
#endif

	for (i = 0; i < _list.length(); i++) {
	    const char *hostname = _list[i]->_host.ptr();
	    sts = pmRecordAddHost(hostname,
				  (theSource.which()->host() == hostname),
				  &(_list[i]->_rhp));
	    if (sts < 0) {
		INV_errorMsg(_POS_, "Failed to record on host %s: %s\n",
			     hostname, pmErrStr(sts));
		goto failed;
	    }

	    if (_allConfig.length() == 0)
		sts = dumpConfig(_list[i]->_rhp->f_config, *(_list[i]), delta);
	    else
		sts = dumpFile(_list[i]->_rhp->f_config);

	    if (sts < 0)
		goto failed;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		cerr << "INV_Record::changeState: Generated config for host ["
		     << i << "] = " << hostname << ":" << endl
		     << *(_list[i]) << endl;
#endif
	}

	// Set the pmlogger default interval to my update interval
	timeStr = "-t";
	timeStr.appendInt(delta);
	timeStr.append("msec");
	sts = pmRecordControl(NULL, PM_REC_SETARG, timeStr.ptr());
	if (sts < 0) {
	    INV_errorMsg(_POS_, 
	    		 "Failed to set default logging interval to %s msec: %s",
			 timeStr.ptr(), pmErrStr(sts));
	    goto failed;		
	}

	if (sts >= 0) {
	    sts = pmRecordControl(NULL, PM_REC_ON, NULL);
	    if (sts < 0) {
		INV_errorMsg(_POS_, 
			     "Failed to start pmlogger process(es): %s\n",
			     pmErrStr(sts));
		goto failed;
	    }
	    else
		_active = OMC_true;
	}
	
	_numActive = 0;
	for (i = 0; i < _list.length(); i++) {

	    const char *hostname = _list[i]->_host.ptr();

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		cerr << "INV_Record::changeState: Creating VkInput callback for host " 
		     << hostname << endl;
#endif

	    _list[i]->_id = XtAppAddInput(theApp->appContext(),
					  _list[i]->_rhp->fd_ipc,
					  (XtPointer)XtInputExceptMask,
					  (XtInputCallbackProc)INV_Record::logFailCB,
					  (XtPointer)(_list[i]));
	    _list[i]->_record = this;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		cerr << "INV_Record::changeState: Created VkInput callback for host " 
		     << hostname << ":" << endl << *(_list[i]) << endl;
#endif
	    _numActive++;
	}

    failed:

	if (sts < 0) {
	    for (i = 0; i < _list.length(); i++)
		delete _list[i];
	    _active = OMC_false;
	}

	theApp->notBusy();
    }
}


int
INV_Record::checkspecial(char* p)
{
    for (int i = 0; p[i] != '\0'; i++) {
        if (p[i] == '*' || p[i] == '~' || p[i] == '?')
            return -1;
    }
    return 0;
}

int
INV_Record::dumpConfig(FILE *fp, const INV_RecordHost &host, uint_t delta)
{
    uint_t	i;
    char	*p;

    fprintf(fp, "#\n# pmlogger config generated by %s\n", pmProgname);
    fprintf(fp, "# for host %s\n#\n", host._host.ptr());

    if (host._once.length()) {
	fprintf(fp, "log mandatory on once {\n");
	for (i = 0; i < host._once.length(); i++)
	    fprintf(fp, "%s\n", host._once[i].ptr());
	fprintf(fp, "}\n");
    }

    if (host._metrics.length()) {
	fprintf(fp, "log mandatory on %d msec {\n", delta);
	for (i = 0; i < host._metrics.length(); i++)
	    fprintf(fp, "%s\n", host._metrics[i].ptr());
	fprintf(fp, "}\n");
    }

    if (_addConfig.length() > 0) {
	FILE *afp = fopen(_addConfig.ptr(), "r");
	if (afp == NULL) {
	    INV_errorMsg(_POS_, 
			 "Could not open additional pmlogger config file \"%s\": %s\n",
			 _addConfig.ptr(), strerror(errno));
	    return -1;
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Record::dumpConfig: Adding entries from "
		 << _addConfig << endl;
#endif

	fprintf(fp, "\n#\n# Additional entries from %s\n#\n\n", 
		_addConfig.ptr());
	while (!feof(afp)) {
	    p = fgets(theBuffer, theBufferLen, afp);
	    if (p == NULL)
		break;
	    fputs(theBuffer, fp);
	}
	fclose(afp);
    }
    return 0;
}

int
INV_Record::dumpFile(FILE *fp)
{
    char	*p;

    FILE *afp = fopen(_allConfig.ptr(), "r");
    if (afp == NULL) {
	INV_errorMsg(_POS_, 
		     "Could not open pmlogger config file \"%s\": %s\n",
		     _allConfig.ptr(), strerror(errno));
	return -1;
    }

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Record::dumpFile: Using pmlogger config "
		 << _allConfig << endl;
#endif

    while (!feof(afp)) {
	p = fgets(theBuffer, theBufferLen, afp);
	if (p == NULL)
	    break;
	fputs(theBuffer, fp);
    }
    fclose(afp);
    return 0;
}

ostream&
operator<<(ostream& os, const INV_RecordHost &host)
{
    uint_t	i;

    os << host._host << ":" << endl << "input CB is "
       << (host._id == (XtInputId)NULL ? "unset" : "set") << endl;
    os << "record host structure: ";
    if (host._rhp) {
	os << endl << "  fd_ipc = " << host._rhp->fd_ipc << endl;
	os << "  logfile = " 
	   << (host._rhp->logfile == NULL ? "<undef>" : host._rhp->logfile)
	   << endl;
	os << "  pid = " << host._rhp->pid << endl;
	os << "  status = " << host._rhp->status << endl;
    }
    else
	os << "not defined" << endl;

    if (host._once.length()) {
	os << "log once:" << endl;
	for (i = 0; i < host._once.length(); i++)
	    os << "  [" << i << "] " << host._once[i] << endl;
    }
    if (host._metrics.length()) {
	os << "log metrics:" << endl;
	for (i = 0; i < host._metrics.length(); i++)
	    os << "  [" << i << "] " << host._metrics[i] << endl;
    }
    return os;
}

ostream&
operator<<(ostream& os, const INV_Record &rhs)
{
    uint_t	h;

    os << "INV_Record: " << rhs._list.length() << " hosts" 
       << endl;

    for (h = 0; h < rhs._list.length(); h++)
	os << '[' << h << "] " << *(rhs._list[h]);

    return os;
}

void
INV_Record::logFailCB(XtPointer clientData, int *, XtInputId *id)
{
    INV_RecordHost	*host = (INV_RecordHost *)clientData;
    int			sts;

#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Record::logFailCB: entering callback" << endl;
#endif

    assert(host->_id == *id);

    if (host->_id != (XtInputId)NULL)
	XtRemoveInput(host->_id);
    host->_id = 0;

#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Record::logFailCB: pmlogger died for "
		 << *host << endl;
#endif

    sts = pmRecordControl(host->_rhp, PM_REC_STATUS, NULL);
    
    if (sts < 0) {
	
	if (WIFEXITED(host->_rhp->status)) {
	    if (WEXITSTATUS(host->_rhp->status))
		INV_errorMsg(_POS_, 
			     "pmlogger(1) for host %s exited prematurely.\nMore details may be found in:\n%s\n",
			     host->_host.ptr(), host->_rhp->logfile);
	    else
		INV_errorMsg(_POS_, 
			     "pmlogger(1) for host %s exited prematurely with status %d.\nMore details may be found in:\n%s\n",
			     host->_host.ptr(), 
			     WEXITSTATUS(host->_rhp->status),
			     host->_rhp->logfile);
	}
	else if (WIFSIGNALED(host->_rhp->status)) {
	    if (WCOREDUMP(host->_rhp->status))
		INV_errorMsg(_POS_, 
			     "pmlogger(1) for host %s received signal %d and dumped core.\nMore details may be found in:\n%s\n",
			     host->_host.ptr(), 
			     WTERMSIG(host->_rhp->status),
			     host->_rhp->logfile);
	    else
		INV_errorMsg(_POS_, 
			     "pmlogger(1) for host %s received signal %d and exited.\nMore details may be found in:\n%s\n",
			     host->_host.ptr(), 
			     WTERMSIG(host->_rhp->status),
			     host->_rhp->logfile);
	}
	else {
	    INV_errorMsg(_POS_, 
			 "pmlogger(1) for host %s exited prematurely.\nMore details may be found in:\n%s\n",
			 host->_host.ptr(),
			 host->_rhp->logfile);
	}
    }

    INV_Record *me = host->_record;

    me->_numActive--;

    if (me->_numActive == 0) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_Record::logFailCB: No loggers left, changing state..."
		 << endl;
#endif

	me->changeState();
	if (me->_stateCB != NULL)
	    (*(me->_stateCB))(me->_stateData);
    }

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_Record::logFailCB: leaving callback, " << me->_numActive
	     << " loggers remain" << endl;
#endif

}
