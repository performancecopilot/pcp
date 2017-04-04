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
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoClipPlane.h>
#include <Inventor/nodes/SoBaseColor.h>

#include "xing.h"
#include "defaultobj.h"

Xing::Xing (const DefaultObj & dobj, int c, int r,
	    int colSpan, int rowSpan,
	    Alignment corners[4])
    : BaseObj (false, dobj, c, r, colSpan, rowSpan, center) 
{
    int i;
    _objtype |= XING;

    for (i = 0; i < 3; i++) {
	_color[i] = dobj.baseColor(i);
    }

    _width = colSpan * _cellWidth;
    _depth = rowSpan * _cellDepth;
    _cellHeight = dobj.baseHeight();
    _cellWidth = dobj.baseHeight();
    _cellDepth = dobj.baseHeight();

    for ( i=0; i < 4; i++ ) {
	_corner[i] = corners[i];
    }
}

void
Xing::finishedAdd ()
{
    int i;
    float ts[12] = {
	(float)(_cellWidth/2.0), (float)(_cellHeight/2.0), (float)(_cellDepth/2.0),
	(float)(_width - _cellWidth), 0, 0,
	0, 0, (float)(_depth - _cellDepth),
	(float)(_cellWidth - _width), 0, 0
    };

    SoBaseColor * cl = new SoBaseColor;
    cl->rgb.setValue (_color);
    _root->addChild (cl);

    SoSeparator * scene = new SoSeparator;

    for (i=0; i < 4; i++ ) {
	SoTranslation * t = new SoTranslation;
	t->translation.setValue (ts+i*3);
	scene->addChild(t);

	SoSphere * s = new SoSphere;
	s->radius.setValue (_cellHeight*0.4);
	scene->addChild (s);

	SoSeparator * csep = new SoSeparator;
	
	float ch;

	ctrans[i] = new SoTransform;
	switch ( _corner[i] ) {
	case south:
	    ch = _cellDepth/2.0;
	    ctrans[i]->translation.setValue (0, 0, _cellDepth/4.0);
	    ctrans[i]->rotation.setValue (SbVec3f(1, 0, 0), M_PI/2);
	    break;

	case east:
	    ch = _cellWidth/2.0;
	    ctrans[i]->translation.setValue (_cellWidth/4.0, 0, 0);
	    ctrans[i]->rotation.setValue (SbVec3f(0, 0, 1), M_PI/2);
	    break;

	case west:
	    ch = _cellWidth/2.0;
	    ctrans[i]->translation.setValue (_cellWidth/-4.0, 0, 0);
	    ctrans[i]->rotation.setValue (SbVec3f(0, 0, 1), M_PI/2);
	    break;

	case north:
	    ch = _cellDepth/2.0;
	    ctrans[i]->translation.setValue (0, 0, _cellDepth/-4.0);
	    ctrans[i]->rotation.setValue (SbVec3f(1, 0, 0), M_PI/2);
	    break;

	default:
	    ch = 0.0;
	    break;
	}
	csep->addChild (ctrans[i]);

	cyls[i] = new SoCylinder;
	cyls[i]->radius.setValue (_cellHeight*0.4);
	cyls[i]->height.setValue (ch);
	csep->addChild (cyls[i]);

	scene->addChild (csep);
    }

    float cilh = sqrt (_cellWidth*_cellWidth*(cols()-1)*(cols()-1) +
			_cellDepth*_cellDepth*(rows()-1)*(rows()-1));

    // We're in the bottom left corner now
    SbVec3f axis(rows()-1, 0, cols()-1);
    SoTransform * tr = new SoTransform;
    tr->rotation.setValue (axis, M_PI/2);
    tr->translation.setValue (((cols()-1)*_cellWidth)/2.0, 0,
			      ((rows()-1)*_cellDepth)/-2.0);

    scene->addChild (tr);

    SoCylinder * c1 = new SoCylinder;
    c1->radius.setValue (_cellHeight*0.4);
    c1->height.setValue (cilh);
    scene->addChild (c1);

    axis[0] = cols() - 1;
    axis[2] = -((float)(rows()-1));

    // To draw a "broken" cylinder, define two clipping planes along the
    // "main" cylinder and draw the crossing one twice, each time enabling
    // a different plane.
    for (i=-1; i < 2; i += 2 ) {
	SoSeparator * sp = new SoSeparator;

	SbVec3f normal(i*((int)(rows()-1)), 0,  i*((int)(cols()-1)));
	SbPlane p (normal, _cellHeight/2.0);

	SoClipPlane * c = new SoClipPlane;
	c->on.setValue (1);
	c->plane = p;

	sp->addChild (c);

	SoRotation * r = new SoRotation;
	r->rotation.setValue (axis,  atanf (axis[0]/(rows()-1))*2.0);
	sp->addChild (r);
	sp->addChild (c1);

	scene->addChild (sp);
    }

    _root->addChild (scene);
}

void
Xing::setTran(float x, float z, int w, int d)
{
    float ch;

    for ( int i=0; i < 4; i++) {
	switch ( _corner[i] ) {
	case south:
	    ch = (d + _cellDepth - _depth)/2.0;
	    ctrans[i]->translation.setValue (0, 0, ch/2.0);
	    break;

	case east:
	    ch = (w + _cellWidth - _width)/2.0;
	    ctrans[i]->translation.setValue (ch/2.0, 0, 0);
	    break;

	case west:
	    ch = (w +_cellWidth - _width)/2.0;
	    ctrans[i]->translation.setValue (-ch/2.0, 0, 0);
	    break;

	case north:
	    ch = (d + _cellDepth - _depth)/2.0;
	    ctrans[i]->translation.setValue (0, 0, -ch/2.0);
	    break;

	default:
	    ch = 0.0;
	    break;
	}

	cyls[i]->height.setValue (ch);
    }

    BaseObj::setTran (x, z, w, d);
}
