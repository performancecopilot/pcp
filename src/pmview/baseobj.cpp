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
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include "baseobj.h"
#include "defaultobj.h"

BaseObj::~BaseObj()
{
    delete(_mod);
}

BaseObj::BaseObj(bool onFlag, 
		 const DefaultObj &defaults,
		 int x, int y, 
		 int cols, int rows, 
		 BaseObj::Alignment align)
: ViewObj(x, y, cols, rows, align),
  _on(onFlag),
  _borderX(defaults.baseBorderX()),
  _borderZ(defaults.baseBorderZ()),
  _baseHeight(defaults.baseHeight()),
  _length(defaults.barLength()),
  _maxHeight(defaults.barHeight()),  
  _mod(0),
  _cube(0),
  _label("\n")
{
    _objtype |= BASEOBJ;

    _baseColor[0] = defaults.baseColor(0);
    _baseColor[1] = defaults.baseColor(1);
    _baseColor[2] = defaults.baseColor(2);
}

void
BaseObj::addBase(SoSeparator *sep)
{
    SoSeparator *cubeSep = new SoSeparator;

    if (_on) {
	SoTranslation *tran = new SoTranslation;
	tran->translation.setValue(0.0, _baseHeight / 2, 0.0);
	sep->addChild(tran);
	SoBaseColor *col = new SoBaseColor;
	col->rgb.setValue(_baseColor[0], _baseColor[1], _baseColor[2]);
	cubeSep->addChild(col);
	_cube = new SoCube;
	_cube->width = baseWidth();
	_cube->depth = baseDepth();
	_cube->height = _baseHeight;
	cubeSep->addChild(_cube);
    }

    _mod = new ToggleMod(cubeSep, (const char *)_label.toAscii());
    sep->addChild(_mod->root());

    if (_on) {
	SoTranslation *tran2 = new SoTranslation;
	tran2->translation.setValue(0.0, _baseHeight / 2.0, 0.0);
	sep->addChild(tran2);
    }
}

void
BaseObj::setBaseSize(int width, int depth)
{
    if (_on) {
	_cube->width = width;
	_cube->depth = depth;
    }
}

#if 0
void
BaseObj::setTran(float xTran, float zTran, int setWidth, int setDepth)
{
    ViewObj::setTran(xTran, zTran, setWidth, setDepth);
}

#endif

QTextStream&
operator<<(QTextStream& os, BaseObj const& rhs)
{
    rhs.display(os);
    return os;
}

void
BaseObj::display(QTextStream& os) const
{
    ViewObj::display(os);
    os << ", border: X = " << _borderX << ", Z = " << _borderZ
       << ", height = " << _baseHeight << ", length = " << _length
       << ", maxHeight = " << _maxHeight << ", color = " << _baseColor[0]
       << ',' << _baseColor[1] << ',' << _baseColor[2] << ", on = "
       << (_on == true ? "true" : "false") << ", label = "
       << (_label == "\n" ? "\\n" : _label);
}

void
BaseObj::add(Modulate *mod)
{
    _mod->addMod(mod);
}

void
BaseObj::finishedAdd()
{ 
}
