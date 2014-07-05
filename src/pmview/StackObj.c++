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
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoTransform.h>

#include "StackObj.h"
#include "ColorList.h"
#include "DefaultObj.h"

StackObj::~StackObj()
{
    delete _stack;
}

StackObj::StackObj(INV_StackMod::Height height, 
		   ViewObj::Shape shape,
		   OMC_Bool baseFlag,
		   const DefaultObj &defaults,
		   uint_t x, uint_t y, 
		   uint_t cols, uint_t rows, 
		   BaseObj::Alignment align)
: ModObj(baseFlag, defaults, x, y, cols, rows, align),
  _width(0),
  _depth(0),
  _shape(shape),
  _height(height),
  _stack(0),
  _text()
{
    _objtype |= STACKOBJ;
}

void
StackObj::finishedAdd()
{
    uint_t	i;
    
    BaseObj::addBase(_root);

    if (_metrics.numMetrics() == 0) {
	pmprintf("%s: Error: Stack object has no metrics\n",
		 pmProgname);
	_length = 0;
    }
    else {
	SoScale *blockScale = new SoScale();
	blockScale->scaleFactor.setValue(_length, _maxHeight, _length);
	_root->addChild(blockScale);

	if (_metrics.numMetrics()) {
	    const ColorSpec *colSpec = theColorLists.list(_colors.ptr());
	    if (colSpec != NULL) {
		if (colSpec->_scale)
		    pmprintf("%s: Warning: Color scale ignored for stack object.\n",
			     pmProgname);
		else {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0)
			cerr << "StackObj::finishedAdd: Adding " 
			     << colSpec->_list.length()
			     << " colors for " << _metrics.numMetrics() 
			     << " metrics" << endl;
#endif

		    for (i = 0; i < colSpec->_list.length(); i++)
			_metrics.add(*(colSpec->_list)[i]);
		}
	    }
	    else
		pmprintf("%s: Warning: No colours specified for stack object, "
			 "defaulting to blue.\n", pmProgname);

	    _metrics.resolveColors(INV_MetricList::perValue);

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		cerr << "StackObj::finishedAdd: metrics: " << endl 
		     << _metrics << endl;
#endif

	    _stack = new INV_StackMod(&_metrics, 
				      ViewObj::object(_shape), 
				      _height);
	    _root->addChild(_stack->root());

	    if (_text.length())
		_stack->setFillText(_text.ptr());

	    BaseObj::add(_stack);
	    ViewObj::theNumModObjects++;
	}
	else
	    _length = 0;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << name() << "has length " << _length << endl;
#endif

    _width = baseWidth() + _length;
    _depth = baseDepth() + _length;
}

void
StackObj::setTran(float xTran, float zTran, uint_t setWidth, uint_t setDepth)
{
    BaseObj::setBaseSize(width(), depth());
    BaseObj::setTran(xTran + (width() / 2.0),
		     zTran + (depth() / 2.0),
		     setWidth, setDepth);
}

ostream&
operator<<(ostream& os, StackObj const& rhs)
{
    rhs.display(os);
    return os;
}

void
StackObj::display(ostream& os) const
{
    BaseObj::display(os);
    os << ", length = " << _length << ": ";
    if (_stack)
	os << *_stack << endl;
    else
	os << "stack undefined!" << endl;
}
