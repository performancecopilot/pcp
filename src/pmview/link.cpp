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
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoBaseColor.h>

#include "togglemod.h"
#include "link.h"
#include "defaultobj.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * Constructor for the Link.
 *    Note that we use different alignment for the base than the one
 *    for the scene. Base is always centered and we get the visual
 *    effect by addjusting the height of the cylinder(s).
 \* * * * * *  * * * * * * * * * * * * * * * * * * * * * * * */
Link::Link (const DefaultObj & defs,
	    int c, int r,
	    int colSpan, int rowSpan,
	    Alignment a)
    : BaseObj (false, defs, c, r, colSpan, rowSpan, center)
    , _tag ("\n")
{
    _objtype |= LINK;

    for ( int i=0; i < 3; i++) {
	_color[i] = defs.baseColor(i);
    }

    align = a;
    c1 = c2 = 0;
    cellHeight = defs.baseHeight();
    cellDepth = defs.baseHeight();
    cellWidth = defs.baseHeight();

    _width = colSpan * cellWidth;
    _depth = rowSpan * cellDepth;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * 
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void
Link::finishedAdd ()
{
    BaseObj::addBase (_root);

    SoBaseColor * color = new SoBaseColor;
    color->rgb.setValue (_color);
    _root->addChild (color);

    float radius = cellHeight * 0.4;

    SoSeparator * mainsep = new SoSeparator;

    SoTranslation * toCenter = new SoTranslation;
    toCenter->translation.setValue (_width/2.0, cellHeight/2.0, _depth/2.0);
    mainsep->addChild (toCenter);

    if ( align == northEast || align == northWest ||
	 align == southEast || align == southWest ) {
	SoRotationXYZ * mrot = new SoRotationXYZ;
	mrot->axis = SoRotationXYZ::Y;
	switch (align) {
	case northEast:
	    mrot->angle = 0;
	    break;

	case northWest:
	    mrot->angle = M_PI/2;
	    break;

	case southWest:
	    mrot->angle = M_PI;
	    break;

	case southEast:
	    mrot->angle = -(M_PI/2);
	    break;

	default:
	    break;
	}
	mainsep->addChild (mrot);


	// L-shape link - could leave without a separator, but
	// geometry calculation becomes messy, so just add one.
	SoSeparator * sp = new SoSeparator;

	SoSphere * s = new SoSphere;
	s->radius.setValue (radius);
	sp->addChild (s);

	t2 = new SoTranslation;
	t3 = new SoTranslation;

	c1 = new SoCylinder;
	c1->radius.setValue (radius);

	c2 = new SoCylinder;
	c2->radius.setValue (radius);

	if ( align == northEast || align == southWest ) {
	    c1->height.setValue (_depth/2.0);
	    c2->height.setValue (_width/2.0);

	    t2->translation.setValue (0, 0, _depth/-4.0);
	    t3->translation.setValue (_width/4.0, _depth/4.0, 0);
	} else {
	    c1->height.setValue (_width/2.0);
	    c2->height.setValue (_depth/2.0);

	    t2->translation.setValue (0, 0, _width/-4.0);
	    t3->translation.setValue (_depth/4.0, _width/4.0, 0);
	}

	sp->addChild (t2);

	SoRotationXYZ * r1 = new SoRotationXYZ;
	r1->axis = SoRotationXYZ::X;
	r1->angle = M_PI/2;

	sp->addChild (r1);
	sp->addChild (c1);
	sp->addChild (t3);

	SoRotationXYZ * r2 = new SoRotationXYZ;
	r2->axis = SoRotationXYZ::Z;
	r2->angle = M_PI/2;

	sp->addChild (r2);
	sp->addChild (c2);

	mainsep->addChild (sp);
    } else {
	SoRotationXYZ * r = new SoRotationXYZ;

	int h;

	if ( align == north || align == south ) { // Vertical pipe 
	    h = rows() * cellDepth;
	    
	    r->axis = SoRotationXYZ::X;
	    r->angle = M_PI/2;
	} else { // Horizontal pipe
	    h = cols() * cellWidth;
	    
	    r->axis = SoRotationXYZ::Z;
	    r->angle = M_PI/2;
	}

	mainsep->addChild (r);

	c1 = new SoCylinder;
	c1->radius.setValue (radius);
	c1->height.setValue (h);
	mainsep->addChild (c1);
    }


    ToggleMod * m = new ToggleMod (mainsep, (const char *)_tag.toAscii());
    _root->addChild (m->root());
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * Adjust the size of the link. Note, that we don't need to change the
 * reported width and depth of an object (Grid does not cope with such
 * changes very well) just adjust the length of cylinder(s).
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void
Link::setTran (float x, float z, int w, int d)
{
    if ( c1 && c2 ) {
	if ( align == northEast || align == southWest ) {
	    c1->height.setValue (d/2.0);
	    c2->height.setValue (w/2.0);

	    t2->translation.setValue (0, 0, d/-4.0);
	    t3->translation.setValue (w/4.0, d/4.0, 0);
	} else {
	    c1->height.setValue (w/2.0);
	    c2->height.setValue (d/2.0);

	    t2->translation.setValue (0, 0, w/-4.0);
	    t3->translation.setValue (d/4.0, w/4.0, 0);
	}
    } else {
	c1->height.setValue ((align == north || align == south ) ? d : w );
    }

    BaseObj::setTran (x, z, w, d);
}

void
Link::setTag (const char * s)
{
    _tag = s;

    if ( strchr (s, '\n') == NULL ) {
	_tag.append ("\n");
    }
}
