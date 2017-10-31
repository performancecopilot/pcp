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
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoScale.h>

#include "stackmod.h"
#include "togglemod.h"
#include "pipeobj.h"
#include "defaultobj.h"
#include "colorlist.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * Constructor for Pipe.
 *    Note that similar to Link, pipes are always centered
 *    relative to the grid. The aligment is used to decide
 *    which end of pipe is decorated and in what direction
 *    pipe is oriented.
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
PipeObj::PipeObj (const DefaultObj & defs,
		  int c, int r,
		  int colSpan, int rowSpan,
		  BaseObj::Alignment align)
    : ModObj (false, defs, c, r, colSpan, rowSpan, center)
    , _align(align)
    , _tag ("\n")
{
    _objtype |= PIPEOBJ;

    for ( int i=0; i < 3; i++) {
	_color[i] = defs.baseColor(i);
    }

    _cylTrans = 0;
    _origt = 0;
    _cyl = 0;
    _stackHeight = defs.pipeLength();
    cellHeight = defs.baseHeight();

    if ( _align == north || _align == south ) {
	cellWidth = defs.baseHeight();
	cellDepth = (defs.pipeLength() < cellHeight) ?
	    cellHeight : defs.pipeLength();
    } else {
	cellWidth = (defs.pipeLength() < cellHeight) ? 
	    cellHeight : defs.pipeLength();
	cellDepth = defs.baseHeight();
    }

    _width = colSpan * cellWidth;
    _depth = rowSpan * cellDepth;

}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * 
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void
PipeObj::finishedAdd ()
{
    BaseObj::addBase (_root);

    int h;
    SoSeparator * sp = new SoSeparator;
    _root->addChild (sp);

    SoTranslation * st = new SoTranslation;
    st->translation.setValue (_width/2.0, cellHeight/2.0, _depth/2.0);
    sp->addChild (st);

    SoRotationXYZ * r = new SoRotationXYZ;
    if ( _align == north || _align == south ) { // Vertical Pipe
	h = _depth;

	r->axis = SoRotationXYZ::X;
	r->angle = M_PI/((_align == north) ? 2 : -2);
    } else { // Horizontal pipe
	h = _width;

	r->axis = SoRotationXYZ::Z;
	r->angle = M_PI/((_align == east) ? 2 : -2);
    }
    sp->addChild (r);

    SoSeparator * sceneSp = new SoSeparator;
    sp->addChild (sceneSp);

    _origt = new SoTranslation;
    _origt->translation.setValue (0, -h/2.0, 0);
    sceneSp->addChild (_origt);

    if (_metrics.numMetrics() ) {
	SoSeparator * stacksp = new SoSeparator;
	const ColorSpec * colSpec;
	SoScale *scale = new SoScale();
	scale->scaleFactor.setValue(cellHeight*0.8, _stackHeight,
				    cellHeight*0.8);
	stacksp->addChild(scale);

	if ((colSpec = theColorLists.list((const char *)_colors.toLatin1()))) {
	    if (colSpec->_scale)
		pmprintf(
		    "%s: Warning: Color scale cannot be applied to pipe\n",
		    pmGetProgname());
	    else {
		for (int i = 0; i < colSpec->_list.size(); i++)
		    _metrics.add(*(colSpec->_list)[i]);
	    }
	} else {
	    pmprintf("%s: Warning: No colours specified for pipe"
		     "defaulting to blue.\n", pmGetProgname());
	}

	_metrics.resolveColors(MetricList::perValue);
	
	StackMod * _stack = new StackMod(&_metrics, ViewObj::object (cylinder),
						 StackMod::fixed);
	_stack->setFillColor(_color);
	_stack->setFillText((const char *)_tag.toLatin1());

	stacksp->addChild(_stack->root());
	sceneSp->addChild(stacksp);

	BaseObj::add(_stack);
	ViewObj::theNumModObjects++;
    } else {
	pmprintf("%s: Error: no metrics for pipe\n", pmGetProgname());
    }

    SoBaseColor * color = new SoBaseColor;
    color->rgb.setValue (_color);
    sceneSp->addChild (color);

    _cylTrans = new SoTranslation;
    _cylTrans->translation.setValue (0, (h+_stackHeight)/2.0, 0);
    sceneSp->addChild (_cylTrans);


    _cyl = new SoCylinder;
    _cyl->radius.setValue (cellHeight*0.4);
    _cyl->height.setValue (h - _stackHeight);

    // In theory, something like "StaticMod", i.e no metrics, no
    // nothing should be provided, but togglemod seems to be
    // working Ok, so....
    ToggleMod * m = new ToggleMod (_cyl, (const char *)_tag.toLatin1());
    sceneSp->addChild (m->root());
}

void
PipeObj::setTran (float x, float z, int w, int d)
{
    if ( _cyl ) {
	float h = (_align == north || _align == south) ? d : w;

	_cylTrans->translation.setValue (0, (h + _stackHeight)/2.0, 0);
	_origt->translation.setValue (0, -h/2.0, 0);
	_cyl->height.setValue (h - _stackHeight);
    }

    ModObj::setTran (x, z, w, d);
}

void
PipeObj::setTag (const char * s)
{
    _tag = s;
    if ( strchr (s, '\n' ) == NULL ) {
	_tag.append ("\n");
    }
}
