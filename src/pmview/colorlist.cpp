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
#include "colorlist.h"
#include <QtGui/QColor>

ColorList	theColorLists;

ColorList::~ColorList()
{
    int	i, j;

    for (i = 0; i < _colors.size(); i++) {
	ColorSpec &spec = *_colors[i];
	for (j = 0; j < spec._list.size(); j++)
	    delete spec._list[j];
	delete _colors[i];
    }
}

ColorList::ColorList()
: _names(),
  _colors()
{
}

const ColorSpec *
ColorList::list(const char *name)
{
    int	i;

    for (i = 0; i < _names.size(); i++)
	if (_names[i] == name)
	    return _colors[i];
    return NULL;
}

bool
ColorList::add(const char *name, const char *scaleColor)
{
    if (list(name) != NULL)
	return false;

    _names.append(name);
    if (scaleColor != NULL) {
	_colors.append(new ColorSpec(true));
	if (addColor(scaleColor, 0.0) == false) {
	    _colors.last()->_list.append(new SbColor(0.0, 0.0, 1.0));
	    _colors.last()->_max.append(0.0);
	}
    }
    else
	_colors.append(new ColorSpec(false));

    return true;
}

bool
ColorList::add(const char *name, float red, float green, float blue)
{
    if (list(name) != NULL)
	return false;

    _names.append(name);
    _colors.append(new ColorSpec(true));
    _colors.last()->_list.append(new SbColor(red, green, blue));
    _colors.last()->_max.append(0.0);

    return true;
}

bool
ColorList::findColor(const char *color, float &red, float &green, float &blue)
{
    QColor col;

    col.setNamedColor(color);
    if (!col.isValid())
	return false;

    red = col.redF();
    green = col.greenF();
    blue = col.blueF();
    return true;
}

bool
ColorList::findColor(const char *color)
{
    QColor col;

    col.setNamedColor(color);
    if (!col.isValid())
	return false;

    _colors.last()->_list.append(
		new SbColor(col.redF(), col.greenF(), col.blueF()));
    return true;
}

bool
ColorList::addColor(const char *color)
{
    assert(_colors.size() > 0);
    assert(_colors.last()->_scale == false);
    return findColor(color);
}

bool
ColorList::addColor(const char *color, double max)
{
    bool result;

    assert(_colors.size() > 0);
    assert(_colors.last()->_scale == true);
    result = findColor(color);
    if (result)
	_colors.last()->_max.append(max);
    return result;
}

bool
ColorList::addColor(float red, float green, float blue)
{
    if (red < 0.0 || red > 1.0 || green < 0.0 || green > 1.0 ||
	blue < 0.0 || blue > 1.0)
	return false;

    assert(_colors.size() > 0);
    assert(_colors.last()->_scale == false);
    _colors.last()->_list.append(new SbColor(red, green, blue));
    return true;
}

bool
ColorList::addColor(float red, float green, float blue, double max)
{
    if (red < 0.0 || red > 1.0 || green < 0.0 || green > 1.0 ||
	blue < 0.0 || blue > 1.0)
	return false;

    assert(_colors.size() > 0);
    assert(_colors.last()->_scale == true);
    _colors.last()->_list.append(new SbColor(red, green, blue));
    _colors.last()->_max.append(max);
    return true;
}

QTextStream&
operator<<(QTextStream& os, ColorList const& rhs)
{
    int		i, j;
    float	r, g, b;

    for (i = 0; i < rhs.numLists(); i++) {
	const ColorSpec &list = *(rhs._colors[i]);
	os << '[' << i << "] = " << rhs._names[i] << ", scale = "
	   << (list._scale == true ? "true" : "false") << ": ";
	if (list._list.size()) {
	    for (j = 0; j < list._list.size(); j++) {
		list._list[j]->getValue(r, g, b);
		os << r << ',' << g << ',' << b;
		if (list._scale)
		    os << "<=" << list._max[j];
		os << ' ';
	    }
	}
	os << endl;
    }
    return os;
}

