/*
 * Copyright (c) 1997-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#include <limits.h>
#include <float.h>
#include <math.h>
// g++ barfs when parsing cstdio if other includes occur before String.h!
#include <pcp/pmc/String.h>
#include "pmapi.h"
#include "impl.h"
#include <pcp/pmc/Hash.h>
#include <pcp/pmc/Group.h>
#include <pcp/pmc/Source.h>
#include <pcp/pmc/Context.h>
#include <pcp/pmc/Metric.h>


PMC_Bool	PMC_Group::_tzLocalInit = PMC_false;
int		PMC_Group::_tzLocal = -1;
PMC_String	PMC_Group::_tzLocalStr;
PMC_String	PMC_Group::_localHost;

PMC_Group::~PMC_Group()
{
    uint_t	i;

    for (i = 0; i < _contexts.length(); i++)
	if (_contexts[i])
	    delete _contexts[i];
}


PMC_Group::PMC_Group(PMC_Bool restrictArchives)
: _contexts(),
  _restrictArch(restrictArchives),
  _mode(PM_CONTEXT_HOST),
  _use(-1),
  _localSource(0),
  _tzFlag(unknownTZ),
  _tzDefault(-1),
  _tzUser(-1),
  _tzUserStr(),
  _tzGroupIndex(0),
  _timeEndDbl(0.0)
{
    char	buf[MAXHOSTNAMELEN];
    char*	tz;

    // Get timezone from environment
    if (_tzLocalInit == PMC_false) {

	(void)gethostname(buf, MAXHOSTNAMELEN);
	buf[MAXHOSTNAMELEN-1] = '\0';
	_localHost = buf;

        tz = __pmTimezone();
	if (tz == NULL)
	    pmprintf("%s: Warning: Unable to get timezone from environment\n",
		     pmProgname);
	else {
	    _tzLocal = pmNewZone(tz);
	    if (_tzLocal < 0)
		pmprintf("%s: Warning: Timezone for localhost: %s\n",
			 pmProgname, pmErrStr(_tzLocal));
	    else {
		_tzLocalStr = tz;
		_tzDefault = _tzLocal;
		_tzFlag = localTZ;
	    }
	}
	_tzLocalInit = PMC_true;
    }
}

int
PMC_Group::use(int type, char const* source)
{
    int		sts = 0;
    uint_t	i;

    if (type == PM_CONTEXT_LOCAL) {
	for (i = 0; i < numContexts(); i++)
	    if (_contexts[i]->source().type() == type)
		break;
    }
    else {
	if (source == NULL) {
	    if (type == PM_CONTEXT_ARCHIVE) {
		pmprintf("%s: Error: Archive context requires path to archive\n",
			 pmProgname);
		return PM_ERR_NOCONTEXT;
	    }
	    else {
		if (!defaultDefined()) {
		    createLocalContext();
		    if (!defaultDefined()) {
			pmprintf("%s: Error: Cannot connect to PMCD on localhost: %s\n",
				 pmProgname, pmErrStr(_localSource->status()));
			return _localSource->status();
		    }
		}
		source = _contexts[0]->source().source().ptr();
	    }
	}

	// Search contexts in this group for an existing match
	for (i = 0; i < numContexts(); i++) {

	    // Archive hostnames may be truncated, so only compare up to
	    // PM_LOG_MAXHOSTLEN - 1.
	    if (strncmp(_contexts[i]->source().source().ptr(), source, 
			PM_LOG_MAXHOSTLEN - 1) == 0)
		break;
	}
    }

    if (i == numContexts()) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "PMC_Group::use: No direct match for context \"" << source
		 << "\" (type " << type << ")." << endl;
	}
#endif

	// Determine live or archive mode by the first source
	if (i == 0)
	    _mode = type;

	// If the assumed mode differs from the requested context type
	// we may need to map the host to an archive
	if (_mode != type) {

	    if (_mode == PM_CONTEXT_HOST && type == PM_CONTEXT_ARCHIVE) {
		pmprintf("%s: Error: Archive \"%s\" requested after live mode was assumed.\n",
			 pmProgname, source);
		return PM_ERR_NOCONTEXT;
	    }

	    // If we are in archive mode, map hosts to archives with the
	    // same host
	    if (_mode == PM_CONTEXT_ARCHIVE && type == PM_CONTEXT_HOST) {
		for (i = 0; i < numContexts(); i++)
		    if (strncmp(_contexts[i]->source().host().ptr(), source, 
				PM_LOG_MAXHOSTLEN - 1) == 0)
			break;

		if (i == numContexts()) {
		    pmprintf("%s: Error: No archives were specified for host \"%s\"\n",
			     pmProgname, source);
		    return PM_ERR_NOTARCHIVE;
		}
	    }
	}
    }

    if (i == numContexts()) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "PMC_Group::use: Creating new context for \"" << source
		 << '\"' << endl;
	}
#endif

	PMC_Source *src;

	src = PMC_Source::getSource(type, source, PMC_false);

	if (src == NULL) {
	    pmprintf("%s: Error: No archives were specified for host \"%s\"\n",
		     pmProgname, source);
	    return PM_ERR_NOTARCHIVE;
	}

	PMC_Context *newContext = new PMC_Context(src);

	if (newContext->hndl() < 0) {
	    sts = newContext->hndl();
	    pmprintf("%s: Error: %s: %s\n", 
		     pmProgname, newContext->source().desc().ptr(), 
		     pmErrStr(sts));
	    delete newContext;
	    return sts;
	}

	// If we are in archive mode and are adding an archive,
	// make sure another archive for the same host does not exist
	if (_restrictArch && type == PM_CONTEXT_ARCHIVE) {
	    for (i = 0; i < numContexts(); i++)
		// No need to restrict comparison here, both are from
		// log labels.
		if (_contexts[i]->source().host() == newContext->source().host()) {
		    pmprintf("%s: Error: Archives \"%s\" and \"%s\" are from "
			     "the same host \"%s\"\n",
			     pmProgname, _contexts[i]->source().source().ptr(),
			     newContext->source().source().ptr(),
			     _contexts[i]->source().host().ptr());
		    delete newContext;
		    return PM_ERR_NOCONTEXT;
		}
	}

	_contexts.append(newContext);
	_use = _contexts.length() - 1;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "PMC_Group::use: Added context " << _use << ": "
		 << *newContext << endl;
#endif
    }

    // We found a match, do we need to use a different context?
    else if (i != (uint_t)_use) {
	_use = i;
	sts = useContext();
	if (sts < 0) {
	    pmprintf("%s: Error: Unable to use context to %s: %s\n",
		     pmProgname, which()->source().desc().ptr(), 
		     pmErrStr(sts));
	    return sts;
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "PMC_Group::use: Using existing context " << _use
		 << " for " << which()->source().desc() << endl;
#endif
    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_PMC)
	cerr << "PMC_Group::use: Using current context " << _use
	     << " (hndl = " << which()->hndl() << ") for " 
	     << which()->source().desc() << endl;
#endif
    
    return which()->hndl();
}

int
PMC_Group::useTZ()
{
    int sts;

    if ((sts = which()->useTZ()) >= 0) {
	_tzDefault = which()->source().tzHndl();
	_tzFlag = groupTZ;
	_tzGroupIndex = _use;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "PMC_Group::useTZ: Using timezone of "
		 << which()->source().desc()
		 << " (" << _tzGroupIndex << ')' << endl;
#endif
    }
    return sts;
}

int
PMC_Group::useTZ(const PMC_String &tz)
{
    int sts = pmNewZone(tz.ptr());

    if (sts >= 0) {
	_tzUser = sts;
	_tzUserStr = tz;
	_tzFlag = userTZ;
	_tzDefault = sts;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "PMC_Group::useTZ: Switching timezones to \"" << tz
		 << "\" (" << _tzUserStr << ')' << endl;
	}
#endif
    }
    return sts;
}

int
PMC_Group::useLocalTZ()
{
    int sts;

    if (_tzLocal >= 0) {
	sts = pmUseZone(_tzLocal);
	if (sts > 0) {
	    _tzFlag = localTZ;
	    _tzDefault = _tzLocal;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC)
		cerr << "PMC_Group::useTZ: Using timezone of host \"localhost\"" << endl;
#endif
	}
    }
    else
	sts = _tzLocal;
    return sts;
}

void
PMC_Group::defaultTZ(PMC_String &label, PMC_String &tz)
{
    if (_tzFlag == userTZ) {
	label = _tzUserStr;
	tz = _tzUserStr;
    }
    else if (_tzFlag == localTZ) {
	label = _localHost;
	tz = _tzLocalStr;
    }
    else {
	label = _contexts[_tzGroupIndex]->source().host();
	tz = _contexts[_tzGroupIndex]->source().timezone();
    }
}

int
PMC_Group::useDefaultTZ()
{
    int	sts = -1;

    if (_tzFlag != unknownTZ)
	sts = pmUseZone(_tzDefault);
    return sts;
}

int
PMC_Group::useDefault()
{
    int		sts;

    if (numContexts() == 0)
	createLocalContext();

    if (numContexts() == 0)
	return _localSource->status();

    _use = 0;
    sts = pmUseContext(which()->hndl());
    return sts;
}

void
PMC_Group::createLocalContext()
{
    if (numContexts() == 0) {
	PMC_Source *_localSource = PMC_Source::getSource(PM_CONTEXT_HOST,
							 _localHost.ptr(),
							 PMC_false);
	// This should never happen
	assert(_localSource != NULL);

#ifdef PCP_DEBUG
	if (_localSource->status() < 0 && pmDebug & DBG_TRACE_PMC)
	    cerr << "PMC_Group::createLocalContext: Default context to "
		 << _localSource->desc() << " failed: " 
		 << pmErrStr(_localSource->status()) << endl;
	else if (pmDebug & DBG_TRACE_PMC)
	    cerr << "PMC_Group::createLocalContext: Default context to "
		 << _localSource->desc() << endl;
#endif

	PMC_Context *newContext = new PMC_Context(_localSource);
	_contexts.append(newContext);
	_use = _contexts.length() - 1;
    }
}

void
PMC_Group::updateBounds()
{
    double		newStart = DBL_MAX;
    double		newEnd = 0.0;
    double		startDbl;
    double		endDbl;
    struct timeval	startTv;
    struct timeval	endTv;
    uint_t		i;

    _timeStart.tv_sec = 0;
    _timeStart.tv_usec = 0;
    _timeEnd = _timeStart;

    for (i = 0; i < numContexts(); i++) {
	if (_contexts[i]->hndl() >= 0 &&
	    _contexts[i]->source().type() == PM_CONTEXT_ARCHIVE) {
	    startTv = _contexts[i]->source().start();
	    endTv = _contexts[i]->source().end();
	    startDbl = __pmtimevalToReal(&startTv);
	    endDbl = __pmtimevalToReal(&endTv);
	    if (startDbl < newStart)
		newStart = startDbl;
	    if (endDbl > newEnd)
		newEnd = endDbl;
	}
    }

    __pmtimevalFromReal(newStart, &_timeStart);
    __pmtimevalFromReal(newEnd, &_timeEnd);
    _timeEndDbl = newEnd;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC) {
        cerr << "PMC_Group::getTimeBounds: start = " << _timeStart.tv_sec 
	     << '.' << _timeStart.tv_usec << ", end = "
             << _timeEnd.tv_sec << '.' << _timeEnd.tv_usec << endl;
    }
#endif
}

void
PMC_Group::dump(ostream &os)
{
    uint_t	i;

    os << "mode: ";
    switch(_mode) {
    case PM_CONTEXT_LOCAL:
	os << "local";
	break;
    case PM_CONTEXT_HOST:
	os << "live host";
	break;
    case PM_CONTEXT_ARCHIVE:
	os << "archive";
	break;
    }

    os << ", timezone: ";
    switch(_tzFlag) {
    case PMC_Group::localTZ:
	os << "local = \"" << _tzLocalStr;
	break;
    case PMC_Group::userTZ:
	os << "user = \"" << _tzUserStr;
	break;
    case PMC_Group::groupTZ:
	os << "group = \"" 
	   << _contexts[_tzGroupIndex]->source().timezone();
	break;
    case PMC_Group::unknownTZ:
	os << "unknown = \"???";
	break;
    }

    os << "\": " << endl;

    os << "  " << numContexts() << " contexts:" << endl;
    for (i = 0; i < numContexts(); i++) {
	os << "    [" << i << "] " << *(_contexts[i]) << endl;
	_contexts[i]->dumpMetrics(os);
    }
}

int
PMC_Group::useContext()
{
    int		sts;

    sts = pmUseContext(which()->hndl());
    if (sts < 0)
	pmprintf("%s: Error: Unable to reuse context to %s: %s\n",
		 pmProgname, which()->source().desc().ptr(), pmErrStr(sts));
    return sts;
}

PMC_Metric*
PMC_Group::addMetric(char const* str, double theScale, PMC_Bool active)
{
    PMC_Metric* metric = new PMC_Metric(this, str, theScale, active);
    if (metric->status() >= 0)
	metric->contextRef().addMetric(metric);
    return metric;
}

PMC_Metric*
PMC_Group::addMetric(pmMetricSpec* theMetric, double theScale, PMC_Bool active)
{
    PMC_Metric* metric = new PMC_Metric(this, theMetric, theScale, active);
    if (metric->status() >= 0)
	metric->contextRef().addMetric(metric);
    return metric;
}

int
PMC_Group::fetch(PMC_Bool update)
{
    uint_t	i;
    int		sts = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC)
	cerr << "PMC_Group::fetch: " << numContexts() << " contexts" << endl;
#endif

    for (i = 0; i < numContexts(); i++)
	_contexts[i]->fetch(update);

    if (numContexts())
	sts = useContext();

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC)
	cerr << "PMC_Group::fetch: Done" << endl;
#endif

    return sts;
}

int
PMC_Group::setArchiveMode(int mode, 
			  const struct timeval *when, 
			  int interval)
{
    uint_t	i;
    int		sts;
    int		result = 0;

    for (i = 0; i < numContexts(); i++) {
	if (_contexts[i]->source().type() != PM_CONTEXT_ARCHIVE)
	    continue;

	sts = pmUseContext(_contexts[i]->hndl());
	if (sts < 0) {
	    pmprintf("%s: Error: Unable to switch to context for %s: %s\n",
		     pmProgname, _contexts[i]->source().desc().ptr(),
		     pmErrStr(sts));
	    result = sts;
	    continue;
	}
	sts = pmSetMode(mode, when, interval);
	if (sts < 0) {
	    pmprintf("%s: Error: Unable to set context mode for %s: %s\n",
		     pmProgname, _contexts[i]->source().desc().ptr(),
		     pmErrStr(sts));
	    result = sts;
	}
    }
    sts = useContext();
    if (sts < 0)
	result = sts;
    return result;
}
