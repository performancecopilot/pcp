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


#include "pmapi.h"
#include "impl.h"
#include "Inv.h"
#include "ColorScale.h"

INV_ColorStep::~INV_ColorStep()
{
}

INV_ColorStep::INV_ColorStep(SbColor col, float val)
: _color(), 
  _min(val)
{
    _color.setValue(col.getValue());
}

INV_ColorStep::INV_ColorStep(float r, float g, float b, float val)
: _color(), 
  _min(val)
{
    SbColor tmp(r, g, b);
    _color.setValue(tmp.getValue());
}

INV_ColorStep::INV_ColorStep(uint32_t col, float val)
: _color(), 
  _min(val)
{
    float dummy = 0.0;
    _color.setPackedValue(col, dummy);
}

INV_ColorStep::INV_ColorStep(const INV_ColorStep &rhs)
: _color(), 
  _min(rhs._min)
{
    _color.setValue(rhs._color.getValue());
}

const INV_ColorStep &
INV_ColorStep::operator=(const INV_ColorStep &rhs)
{
    if (this != &rhs) {
    	_color.setValue(rhs._color.getValue());
	_min = rhs._min;
    }
    return *this;
}

INV_ColorScale::~INV_ColorScale()
{
    uint_t	i;

    for (i = 0; i < _colors.length(); i++)
    	delete _colors[i];
    _colors.removeAll();
}

INV_ColorScale::INV_ColorScale(const SbColor &col)
: _colors()
{
    add(new INV_ColorStep(col));
}

INV_ColorScale::INV_ColorScale(float r, float g, float b)
: _colors()
{
    add(new INV_ColorStep(r, g, b));
}

INV_ColorScale::INV_ColorScale(uint32_t col)
: _colors()
{
    add(new INV_ColorStep(col));
}

INV_ColorScale::INV_ColorScale(const INV_ColorScale &rhs)
: _colors()
{
    uint_t	i;

    _colors.resize(rhs._colors.length());
    for (i = 0; i < rhs._colors.length(); i++)
    	add(new INV_ColorStep(rhs[i]));
}

const INV_ColorScale &
INV_ColorScale::operator=(const INV_ColorScale &rhs)
{
    uint_t	i;

    if (this != &rhs) {
    	for (i = 0; i < _colors.length(); i++)
	    delete _colors[i];
	_colors.removeAll();
	for (i = 0; i < rhs._colors.length(); i++)
	    add(new INV_ColorStep(rhs[i]));
    }
    return *this;
}

int
INV_ColorScale::add(INV_ColorStep *ptr)
{
    if (_colors.length()) {
    	float prev = _colors.tail()->min();
	if (prev >= ptr->min()) {
	    INV_warningMsg(_POS_, 
			   "Color step (%f) was less than previous step (%f), skipping.",
			   ptr->min(), prev);
	    return -1;
	}
    }
    _colors.append(ptr);

    return 0;
}

const INV_ColorStep &
INV_ColorScale::step(float value)
{
    uint_t	i = _colors.length();

    while (i > 0 && _colors[i-1]->min() > value)
    	i--;

    if (i == 0)
	return *(_colors[0]);
    return *(_colors[i-1]);
}

ostream&
operator<<(ostream &os, const INV_ColorScale &rhs)
{
    uint_t	i;

    if (rhs._colors.length() > 0) {
        os << '[' << rhs[0].min();
	for (i = 1; i < rhs.numSteps(); i++)
	    os << ", " << rhs[i].min();
	os << ']';
    }
    else {
    	os << "empty";
    }

    return os;
}

