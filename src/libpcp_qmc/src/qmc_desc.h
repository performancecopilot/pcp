/*
 * Copyright (c) 1998-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef QMC_DESC_H
#define QMC_DESC_H

#include "qmc.h"
#include <qstring.h>

class QmcDesc
{
public:
    QmcDesc(pmID pmid);

    int status() const	{ return my.status; }
    pmID id() const	{ return my.pmid; }
    pmDesc desc() const	{ return my.desc; }
    const pmDesc *descPtr() const	{ return &my.desc; }
    const QString units() const		{ return my.units; }
    const QString shortUnits() const	{ return my.shortUnits; }
    const pmUnits &scaleUnits() const	{ return my.scaleUnits; }

    void setScaleUnits(const pmUnits &units);

    // Are we using scaled units provided by a call to setScaleUnits?
    bool useScaleUnits() const	{ return my.scaleFlag; }

private:
    struct {
	int status;
	pmID pmid;
	pmDesc desc;
	QString units;
	QString shortUnits;
	pmUnits scaleUnits;
	bool scaleFlag;
    } my;

    void setUnitStrings();
    static const char *shortUnitsString(pmUnits *pu);
};

#endif	// QMC_DESC_H
