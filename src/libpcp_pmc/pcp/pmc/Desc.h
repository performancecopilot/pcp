/* -*- C++ -*- */

#ifndef _PMC_DESC_H_
#define _PMC_DESC_H_

/*
 * Copyright (c) 1998-2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: Desc.h,v 1.2 2005/05/10 00:46:37 kenmcd Exp $"

#include <pcp/pmc/PMC.h>
#include <pcp/pmc/String.h>

class PMC_Desc
{
private:

    int		_sts;
    pmID	_pmid;
    pmDesc	_desc;
    PMC_String	_units;
    PMC_String	_abvUnits;
    pmUnits	_scaleUnits;
    PMC_Bool	_scaleFlag;

public:

    ~PMC_Desc()
	{}

    // Get the descriptor for <pmid>
    PMC_Desc(pmID pmid);

    // Normal PCP status, <0 is an error
    int status() const
	{ return _sts; }

    // My PMID
    pmID id() const
	{ return _pmid; }

    // The real PCP descriptor
    pmDesc desc() const
	{ return _desc; }

    // Pointer to the real descriptor, so that we don't need to take
    // a pointer to a temporary
    const pmDesc *descPtr() const
	{ return &_desc; }

    // The unit string
    const PMC_String &units() const
	{ return _units; }

    // The abrieviated unit string
    const PMC_String &abvUnits() const
	{ return _abvUnits; }

    // Are we using scaled units provided by a call to setScaleUnits?
    PMC_Bool useScaleUnits() const
	{ return _scaleFlag; }

    // What are the scaling units
    const pmUnits &scaleUnits() const
	{ return _scaleUnits; }

    // Set the scaling units
    void setScaleUnits(const pmUnits &units);

private:

    void setUnitStrs();
    static const char *abvUnitsStr(pmUnits *pu);
};

#endif /* _PMC_DESC_H_ */
