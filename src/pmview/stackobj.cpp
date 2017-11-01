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
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoTransform.h>
#include "stackobj.h"
#include "colorlist.h"
#include "defaultobj.h"

StackObj::~StackObj()
{
    delete _stack;
}

StackObj::StackObj(StackMod::Height height, 
		   ViewObj::Shape shape,
		   bool baseFlag,
		   const DefaultObj &defaults,
		   int x, int y, 
		   int cols, int rows, 
		   BaseObj::Alignment align)
: ModObj(baseFlag, defaults, x, y, cols, rows, align),
  _width(0),
  _depth(0),
  _height(height),
  _stack(0),
  _text()
{
    _objtype |= STACKOBJ;
    _shape = shape;
}

void
StackObj::finishedAdd()
{
    int		i;
    
    BaseObj::addBase(_root);

    if (_metrics.numMetrics() == 0) {
	pmprintf("%s: Error: Stack object has no metrics\n",
		 pmGetProgname());
	_length = 0;
    }
    else {
	SoScale *blockScale = new SoScale();
	blockScale->scaleFactor.setValue(_length, _maxHeight, _length);
	_root->addChild(blockScale);

	if (_metrics.numMetrics()) {
	    const char *colName = (const char *)_colors.toLatin1();
	    const ColorSpec *colSpec = theColorLists.list(colName);
	    if (colSpec != NULL) {
		if (colSpec->_scale)
		    pmprintf("%s: Warning: Color scale ignored for stack object.\n",
			     pmGetProgname());
		else {
		    if (pmDebugOptions.appl0)
			cerr << "StackObj::finishedAdd: Adding " 
			     << colSpec->_list.length()
			     << " colors for " << _metrics.numMetrics() 
			     << " metrics" << endl;

		    for (i = 0; i < colSpec->_list.size(); i++)
			_metrics.add(*(colSpec->_list)[i]);
		}
	    }
	    else
		pmprintf("%s: Warning: No colours specified for stack object, "
			 "defaulting to blue.\n", pmGetProgname());

	    _metrics.resolveColors(MetricList::perValue);

	    if (pmDebugOptions.appl0)
		cerr << "StackObj::finishedAdd: metrics: " << endl 
		     << _metrics << endl;

	    _stack = new StackMod(&_metrics, ViewObj::object(_shape), _height);
	    _root->addChild(_stack->root());

	    if (_text.length())
		_stack->setFillText((const char *)_text.toLatin1());

	    BaseObj::add(_stack);
	    ViewObj::theNumModObjects++;
	}
	else
	    _length = 0;
    }

    if (pmDebugOptions.appl0)
	cerr << name() << "has length " << _length << endl;

    _width = baseWidth() + _length;
    _depth = baseDepth() + _length;
}

void
StackObj::setTran(float xTran, float zTran, int setWidth, int setDepth)
{
    BaseObj::setBaseSize(width(), depth());
    BaseObj::setTran(xTran + (width() / 2.0),
		     zTran + (depth() / 2.0),
		     setWidth, setDepth);
}

QTextStream&
operator<<(QTextStream& os, StackObj const& rhs)
{
    rhs.display(os);
    return os;
}

void
StackObj::display(QTextStream& os) const
{
    BaseObj::display(os);
    os << ", length = " << _length << ": ";
    if (_stack)
	os << *_stack << endl;
    else
	os << "stack undefined!" << endl;
}
