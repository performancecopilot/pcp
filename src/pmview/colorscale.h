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
#ifndef _COLORSCALE_H
#define _COLORSCALE_H

#include <Inventor/SbColor.h>
#include <QtCore/QTextStream>
#include <QtCore/QList>

class ColorStep
{
private:

    SbColor	_color;
    float	_min;

public:

    ~ColorStep();

    ColorStep(SbColor col, float val = 0.0);
    ColorStep(float r, float g, float b, float val = 0.0);
    ColorStep(uint32_t col, float val = 0.0);
    ColorStep(const ColorStep &rhs);

    const ColorStep &operator=(const ColorStep &);

    const SbColor &color() const
    	{ return _color; }
    SbColor &color()
    	{ return _color; }

    const float &min() const
    	{ return _min; }
    float &min()
    	{ return _min; }
};

typedef QList<ColorStep *> ColorStepList;

class ColorScale
{
private:

    ColorStepList	_colors;

public:

    ~ColorScale();

    ColorScale(const SbColor &col);
    ColorScale(float r, float g, float b);
    ColorScale(uint32_t col);
    ColorScale(const ColorScale &);
    const ColorScale &operator=(const ColorScale &);

    int numSteps() const
	{ return _colors.size(); }

    int add(ColorStep *ptr);

    const ColorStep &operator[](int i) const
	{ return *(_colors[i]); }
    ColorStep &operator[](int i)
	{ return *(_colors[i]); }

    const ColorStep &step(float);

    friend QTextStream &operator<<(QTextStream &os, const ColorScale &rhs);

private:

    ColorScale();
    // Not defined
};

#endif /* _COLORSCALE_H */
