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

#ident "$Id: Desc.c++,v 1.2 2005/05/10 00:46:37 kenmcd Exp $"

#include <iostream.h>
#include <pcp/pmc/Desc.h>

PMC_Desc::PMC_Desc(pmID pmid)
: _sts(0), _pmid(pmid), _scaleFlag(PMC_false)
{
    _sts = pmLookupDesc(_pmid, &_desc);
    if (_sts >= 0) {
	_scaleUnits = _desc.units;
	setUnitStrs();
    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_PMC) {
	cerr << "PMC_Desc::PMC_Desc: unable to lookup "
	     << pmIDStr(_pmid) << ": " << pmErrStr(_sts) << endl;
    }
#endif
}

void
PMC_Desc::setUnitStrs()
{
    const char *units = pmUnitsStr(&_scaleUnits);
    const char *abvUnits = abvUnitsStr(&_scaleUnits);
    if (_desc.sem == PM_SEM_COUNTER) {
	// Time utilisation
	if (_scaleFlag &&
	    _scaleUnits.dimTime == 1 &&
	    _scaleUnits.dimSpace == 0 &&
	    _scaleUnits.dimCount == 0) {
	    _units = "Time Utilization";
	    _abvUnits = "util";
	}
	else {
	    _units = units;
	    _units.append(" / second");
	    _abvUnits = abvUnits;
	    _abvUnits.append("/s");
	}
    }
    else {
	if (units[0] == '\0')
	    _units = "none";
	else
	    _units = units;

	if (abvUnits[0] == '\0')
	    _abvUnits = "none";
	else
	    _abvUnits = abvUnits;
    }
}

const char *
PMC_Desc::abvUnitsStr(pmUnits *pu)
{
    char	*spacestr;
    char	*timestr;
    char	*countstr;
    char	*p;
    char	sbuf[20];
    char	tbuf[20];
    char	cbuf[20];
    static char	buf[60];

    buf[0] = '\0';

    if (pu->dimSpace) {
	switch (pu->scaleSpace) {
	    case PM_SPACE_BYTE:
		spacestr = "b";
		break;
	    case PM_SPACE_KBYTE:
		spacestr = "Kb";
		break;
	    case PM_SPACE_MBYTE:
		spacestr = "Mb";
		break;
	    case PM_SPACE_GBYTE:
		spacestr = "Gb";
		break;
	    case PM_SPACE_TBYTE:
		spacestr = "Tb";
		break;
	    default:
		sprintf(sbuf, "space-%d", pu->scaleSpace);
		spacestr = sbuf;
		break;
	}
    }
    if (pu->dimTime) {
	switch (pu->scaleTime) {
	    case PM_TIME_NSEC:
		timestr = "ns";
		break;
	    case PM_TIME_USEC:
		timestr = "us";
		break;
	    case PM_TIME_MSEC:
		timestr = "msec";
		break;
	    case PM_TIME_SEC:
		timestr = "s";
		break;
	    case PM_TIME_MIN:
		timestr = "m";
		break;
	    case PM_TIME_HOUR:
		timestr = "h";
		break;
	    default:
		sprintf(tbuf, "time-%d", pu->scaleTime);
		timestr = tbuf;
		break;
	}
    }
    if (pu->dimCount) {
	switch (pu->scaleCount) {
	    case 0:
		countstr = "c";
		break;
	    case 1:
		countstr = "cx10";
		break;
	    default:
		sprintf(cbuf, "cx10^%d", pu->scaleCount);
		countstr = cbuf;
		break;
	}
    }

    p = buf;

    if (pu->dimSpace > 0) {
	if (pu->dimSpace == 1)
	    sprintf(p, "%s", spacestr);
	else
	    sprintf(p, "%s^%d", spacestr, pu->dimSpace);
	while (*p) p++;
    }
    if (pu->dimTime > 0) {
	if (pu->dimTime == 1)
	    sprintf(p, "%s", timestr);
	else
	    sprintf(p, "%s^%d", timestr, pu->dimTime);
	while (*p) p++;
    }
    if (pu->dimCount > 0) {
	if (pu->dimCount == 1)
	    sprintf(p, "%s", countstr);
	else
	    sprintf(p, "%s^%d", countstr, pu->dimCount);
	while (*p) p++;
    }
    if (pu->dimSpace < 0 || pu->dimTime < 0 || pu->dimCount < 0) {
	*p++ = '/';
	if (pu->dimSpace < 0) {
	    if (pu->dimSpace == -1)
		sprintf(p, "%s", spacestr);
	    else
		sprintf(p, "%s^%d", spacestr, -pu->dimSpace);
	    while (*p) p++;
	}
	if (pu->dimTime < 0) {
	    if (pu->dimTime == -1)
		sprintf(p, "%s", timestr);
	    else
		sprintf(p, "%s^%d", timestr, -pu->dimTime);
	    while (*p) p++;
	}
	if (pu->dimCount < 0) {
	    if (pu->dimCount == -1)
		sprintf(p, "%s", countstr);
	    else
		sprintf(p, "%s^%d", countstr, -pu->dimCount);
	    while (*p) p++;
	}
    }

    if (buf[0] == '\0') {
	if (pu->scaleCount == 1)
	    sprintf(buf, "x10");
	else if (pu->scaleCount != 0)
	    sprintf(buf, "x10^%d", pu->scaleCount);
    }
    else
	*p = '\0';

    return buf;
}

void
PMC_Desc::setScaleUnits(const pmUnits &units)
{
    _scaleUnits = units;
    _scaleFlag = PMC_true;
    setUnitStrs();
}
