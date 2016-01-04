/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
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
 */
#ifndef _DEFAULTOBJ_H_
#define _DEFAULTOBJ_H_

#include "main.h"
#include <QTextStream>

class DefaultObj
{
private:

    static DefaultObj	*theDefaultObj;

    int			_baseBorderX;
    int			_baseBorderZ;
    int			_baseHeight;
    float		_baseColor[3];

    int			_barSpaceX;
    int			_barSpaceZ;
    int			_barSpaceLabel;
    int			_barLength;
    int			_barHeight;

    int			_labelMargin;
    float		_labelColor[3];

    int			_gridMinWidth;
    int			_gridMinDepth;
    int			_pipeLength;

public:

    ~DefaultObj()
	{}

    DefaultObj();
    DefaultObj(const DefaultObj &);
    const DefaultObj &operator=(const DefaultObj &rhs);

    static const DefaultObj &defObj();

    // Query
    int baseBorderX() const
	{ return _baseBorderX; }
    int baseBorderZ() const
	{ return _baseBorderZ; }
    int baseHeight() const
	{ return _baseHeight; }
    float baseColor(int i) const
	{ return _baseColor[i]; }
    int barSpaceX() const
	{ return _barSpaceX; }
    int barSpaceZ() const
	{ return _barSpaceZ; }
    int barSpaceLabel() const
	{ return _barSpaceLabel; }
    int barLength() const
	{ return _barLength; }
    int barHeight() const
	{ return _barHeight; }
    int labelMargin() const
	{ return _labelMargin; }
    float labelColor(int i) const
	{ return _labelColor[i]; }
    int gridMinWidth() const
	{ return _gridMinWidth; }
    int gridMinDepth() const
	{ return _gridMinDepth; }
    int pipeLength () const
	{ return _pipeLength; }
	
    // Local Changes
    int &baseBorderX()
	{ return _baseBorderX; }
    int & pipeLength ()
	{ return _pipeLength; }
    int &baseBorderZ()
	{ return _baseBorderZ; }
    int &baseHeight()
	{ return _baseHeight; }
    void baseColor(float r, float g, float b)
	{ _baseColor[0] = r; _baseColor[1] = g; _baseColor[2] = b; }
    int &barSpaceX()
	{ return _barSpaceX; }
    int &barSpaceZ()
	{ return _barSpaceZ; }
    int &barSpaceLabel()
	{ return _barSpaceLabel; }
    int &barLength()
	{ return _barLength; }
    int &barHeight()
	{ return _barHeight; }
    int &labelMargin()
	{ return _labelMargin; }
    void labelColor(float r, float g, float b)
	{ _labelColor[0] = r; _labelColor[1] = g; _labelColor[2] = b; }
    int &gridMinWidth()
	{ return _gridMinWidth; }
    int &gridMinDepth()
	{ return _gridMinDepth; }

    // Global
    static void baseBorderX(int val)
	{ changeDefObj()._baseBorderX = val; }
    static void baseBorderZ(int val)
	{ changeDefObj()._baseBorderZ = val; }
    static void baseHeight(int val)
	{ changeDefObj()._baseHeight = val; }
    static void baseColors(float r, float g, float b)
	{ changeDefObj().baseColor(r, g, b); }
    static void barSpaceX(int val)
	{ changeDefObj()._barSpaceX = val; }
    static void barSpaceZ(int val)
	{ changeDefObj()._barSpaceZ = val; }
    static void barSpaceLabel(int val)
	{ changeDefObj()._barSpaceLabel = val; }
    static void barLength(int val)
	{ changeDefObj()._barLength = val; }
    static void barHeight(int val)
	{ changeDefObj()._barHeight = val; }
    static void labelMargin(int val)
	{ changeDefObj()._labelMargin = val; }
    static void labelColors(float r, float g, float b)
	{ changeDefObj().labelColor(r, g, b); }
    static void gridMinWidth(int val)
	{ changeDefObj()._gridMinWidth = val; }
    static void gridMinDepth(int val)
	{ changeDefObj()._gridMinDepth = val; }

    friend QTextStream& operator<<(QTextStream &os, const DefaultObj &rhs);

private:

    static DefaultObj &changeDefObj();
    void getResources();
};

#endif /* _DEFAULTOBJ_H_ */
