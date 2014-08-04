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
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include "pmview.h"
#include "viewobj.h"

#include <iostream>
using namespace std;

int		ViewObj::theNumModObjects = 0;

ViewObj::~ViewObj()
{
}

ViewObj::ViewObj(int x, int z, int cols, int rows, Alignment align)
    : _root(new SoSeparator)
    , _tran(new SoTranslation)
    , _objtype(VIEWOBJ)
    , _col(x)
    , _row(z)
    , _cols(cols)
    , _rows(rows)
{
    _tran->translation.setValue(0.0, 0.0, 0.0);
    _root->addChild(_tran);

    switch(align) {
    case north:
	_xAlign = 0.5; 
	_zAlign = 0.0;
	break;
    case south:
	_xAlign = 0.5; 
	_zAlign = 1.0;
	break;
    case east:
	_xAlign = 1.0; 
	_zAlign = 0.5;
	break;
    case west:
	_xAlign = 0.0; 
	_zAlign = 0.5;
	break;
    case northEast:
	_xAlign = 1.0; 
	_zAlign = 0.0;
	break;
    case northWest:
	_xAlign = 0.0; 
	_zAlign = 0.0;
	break;
    case southEast:
	_xAlign = 1.0; 
	_zAlign = 1.0;
	break;
    case southWest:
	_xAlign = 0.0; 
	_zAlign = 1.0;
	break;
    case center:
    default:
	_xAlign = 0.5; 
	_zAlign = 0.5;
	break;
    }
}

SoNode *
ViewObj::object(Shape shape)
{
    static SoSeparator *cubeSep = NULL;
    static SoSeparator *cylSep = NULL;
    SoNode *obj;

    switch(shape) {
    case cylinder:
	if (cylSep == NULL) {
	    cylSep = new SoSeparator();
	    SoTranslation *cylTran = new SoTranslation;
	    cylTran->translation.setValue(0.0, 0.5, 0.0);
	    cylSep->addChild(cylTran);
	    SoCylinder *cyl = new SoCylinder;
	    cyl->radius.setValue(0.5);
	    cyl->height.setValue(1.0);
	    cylSep->addChild(cyl);
	}
	obj = cylSep;
	break;

    case cube:
    default:
	if (cubeSep == NULL) {
	    cubeSep = new SoSeparator();
	    SoTranslation *cubeTran = new SoTranslation;
	    cubeTran->translation.setValue(0.0, 0.5, 0.0);
	    cubeSep->addChild(cubeTran);
	    SoCube *cube = new SoCube;
	    cube->height = cube->width = cube->depth = 1.0;
	    cubeSep->addChild(cube);
	}
	obj = cubeSep;
	break;
    }

    return obj;
}

void
ViewObj::setTran(float xTran, float zTran, int setWidth, int setDepth)
{
    float x = xTran + ((setWidth - (float)width()) * _xAlign);
    float z = zTran + ((setDepth - (float)depth()) * _zAlign);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	cerr << "ViewObj::setTran: " << name() << ":" << endl;
	cerr << "x=" << x << ", xTran =" << xTran << ", setWidth=" 
	     << setWidth << ", width=" << width() << ", xAlign=" << _xAlign 
	     << endl;
	cerr << "z=" << z << ", zTran =" << zTran << ", setDepth=" 
	     << setDepth << ", depth=" << depth() << ", zAlign=" << _zAlign 
	     << endl << endl;
    }
#endif

    _tran->translation.setValue(x, 0.0, z);
}

QTextStream&
operator<<(QTextStream& os, ViewObj const& rhs)
{
    rhs.display(os);
    return os;
}

void
ViewObj::display(QTextStream& os) const
{
    os << name() << ": size = " << width() << "x" << depth() << " ("
       << _tran->translation.getValue()[0] << ','
       << _tran->translation.getValue()[1] << ','
       << _tran->translation.getValue()[2] << "), cols = " << _cols
       << ", rows = " << _rows << ", alignment = " << _xAlign << ','
       << _zAlign;
}

void
ViewObj::dumpShape(QTextStream& os, ViewObj::Shape shape) const
{
    if (shape == ViewObj::cube)
	os << "cube";
    else
	os << "cylinder";
}
