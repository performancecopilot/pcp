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


#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include "BaseObj.h"
#include "DefaultObj.h"

BaseObj::~BaseObj()
{
    delete(_mod);
}

BaseObj::BaseObj(OMC_Bool onFlag, 
		 const DefaultObj &defaults,
		 uint_t x, uint_t y, 
		 uint_t cols, uint_t rows, 
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

    _mod = new INV_ToggleMod(cubeSep, _label.ptr());
    sep->addChild(_mod->root());

    if (_on) {
	SoTranslation *tran2 = new SoTranslation;
	tran2->translation.setValue(0.0, _baseHeight / 2.0, 0.0);
	sep->addChild(tran2);
    }
}

void
BaseObj::setBaseSize(uint_t width, uint_t depth)
{
    if (_on) {
	_cube->width = width;
	_cube->depth = depth;
    }
}

#if 0
void
BaseObj::setTran(float xTran, float zTran, uint_t setWidth, uint_t setDepth)
{
    ViewObj::setTran(xTran, zTran, setWidth, setDepth);
}

#endif

ostream&
operator<<(ostream& os, BaseObj const& rhs)
{
    rhs.display(os);
    return os;
}

void
BaseObj::display(ostream& os) const
{
    ViewObj::display(os);
    os << ", border: X = " << _borderX << ", Z = " << _borderZ
       << ", height = " << _baseHeight << ", length = " << _length
       << ", maxHeight = " << _maxHeight << ", color = " << _baseColor[0]
       << ',' << _baseColor[1] << ',' << _baseColor[2] << ", on = "
       << (_on == OMC_true ? "true" : "false") << ", label = "
       << (_label == "\n" ? "\\n" : _label.ptr());
}

void
BaseObj::add(INV_Modulate *mod)
{
    _mod->addMod(mod);
}

void
BaseObj::finishedAdd()
{ 
}
