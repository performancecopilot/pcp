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
#include "defaultobj.h"
#include "colorlist.h"
#include <QSettings>

#include <iostream>
using namespace std;

DefaultObj	*DefaultObj::theDefaultObj;

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
    int i;

    for (i = 0; i < 3; i++) {
	_baseColor[i] = rhs._baseColor[i];
	_labelColor[i] = rhs._labelColor[i];
    }
}

const DefaultObj &
DefaultObj::operator=(const DefaultObj &rhs)
{
    int i;

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

QTextStream&
operator<<(QTextStream &os, const DefaultObj &rhs)
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

static void
getColorResource(const char *name, QString label, float &r, float &g, float &b)
{
    if (label != QString::null && label.compare("default") != 0) {
	const char *str = (const char *)label.toLatin1();
	if (ColorList::findColor(str, r, g, b) == false) {
	    pmprintf("%s: Unable to map color resource \"%s\" to \"%s\", "
		     "using default color rgbi:%f/%f/%f\n",
		     pmProgname, name, str, r, g, b);
	}
    }
}

void
DefaultObj::getResources()
{
    QString color;
    QSettings resources;
    resources.beginGroup(pmProgname);
    
    _baseBorderX = resources.value("baseBorderWidth", 8).toInt();
    _baseBorderZ = resources.value("baseBorderDepth", 8).toInt();
    _baseHeight = resources.value("baseHeight", 2).toInt();
    color = resources.value("baseColor", QString("default")).toString();
    getColorResource("baseColor", color,
			_baseColor[0], _baseColor[1], _baseColor[2]);
    _barSpaceX = resources.value("barSpaceWidth", 8).toInt();
    _barSpaceZ = resources.value("barSpaceDepth", 8).toInt();
    _barSpaceLabel = resources.value("barSpaceLabel", 6).toInt();
    _barLength = resources.value("barLength", 28).toInt();
    _barHeight = resources.value("barHeight", 80).toInt();
    _labelMargin = resources.value("labelMargin", 5).toInt();
    color = resources.value("labelColor", QString("default")).toString();
    getColorResource("labelColor", color,
			_labelColor[0], _labelColor[1], _labelColor[2]);
    _gridMinWidth = resources.value("gridMinWidth", 20).toInt();
    _gridMinDepth = resources.value("gridMinDepth", 20).toInt();

    resources.endGroup();

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "DefaultObj::getResources: " << *this << endl;
#endif
}
