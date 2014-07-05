/* -*- C++ -*- */

#ifndef _COLORLIST_H_
#define _COLORLIST_H_

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


#include "String.h"
#include "List.h"
#include "MetricList.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

struct ColorSpec
{
    OMC_Bool		_scale;
    INV_ColorList	_list;
    OMC_RealList	_max;

    ColorSpec(OMC_Bool scale)
	: _scale(scale) {}
};

typedef OMC_List<ColorSpec *> ColorsList;

class ColorList
{
private:

    OMC_StrList	_names;
    ColorsList	_colors;

public:

    virtual ~ColorList();

    ColorList();

    uint_t numLists() const
	{ return _colors.length(); }

    const ColorSpec *list(const char *name);

    OMC_Bool add(const char *name, const char *color = NULL);
    OMC_Bool add(const char *name, float red, float green, float blue);

    // Add colors
    OMC_Bool addColor(const char *color);
    OMC_Bool addColor(float red, float blue, float green);

    // Add scaled colors
    OMC_Bool addColor(const char *color, double max);
    OMC_Bool addColor(float red, float green, float blue, double max);

    static OMC_Bool findColor(const char *color, float &r, float &g, float &b);

    friend ostream& operator<<(ostream& os, ColorList const& rhs);

private:
    OMC_Bool findColor(const char *color);

    ColorList(ColorList const &);
    ColorList const& operator=(ColorList const &);
    // Never defined
};

extern ColorList	theColorLists;

#endif /* _COLORLIST_H_ */
