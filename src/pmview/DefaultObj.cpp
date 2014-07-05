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


#include <Vk/VkResource.h>
#include <stdlib.h>
#include "pmapi.h"
#include "impl.h"
#include "DefaultObj.h"
#include "ColorList.h"

DefaultObj	*DefaultObj::theDefaultObj = NULL;

DefaultObj::DefaultObj()
: _baseBorderX(8),
  _baseBorderZ(8),
  _baseHeight(6),
  _barSpaceX(8),
  _barSpaceZ(8),
  _barSpaceLabel(6),
  _barLength(28),
  _barHeight(80),
  _labelMargin(5),
  _gridMinWidth(20),
  _gridMinDepth(20)
{
    _baseColor[0] = _baseColor[1] = _baseColor[2] = 0.15;
    _labelColor[0] = _labelColor[1] = _labelColor[2] = 1.0;
    _pipeLength = _barHeight;
}

const DefaultObj &
DefaultObj::defObj()
{
    if (!theDefaultObj) {
	theDefaultObj = new DefaultObj;
	theDefaultObj->getResources();
    }
    return *theDefaultObj;
}

DefaultObj &
DefaultObj::changeDefObj()
{
    if (!theDefaultObj) {
	theDefaultObj = new DefaultObj;
	theDefaultObj->getResources();
    }
    return *theDefaultObj;
}

DefaultObj::DefaultObj(const DefaultObj &rhs)
: _baseBorderX(rhs._baseBorderX),
  _baseBorderZ(rhs._baseBorderZ),
  _baseHeight(rhs._baseHeight),
  _barSpaceX(rhs._barSpaceX),
  _barSpaceZ(rhs._barSpaceZ),
  _barSpaceLabel(rhs._barSpaceLabel),
  _barLength(rhs._barLength),
  _barHeight(rhs._barHeight),
  _labelMargin(rhs._labelMargin),
  _gridMinWidth(rhs._gridMinWidth),
  _gridMinDepth(rhs._gridMinDepth),
  _pipeLength(rhs._pipeLength)
{
    uint_t i;

    for (i = 0; i < 3; i++) {
	_baseColor[i] = rhs._baseColor[i];
	_labelColor[i] = rhs._labelColor[i];
    }
}

const DefaultObj &
DefaultObj::operator=(const DefaultObj &rhs)
{
    uint_t i;

    if (this != &rhs) {
	_baseBorderX = rhs._baseBorderX;
	_baseBorderZ = rhs._baseBorderZ;
	_baseHeight = rhs._baseHeight;
	_barSpaceX = rhs._barSpaceX;
	_barSpaceZ = rhs._barSpaceZ;
	_barSpaceLabel = rhs._barSpaceLabel;
	_barLength = rhs._barLength;
	_barHeight = rhs._barHeight;
	_labelMargin = rhs._labelMargin;
	_gridMinWidth = rhs._gridMinWidth;
	_gridMinDepth = rhs._gridMinDepth;

	for (i = 0; i < 3; i++) {
	    _baseColor[i] = rhs._baseColor[i];
	    _labelColor[i] = rhs._labelColor[i];
	}
    }
    return *this;
}

ostream&
operator<<(ostream &os, const DefaultObj &rhs)
{
    os << "baseBorderX=" << rhs._baseBorderX;
    os << ", baseBorderZ=" << rhs._baseBorderZ;
    os << ", baseHeight=" << rhs._baseHeight;
    os << ", baseColor=" << rhs._baseColor[0] << ',' << rhs._baseColor[1]
       << ',' << rhs._baseColor[2] << endl;
    os << ", barSpaceX=" << rhs._barSpaceX;
    os << ", barSpaceZ=" << rhs._barSpaceZ;
    os << ", barSpaceLabel=" << rhs._barSpaceLabel;
    os << ", barLength=" << rhs._barLength;
    os << ", barHeight=" << rhs._barHeight;
    os << ", labelMargin=" << rhs._labelMargin;
    os << ", labelColor=" << rhs._labelColor[0] << ',' << rhs._labelColor[1]
       << ',' << rhs._labelColor[2] << endl;
    os << ", gridMinWidth=" << rhs._gridMinWidth;
    os << ", gridMinDepth=" << rhs._gridMinDepth;
    return os;
}

static uint_t
getResource(const char *label, uint_t def)
{
    char	*msg;
    char	*str;
    uint_t	result = def;

    str = VkGetResource(label, XmRString);
    if (str != NULL && strcmp(str, "default") != 0) {
	unsigned long tmp = strtoul(str, &msg, 10);
	if (*msg != '\0')
	    pmprintf("%s: Resource value \"%s\" for \"%s\" is not an unsigned "
		     "integer or \"default\". Using default value of %u\n",
		     pmProgname, str, label, def);
	else
	    result = (uint_t)tmp;
    }

    return result;
}

void
getColorResource(const char *label, float &red, float &green, float &blue)
{
    char *str;

    str = VkGetResource(label, XmRString);
    if (str != NULL && strcmp(str, "default") != 0) {
	if (ColorList::findColor(str, red,
				 green, blue) == OMC_false) {
	    pmprintf("%s: Unable to map color resource \"%s\" to \"%s\", "
		     "using default color rgbi:%f/%f/%f\n",
		     pmProgname, label, str, red, green, blue);
	}
    }
}

void
DefaultObj::getResources()
{

    _baseBorderX = getResource("baseBorderWidth", 8);
    _baseBorderZ = getResource("baseBorderDepth", 8);
    _baseHeight = getResource("baseHeight", 2);
    getColorResource("baseColor", _baseColor[0], _baseColor[1], _baseColor[2]);
    _barSpaceX = getResource("barSpaceWidth", 8);
    _barSpaceZ = getResource("barSpaceDepth", 8);
    _barSpaceLabel = getResource("barSapceLabel", 6);
    _barLength = getResource("barLength", 28);
    _barHeight = getResource("barHeight", 80);
    _labelMargin = getResource("labelMargin", 5);
    getColorResource("labelColor", _labelColor[0], _labelColor[1], 
		     _labelColor[2]);
    _gridMinWidth = getResource("gridMinWidth", 20);
    _gridMinDepth = getResource("gridMinDepth", 20);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "DefaultObj::getResources: " << *this << endl;
#endif
}
