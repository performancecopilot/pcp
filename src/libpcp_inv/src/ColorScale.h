/* -*- C++ -*- */

#ifndef _INV_COLSCALE_H
#define _INV_COLSCALE_H

/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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


#include <Inventor/SbColor.h>
#include "List.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class INV_ColorStep
{
private:

    SbColor	_color;
    float	_min;

public:

    ~INV_ColorStep();

    INV_ColorStep(SbColor col, float val = 0.0);
    INV_ColorStep(float r, float g, float b, float val = 0.0);
    INV_ColorStep(uint32_t col, float val = 0.0);
    INV_ColorStep(const INV_ColorStep &rhs);

    const INV_ColorStep &operator=(const INV_ColorStep &);

    const SbColor &color() const
    	{ return _color; }
    SbColor &color()
    	{ return _color; }

    const float &min() const
    	{ return _min; }
    float &min()
    	{ return _min; }
};

typedef OMC_List<INV_ColorStep *> INV_ColStepList;

class INV_ColorScale
{
private:

    INV_ColStepList	_colors;

public:

    ~INV_ColorScale();

    INV_ColorScale(const SbColor &col);
    INV_ColorScale(float r, float g, float b);
    INV_ColorScale(uint32_t col);
    INV_ColorScale(const INV_ColorScale &);
    const INV_ColorScale &operator=(const INV_ColorScale &);

    uint_t numSteps() const
	{ return _colors.length(); }

    int add(INV_ColorStep *ptr);

    const INV_ColorStep &operator[](int i) const
	{ return *(_colors[i]); }
    INV_ColorStep &operator[](int i)
	{ return *(_colors[i]); }

    const INV_ColorStep &step(float);

friend ostream& operator<<(ostream &os, const INV_ColorScale &rhs);

private:

    INV_ColorScale();
    // Not defined
};

#endif /* _INV_COLSCALE_H */

