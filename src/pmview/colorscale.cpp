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
#include "main.h"
#include "colorscale.h"

ColorStep::~ColorStep()
{
}

ColorStep::ColorStep(SbColor col, float val)
: _color(), 
  _min(val)
{
    _color.setValue(col.getValue());
}

ColorStep::ColorStep(float r, float g, float b, float val)
: _color(), 
  _min(val)
{
    SbColor tmp(r, g, b);
    _color.setValue(tmp.getValue());
}

ColorStep::ColorStep(uint32_t col, float val)
: _color(), 
  _min(val)
{
    float dummy = 0.0;
    _color.setPackedValue(col, dummy);
}

ColorStep::ColorStep(const ColorStep &rhs)
: _color(), 
  _min(rhs._min)
{
    _color.setValue(rhs._color.getValue());
}

const ColorStep &
ColorStep::operator=(const ColorStep &rhs)
{
    if (this != &rhs) {
	_color.setValue(rhs._color.getValue());
	_min = rhs._min;
    }
    return *this;
}

ColorScale::~ColorScale()
{
    int		i;

    for (i = 0; i < _colors.size(); i++)
	delete _colors.takeAt(i);
}

ColorScale::ColorScale(const SbColor &col)
: _colors()
{
    add(new ColorStep(col));
}

ColorScale::ColorScale(float r, float g, float b)
: _colors()
{
    add(new ColorStep(r, g, b));
}

ColorScale::ColorScale(uint32_t col)
: _colors()
{
    add(new ColorStep(col));
}

ColorScale::ColorScale(const ColorScale &rhs)
: _colors()
{
    int		i;

    for (i = 0; i < rhs._colors.size(); i++)
    	add(new ColorStep(rhs[i]));
}

const ColorScale &
ColorScale::operator=(const ColorScale &rhs)
{
    int		i;

    if (this != &rhs) {
    	for (i = 0; i < _colors.size(); i++)
	    delete _colors.takeAt(i);
	for (i = 0; i < rhs._colors.size(); i++)
	    add(new ColorStep(rhs[i]));
    }
    return *this;
}

int
ColorScale::add(ColorStep *ptr)
{
    if (_colors.size()) {
    	float prev = _colors.last()->min();
	if (prev >= ptr->min()) {
	    warningMsg(_POS_, 
		"Color step (%f) was less than previous step (%f), skipping.",
		ptr->min(), prev);
	    return -1;
	}
    }
    _colors.append(ptr);

    return 0;
}

const ColorStep &
ColorScale::step(float value)
{
    int		i = _colors.size();

    while (i > 0 && _colors[i-1]->min() > value)
    	i--;

    if (i == 0)
	return *(_colors[0]);
    return *(_colors[i-1]);
}

QTextStream&
operator<<(QTextStream &os, const ColorScale &rhs)
{
    int		i;

    if (rhs._colors.size() > 0) {
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

