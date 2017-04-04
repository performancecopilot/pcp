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

#include <ctype.h>
#include "stackmod.h"
#include "togglemod.h"
#include "scenefileobj.h"
#include "defaultobj.h"
#include "colorlist.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * Constructor for Generic Inventor Scene Object.
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
SceneFileObj::SceneFileObj (const DefaultObj & defs,
		  int c, int r,
		  int colSpan, int rowSpan,
		  BaseObj::Alignment align)
    : ModObj (false, defs, c, r, colSpan, rowSpan, center)
    , _align(align)
    , _tag ("\n")
{
    _objtype |= SCENEFILEOBJ;

    for ( int i=0; i < 3; i++) {
	_color[i] = defs.baseColor(i);
    }

    cellHeight = defs.baseHeight();
    _stackHeight = defs.pipeLength();

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
    _sceneFileName[0] = '\0';
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * 
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void
SceneFileObj::finishedAdd ()
{
    SoSeparator *sp;

    BaseObj::addBase (_root);
    if ((sp = readSceneFile())) {
	_root->addChild (sp);
	// To make sure the viewer will display a scene with a single
	// IV scenegraph treat all IV scenegraphs as "modulated" objects.
	ViewObj::theNumModObjects++; 
    } else {
        pmprintf("Warning: Failed to read scene file \"%s\"\n", _sceneFileName);
    }
}

void
SceneFileObj::setTag (const char * s)
{
    _tag = s;
    if ( strchr (s, '\n' ) == NULL ) {
	_tag.append ("\n");
    }
}

SoSeparator *
SceneFileObj::readSceneFile(void)
{
    SoInput input;
    SoSeparator *s;
    FILE *f = NULL;

    if (_sceneFileName[0] == '<') {
	char *p = _sceneFileName+1;

	while (*p) {
	    if (! isdigit(*p))
		break;
	    p++;
	}

	if (*p == '\0') {
	    int fd = atoi(_sceneFileName+1);

	    if ((f = fdopen(fd, "r")) == NULL) {
		return (NULL);
	    }
	    input.setFilePointer(f);
	}
    }

    if (f == NULL) {
	if (!input.openFile(_sceneFileName))
	    return NULL;
    }
    s = SoDB::readAll(&input);
    input.closeFile();

    return s;
}
