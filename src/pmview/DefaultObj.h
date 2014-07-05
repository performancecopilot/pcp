/* -*- C++ -*- */

#ifndef _DEFAULTOBJ_H_
#define _DEFAULTOBJ_H_

/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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


#include "Bool.h"
#include "Inv.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class DefaultObj
{
private:

    static DefaultObj	*theDefaultObj;

    uint_t		_baseBorderX;
    uint_t		_baseBorderZ;
    uint_t		_baseHeight;
    float		_baseColor[3];

    uint_t		_barSpaceX;
    uint_t		_barSpaceZ;
    uint_t		_barSpaceLabel;
    uint_t		_barLength;
    uint_t		_barHeight;

    uint_t		_labelMargin;
    float		_labelColor[3];

    uint_t		_gridMinWidth;
    uint_t		_gridMinDepth;
    uint_t		_pipeLength;

public:

    ~DefaultObj()
	{}

    DefaultObj();
    DefaultObj(const DefaultObj &);
    const DefaultObj &operator=(const DefaultObj &rhs);

    static const DefaultObj &defObj();

    // Query
    uint_t baseBorderX() const
	{ return _baseBorderX; }
    uint_t baseBorderZ() const
	{ return _baseBorderZ; }
    uint_t baseHeight() const
	{ return _baseHeight; }
    float baseColor(uint_t i) const
	{ return _baseColor[i]; }
    uint_t barSpaceX() const
	{ return _barSpaceX; }
    uint_t barSpaceZ() const
	{ return _barSpaceZ; }
    uint_t barSpaceLabel() const
	{ return _barSpaceLabel; }
    uint_t barLength() const
	{ return _barLength; }
    uint_t barHeight() const
	{ return _barHeight; }
    uint_t labelMargin() const
	{ return _labelMargin; }
    float labelColor(uint_t i) const
	{ return _labelColor[i]; }
    uint_t gridMinWidth() const
	{ return _gridMinWidth; }
    uint_t gridMinDepth() const
	{ return _gridMinDepth; }
    uint_t pipeLength () const
	{ return _pipeLength; }
	
    // Local Changes
    uint_t &baseBorderX()
	{ return _baseBorderX; }
    uint_t & pipeLength ()
	{ return _pipeLength; }
    uint_t &baseBorderZ()
	{ return _baseBorderZ; }
    uint_t &baseHeight()
	{ return _baseHeight; }
    void baseColor(float r, float g, float b)
	{ _baseColor[0] = r; _baseColor[1] = g; _baseColor[2] = b; }
    uint_t &barSpaceX()
	{ return _barSpaceX; }
    uint_t &barSpaceZ()
	{ return _barSpaceZ; }
    uint_t &barSpaceLabel()
	{ return _barSpaceLabel; }
    uint_t &barLength()
	{ return _barLength; }
    uint_t &barHeight()
	{ return _barHeight; }
    uint_t &labelMargin()
	{ return _labelMargin; }
    void labelColor(float r, float g, float b)
	{ _labelColor[0] = r; _labelColor[1] = g; _labelColor[2] = b; }
    uint_t &gridMinWidth()
	{ return _gridMinWidth; }
    uint_t &gridMinDepth()
	{ return _gridMinDepth; }

    // Global
    static void baseBorderX(uint_t val)
	{ changeDefObj()._baseBorderX = val; }
    static void baseBorderZ(uint_t val)
	{ changeDefObj()._baseBorderZ = val; }
    static void baseHeight(uint_t val)
	{ changeDefObj()._baseHeight = val; }
    static void baseColors(float r, float g, float b)
	{ changeDefObj().baseColor(r, g, b); }
    static void barSpaceX(uint_t val)
	{ changeDefObj()._barSpaceX = val; }
    static void barSpaceZ(uint_t val)
	{ changeDefObj()._barSpaceZ = val; }
    static void barSpaceLabel(uint_t val)
	{ changeDefObj()._barSpaceLabel = val; }
    static void barLength(uint_t val)
	{ changeDefObj()._barLength = val; }
    static void barHeight(uint_t val)
	{ changeDefObj()._barHeight = val; }
    static void labelMargin(uint_t val)
	{ changeDefObj()._labelMargin = val; }
    static void labelColors(float r, float g, float b)
	{ changeDefObj().labelColor(r, g, b); }
    static void gridMinWidth(uint_t val)
	{ changeDefObj()._gridMinWidth = val; }
    static void gridMinDepth(uint_t val)
	{ changeDefObj()._gridMinDepth = val; }

    friend ostream& operator<<(ostream &os, const DefaultObj &rhs);

private:

    static DefaultObj &changeDefObj();
    void getResources();

};

#endif /* _VIEWOBJ_H_ */
