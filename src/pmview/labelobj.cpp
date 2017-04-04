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

#include "pmview.h"
#include "labelobj.h"
#include "defaultobj.h"

LabelObj::~LabelObj()
{
    if (_text)
	delete _text;
}

LabelObj::LabelObj(const DefaultObj &defaults, 
		   int x, int y, 
		   int cols, int rows, 
		   ViewObj::Alignment align)
: ViewObj(x, y, cols, rows, align),
  _str(),
  _text(0),
  _dir(Text::right),
  _fontSize(Text::medium),
  _margin(defaults.labelMargin())
{
    _objtype |= LABELOBJ;

    _color[0] = defaults.labelColor(0);
    _color[1] = defaults.labelColor(1);
    _color[2] = defaults.labelColor(2);
}

LabelObj::LabelObj(Text::Direction dir, 
		   Text::FontSize fontSize,
		   const DefaultObj &defaults,
		   int x, int y, 
		   int cols, int rows, 
		   ViewObj::Alignment align)
: ViewObj(x, y, cols, rows, align),
  _str(),
  _text(0),
  _dir(dir),
  _fontSize(fontSize),
  _margin(defaults.labelMargin())
{
    _color[0] = defaults.labelColor(0);
    _color[1] = defaults.labelColor(1);
    _color[2] = defaults.labelColor(2);
}

void
LabelObj::finishedAdd()
{
    _text = new Text(_str, _dir, _fontSize);
    SoBaseColor *base = new SoBaseColor;
    base->rgb.setValue(_color[0], _color[1], _color[2]);
    _root->addChild(base);
    _root->addChild(_text->root());
}

void
LabelObj::setTran(float xTran, float zTran, int setWidth, int setDepth)
{
    switch(_text->dir()) {
    case Text::right:
	ViewObj::setTran(xTran + width() - _margin,
		     zTran + _margin,
		     setWidth, setDepth);
	break;
    case Text::down:
	ViewObj::setTran(xTran + _margin,
			 zTran + depth() - _margin,
			 setWidth, setDepth);
	break;
    default:
	ViewObj::setTran(xTran + _margin,
			 zTran + _margin,
			 setWidth, setDepth);
	break;
    }
}

QTextStream&
operator<<(QTextStream& os, LabelObj const& rhs)
{
    rhs.display(os);
    return os;
}

void
LabelObj::display(QTextStream& os) const
{
    ViewObj::display(os);
    os << ", label = \"" << _str << "\", margin = " << _margin << ", text = ";
    if (_text == NULL)
	os << "undefined";
    else
	os << *_text;
}

