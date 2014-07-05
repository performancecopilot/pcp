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


#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xcms.h>

#include <Vk/VkApp.h>

#include "Inv.h"
#include "App.h"
#include "ColorList.h"

ColorList	theColorLists;

ColorList::~ColorList()
{
    uint_t	i;
    uint_t	j;

    for (i = 0; i < _colors.length(); i++) {
	ColorSpec &spec = *_colors[i];
	for (j = 0; j < spec._list.length(); j++)
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
    uint_t	i;
    for (i = 0; i < _names.length(); i++)
	if (_names[i] == name)
	    return _colors[i];
    return NULL;
}

OMC_Bool
ColorList::add(const char *name, const char *scaleColor)
{
    if (list(name) != NULL)
	return OMC_false;

    _names.append(name);
    if (scaleColor != NULL) {
	_colors.append(new ColorSpec(OMC_true));
	if (addColor(scaleColor, 0.0) == OMC_false) {
	    _colors.tail()->_list.append(new SbColor(0.0, 0.0, 1.0));
	    _colors.tail()->_max.append(0.0);
	}
    }
    else
	_colors.append(new ColorSpec(OMC_false));

    return OMC_true;
}

OMC_Bool
ColorList::add(const char *name, float red, float green, float blue)
{
    if (list(name) != NULL)
	return OMC_false;

    _names.append(name);
    _colors.append(new ColorSpec(OMC_true));
    _colors.tail()->_list.append(new SbColor(red, green, blue));
    _colors.tail()->_max.append(0.0);

    return OMC_true;
}

OMC_Bool
ColorList::findColor(const char *color, float &red, float &green, float &blue)
{
    XcmsColor   col;
    XcmsColor   screenColor;
    Status      s;
    Display     *display = theApp->display();

    s = XcmsLookupColor(display, 
                        DefaultColormap(display, DefaultScreen(display)),
                        color, &col, &screenColor, XcmsRGBiFormat);

    if (s == XcmsFailure)
        return OMC_false;

    red = col.spec.RGBi.red;
    green = col.spec.RGBi.green;
    blue = col.spec.RGBi.blue;

    return OMC_true;
}

OMC_Bool
ColorList::findColor(const char *color)
{
    XcmsColor   col;
    XcmsColor   screenColor;
    Status      s;
    Display     *display = theApp->display();

    s = XcmsLookupColor(display, 
                        DefaultColormap(display, DefaultScreen(display)),
                        color, &col, &screenColor, XcmsRGBiFormat);

    if (s == XcmsFailure)
        return OMC_false;

    _colors.tail()->_list.append(new SbColor(col.spec.RGBi.red,
					     col.spec.RGBi.green,
					     col.spec.RGBi.blue));

    return OMC_true;
}

OMC_Bool
ColorList::addColor(const char *color)
{
    assert(_colors.length() > 0);
    assert(_colors.tail()->_scale == OMC_false);
    return findColor(color);
}

OMC_Bool
ColorList::addColor(const char *color, double max)
{
    OMC_Bool result;

    assert(_colors.length() > 0);
    assert(_colors.tail()->_scale == OMC_true);
    result = findColor(color);
    if (result)
	_colors.tail()->_max.append(max);
    return result;
}

OMC_Bool
ColorList::addColor(float red, float green, float blue)
{
    if (red < 0.0 || red > 1.0 || green < 0.0 || green > 1.0 ||
	blue < 0.0 || blue > 1.0)
	return OMC_false;

    assert(_colors.length() > 0);
    assert(_colors.tail()->_scale == OMC_false);
    _colors.tail()->_list.append(new SbColor(red, green, blue));
    return OMC_true;
}

OMC_Bool
ColorList::addColor(float red, float green, float blue, double max)
{
    if (red < 0.0 || red > 1.0 || green < 0.0 || green > 1.0 ||
	blue < 0.0 || blue > 1.0)
	return OMC_false;

    assert(_colors.length() > 0);
    assert(_colors.tail()->_scale == OMC_true);
    _colors.tail()->_list.append(new SbColor(red, green, blue));
    _colors.tail()->_max.append(max);
    return OMC_true;
}

ostream&
operator<<(ostream& os, ColorList const& rhs)
{
    uint_t	i, j;
    float	r, g, b;

    for (i = 0; i < rhs.numLists(); i++) {
	const ColorSpec &list = *(rhs._colors[i]);
	os << '[' << i << "] = " << rhs._names[i] << ", scale = "
	   << (list._scale == OMC_true ? "true" : "false") << ": ";
	if (list._list.length()) {
	    for (j = 0; j < list._list.length(); j++) {
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

