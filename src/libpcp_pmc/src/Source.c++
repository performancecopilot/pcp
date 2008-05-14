/*
 * Copyright (c) 1998,2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <pcp/pmc/Source.h>

PMC_SourceList	PMC_Source::_sourceList;
PMC_String	PMC_Source::_localHost;

void
PMC_Source::retryConnect(int type, const char *source)
{
    int		oldTZ;
    int		oldContext;
    int		sts;
    char	*tzs;

    if (_localHost.length() == 0) {
	char        buf[MAXHOSTNAMELEN];
	(void)gethostname(buf, MAXHOSTNAMELEN);
	buf[MAXHOSTNAMELEN-1] = '\0';
	_localHost = buf;
    }

    switch(type) {
    case PM_CONTEXT_LOCAL:
	{
	    _desc = "CONTEXT_LOCAL";
	    _host = _localHost;
	    _source = _host;
	    break;
	}
    case PM_CONTEXT_HOST:
	{
  	    _desc = "host \"";
	    _desc.append(source);
	    _desc.appendChar('\"');
	    _host = source;
	    _source = _host;
	    break;
	}
    case PM_CONTEXT_ARCHIVE:
	{
	    _desc = "archive \"";
	    _desc.append(source);
	    _desc.appendChar('\"');
	    _source = source;
	    break;
	}
    }

    oldContext = pmWhichContext();

    _sts = pmNewContext(type, source);
    if (_sts >= 0) {

	_hndls.append(_sts);

	if (_type == PM_CONTEXT_ARCHIVE) {
	    pmLogLabel lp;
	    _sts = pmGetArchiveLabel(&lp);
	    if (_sts < 0) {
		pmprintf("%s: Unable to obtain log label for \"%s\": %s\n",
			 pmProgname, _desc.ptr(), pmErrStr(_sts));
		_host="unknown?";
		goto done;
	    }
	    else {
		_host = lp.ll_hostname;
		_start = lp.ll_start;
	    }
	    _sts = pmGetArchiveEnd(&_end);
	    if (_sts < 0) {
		pmprintf("%s: Unable to determine end of \"%s\": %s\n",
			 pmProgname, _desc.ptr(), pmErrStr(_sts));
		goto done;
	    }
	}
	else {
	    _start.tv_sec = 0;
	    _start.tv_usec = 0;
	    _end = _start;
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "PMC_Source::PMC_Source: Created context "
		 << _hndls.tail() << " to " << _desc << endl;
#endif

	oldTZ = pmWhichZone(&tzs);
	_tz = pmNewContextZone();

	if (_tz < 0) {
	    pmprintf("%s: Warning: Unable to obtain timezone for %s: %s\n",
		     pmProgname, _desc.ptr(), pmErrStr(_tz));
	}
	else {
	    sts = pmWhichZone(&tzs);
	    if (sts >= 0)
		_timezone = tzs;
	    else {
		pmprintf("%s: Warning: Unable to obtain timezone for %s: %s\n",
			 pmProgname, _desc.ptr(), pmErrStr(sts));
	    }
	}

	if (oldTZ >= 0) {
	    sts = pmUseZone(oldTZ);
	    if (sts < 0) {
		pmprintf("%s: Warning: Unable to switch timezones. Using timezone for %s: %s\n",
			 pmProgname, _desc.ptr(), pmErrStr(sts));
	    }	
	}
    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_PMC) {
        cerr << "PMC_Source::PMC_Source: Context to " << source
             << " failed: " << pmErrStr(_sts) << endl;
    }
#endif

 done:
    _sourceList.append(this);

    if (oldContext >= 0) {
	sts = pmUseContext(oldContext);
	if (sts < 0) {
	    pmprintf("%s: Warning: Unable to switch contexts. Using context to %s: %s\n",
		     pmProgname, _desc.ptr(), pmErrStr(sts));
	}
    }
}

PMC_Source::~PMC_Source()
{
    uint_t	i;

    for (i = 0; i < _sourceList.length(); i++)
	if (_sourceList[i] == this)
	    break;
    if (i < _sourceList.length())
	_sourceList.remove(i);
}

PMC_Source*
PMC_Source::getSource(int type, char const* source, PMC_Bool matchHosts)
{
    uint_t	i;
    PMC_Source*	src = NULL;

    for (i = 0; i < _sourceList.length(); i++) {
	src = _sourceList[i];
	if (matchHosts &&
	    type == PM_CONTEXT_HOST) {
	    if (src->type() == PM_CONTEXT_ARCHIVE &&
		src->host() == source) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PMC)
		    cerr << "PMC_Source::getSource: Matched host "
			 << source << " to archive " << src->source()
			 << " (source " << i << ")" << endl;
#endif
		break;
	    }
	}
	else if (src->type() == type &&
		 src->source() == source) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC)
		cerr << "PMC_Source::getSource: Matched " << source
		     << " to source " << i << endl;
#endif
	    if (src->_sts < 0) {
		// try and connect again
		src->retryConnect(type, source);
	    }
	    break;
	}
    }

    if (i == _sourceList.length() && 
	!(matchHosts == PMC_true && type == PM_CONTEXT_HOST)) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    if (type != PM_CONTEXT_LOCAL)
		cerr << "PMC_Source::getSource: Creating new source for "
		     << source << endl;
	    else
		cerr << "PMC_Source::getSource: Creating new source for CONTEXT_LOCAL"
		     << endl;
#endif

	src = new PMC_Source(type, source);
    }

#ifdef PCP_DEBUG
    if (src == NULL && pmDebug & DBG_TRACE_PMC)
	cerr << "PMC_Source::getSource: Unable to map host "
	     << source << " to an arch context" << endl;
#endif

    return src;
}

PMC_Source::PMC_Source(int type, const char* source)
: _sts(-1),
  _type(type),
  _source(),
  _host(),
  _desc(),
  _timezone(),
  _tz(0),
  _hndls(),
  _dupFlag(PMC_false)
{
    this->retryConnect(type, source);
}

int
PMC_Source::dupContext()
{
    int sts = 0;

    if (_sts < 0)
	return _sts;

    if (_dupFlag == PMC_false && _hndls.length() == 1) {
	sts = pmUseContext(_hndls[0]);
	if (sts >= 0) {
	    sts = _hndls[0];
	    _dupFlag = PMC_true;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC)
		cerr << "PMC_Source::dupContext: Using original context for "
		     << _desc << endl;
#endif
	}
	else {
	    pmprintf("%s: Error: Unable to switch to context for \"%s\": %s\n",
		     pmProgname, _desc.ptr(), pmErrStr(sts));
	}
    }
    else if (_hndls.length()) {
	sts = pmUseContext(_hndls[0]);

	if (sts >= 0) {
	    sts = pmDupContext();
	    if (sts >= 0) {
		_hndls.append(sts);
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PMC)
		    cerr << "PMC_Source::dupContext: " << _desc 
			 << " duplicated, hndl[" << _hndls.length()-1 
			 << "] = " << sts << endl;
#endif
	    }
	    else {
		pmprintf("%s: Error: Unable to duplicate context to \"%s\": %s\n",
			 pmProgname, _desc.ptr(), pmErrStr(sts));
	    }
	}
	else {
	    pmprintf("%s: Error: Unable to switch to context for \"%s\": %s\n",
		     pmProgname, _desc.ptr(), pmErrStr(sts));
	}
    }

    // No active contexts, create a new context
    else {

	sts = pmNewContext(_type, _source.ptr());
	if (sts >= 0) {
	    _hndls.append(sts);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC)
		cerr << "PMC_Source::dupContext: new context to " << _desc 
		     << " created, hndl = " << sts << endl;
#endif
	}
    }
#ifdef PCP_DEBUG
    if (sts < 0 && pmDebug & DBG_TRACE_PMC) {
	if (sts < 0)
	    cerr << "PMC_Source::dupContext: context to " << _desc
		 << " failed: " << pmErrStr(_sts) << endl;
    }
#endif

    return sts;
}

int
PMC_Source::delContext(int hndl)
{
    uint_t	i;
    int		sts;

    for (i = 0; i < _hndls.length(); i++)
	if (_hndls[i] == hndl)
	    break;

    if (i == _hndls.length()) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "PMC_Source::delContext: Attempt to delete " << hndl
		 << " from list for " << _desc << ", but it is not listed"
		 << endl;
#endif
	return PM_ERR_NOCONTEXT;
	/*NOTREACHED*/
    }

    sts = pmDestroyContext(_hndls[i]);
    _hndls.remove(i);

    // If this is a valid source, but no more contexts remain,
    // then we should delete ourselves
    if (_hndls.length() == 0 && _sts >= 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "PMC_Source::delContext: No contexts remain, removing "
		 << _desc << endl;
#endif
	delete this;
    }

    return sts;
}

ostream&
operator<<(ostream &os, const PMC_Source &rhs)
{
    os << rhs._desc;
    return os;
}

void
PMC_Source::dump(ostream& os)
{
    uint_t	i;

    os << "  sts = " << _sts << ", type = " << _type << ", source = " 
       << _source << endl << "  host = " << _host << ", timezone = " 
       << _timezone << ", tz hndl = " << _tz << endl;

    if (_sts >= 0) {
	os << "  start = " << __pmtimevalToReal(&_start) << ", end = "
	   << __pmtimevalToReal(&_end) << ", dupFlag = "
	   << (_dupFlag == PMC_true ? "true" : "false") << endl << "  " 
	   << _hndls.length() << " contexts: ";
    }

    for (i = 0; i < _hndls.length(); i++)
	os << _hndls[i] << ' ';

    os << endl;
}

void
PMC_Source::dumpList(ostream& os)
{
    uint_t	i;
    os << _sourceList.length() << " sources:" << endl;
    for (i = 0; i < _sourceList.length(); i++) {
	os << '[' << i << "] " << *(_sourceList[i]) << endl;
	_sourceList[i]->dump(os);
    }
}
