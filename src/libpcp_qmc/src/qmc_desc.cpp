/*
 * Copyright (c) 1998,2005 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
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
 */

#include "qmc_desc.h"
#include <QTextStream>

QmcDesc::QmcDesc(pmID pmid)
{
    my.pmid = pmid;
    my.scaleFlag = false;
    my.status = pmLookupDesc(my.pmid, &my.desc);
    if (my.status >= 0) {
	my.scaleUnits = my.desc.units;
	setUnitStrings();
    }
    else if (pmDebug & DBG_TRACE_PMC) {
	QTextStream cerr(stderr);
	cerr << "QmcDesc::QmcDesc: unable to lookup "
	     << pmIDStr(my.pmid) << ": " << pmErrStr(my.status) << endl;
    }
}

void
QmcDesc::setUnitStrings()
{
    const char *units = pmUnitsStr(&my.scaleUnits);
    const char *shortUnits = shortUnitsString(&my.scaleUnits);

    if (my.desc.sem == PM_SEM_COUNTER) {
	if (my.scaleFlag &&
	    my.scaleUnits.dimTime == 1 &&
	    my.scaleUnits.dimSpace == 0 &&
	    my.scaleUnits.dimCount == 0) {
	    my.units = "Time Utilization";
	    my.shortUnits = "util";
	}
	else {
	    my.units = units;
	    my.units.append(" / second");
	    my.shortUnits = shortUnits;
	    my.shortUnits.append("/s");
	}
    }
    else {
	if (units[0] == '\0')
	    my.units = "none";
	else
	    my.units = units;
	if (shortUnits[0] == '\0')
	    my.shortUnits = "none";
	else
	    my.shortUnits = shortUnits;
    }
}

void
QmcDesc::setScaleUnits(const pmUnits &units)
{
    my.scaleUnits = units;
    my.scaleFlag = true;
    setUnitStrings();
}

const char *
QmcDesc::shortUnitsString(pmUnits *pu)
{
    const char *spaceString, *timeString, *countString;
    char sbuf[20], tbuf[20], cbuf[20];
    static char buf[60];
    char *p;

    spaceString = timeString = countString = NULL;
    buf[0] = '\0';

    if (pu->dimSpace) {
	switch (pu->scaleSpace) {
	    case PM_SPACE_BYTE:
		spaceString = "b";
		break;
	    case PM_SPACE_KBYTE:
		spaceString = "Kb";
		break;
	    case PM_SPACE_MBYTE:
		spaceString = "Mb";
		break;
	    case PM_SPACE_GBYTE:
		spaceString = "Gb";
		break;
	    case PM_SPACE_TBYTE:
		spaceString = "Tb";
		break;
	    default:
		sprintf(sbuf, "space-%d", pu->scaleSpace);
		spaceString = sbuf;
		break;
	}
    }
    if (pu->dimTime) {
	switch (pu->scaleTime) {
	    case PM_TIME_NSEC:
		timeString = "ns";
		break;
	    case PM_TIME_USEC:
		timeString = "us";
		break;
	    case PM_TIME_MSEC:
		timeString = "msec";
		break;
	    case PM_TIME_SEC:
		timeString = "s";
		break;
	    case PM_TIME_MIN:
		timeString = "m";
		break;
	    case PM_TIME_HOUR:
		timeString = "h";
		break;
	    default:
		sprintf(tbuf, "time-%d", pu->scaleTime);
		timeString = tbuf;
		break;
	}
    }
    if (pu->dimCount) {
	switch (pu->scaleCount) {
	    case 0:
		countString = "c";
		break;
	    case 1:
		countString = "cx10";
		break;
	    default:
		sprintf(cbuf, "cx10^%d", pu->scaleCount);
		countString = cbuf;
		break;
	}
    }

    p = buf;

    if (pu->dimSpace > 0) {
	if (pu->dimSpace == 1)
	    sprintf(p, "%s", spaceString);
	else
	    sprintf(p, "%s^%d", spaceString, pu->dimSpace);
	while (*p) p++;
    }
    if (pu->dimTime > 0) {
	if (pu->dimTime == 1)
	    sprintf(p, "%s", timeString);
	else
	    sprintf(p, "%s^%d", timeString, pu->dimTime);
	while (*p) p++;
    }
    if (pu->dimCount > 0) {
	if (pu->dimCount == 1)
	    sprintf(p, "%s", countString);
	else
	    sprintf(p, "%s^%d", countString, pu->dimCount);
	while (*p) p++;
    }
    if (pu->dimSpace < 0 || pu->dimTime < 0 || pu->dimCount < 0) {
	*p++ = '/';
	if (pu->dimSpace < 0) {
	    if (pu->dimSpace == -1)
		sprintf(p, "%s", spaceString);
	    else
		sprintf(p, "%s^%d", spaceString, -pu->dimSpace);
	    while (*p) p++;
	}
	if (pu->dimTime < 0) {
	    if (pu->dimTime == -1)
		sprintf(p, "%s", timeString);
	    else
		sprintf(p, "%s^%d", timeString, -pu->dimTime);
	    while (*p) p++;
	}
	if (pu->dimCount < 0) {
	    if (pu->dimCount == -1)
		sprintf(p, "%s", countString);
	    else
		sprintf(p, "%s^%d", countString, -pu->dimCount);
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
