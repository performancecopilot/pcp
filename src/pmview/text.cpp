/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <Inventor/Qt/viewers/SoQtExaminerViewer.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/nodes/SoPickStyle.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoCube.h>
#include "main.h"
#include "text.h"

const char     		*Text::theHeightStr = "gjpqy|_";

SoFont			*Text::theSmallFont = (SoFont *)0;
SoFont			*Text::theMediumFont = (SoFont *)0;
SoFont			*Text::theLargeFont = (SoFont *)0;

SbVec3f			Text::theColor(1.0, 1.0, 1.0);
SoGetBoundingBoxAction	*Text::theBoxAction = (SoGetBoundingBoxAction *)0;

Text::~Text()
{
}

Text::Text(const QString &theString, 
	   Direction theDir, 
	   FontSize theFontSize,
	   bool rightJust)
: _width(0), 
  _depth(0), 
  _dir(theDir),
  _fontSize(theFontSize),
  _rightJustFlag(rightJust),
  _root(0),
  _translation(0)
{
    float	x = 0.0;
    float	y = 0.0;
    float	z = 0.0;
    int		width = 0;
    int		height = 0;
    SoRotation	*rot1;
    SoRotation	*rot2;
    SoText3	*theText;

    _root = new SoSeparator;
    _translation = new SoTranslation;
    theText = new SoText3;

    SoPickStyle *style = new SoPickStyle;
    style->style = SoPickStyle::UNPICKABLE;
    _root->addChild(style);

    if (theSmallFont == (SoFont *)0) {
	char * font = getenv ("PMVIEW_FONT");

	theSmallFont = new SoFont;
	theMediumFont = new SoFont;
	theLargeFont = new SoFont;

	if ( font != NULL ) {
	    theSmallFont->name.setValue(font);
	    theMediumFont->name.setValue(font);
	    theLargeFont->name.setValue(font);
#ifdef __sgi
	} else {
	    // On Irix we know that Helvetica-Narrow is shipped as part
	    // of  x_eoe.sw.Xfonts, so we're going to use it since it's
	    // an easier font to render compared to Times.
	    theSmallFont->name.setValue("Helvetica-Narrow");
	    theMediumFont->name.setValue("Helvetica-Narrow");
	    theLargeFont->name.setValue("Helvetica-Narrow");
#endif
	}

	theSmallFont->size.setValue(14);
	theMediumFont->size.setValue(24);
	theLargeFont->size.setValue(32);
	theSmallFont->ref();
	theMediumFont->ref();
	theLargeFont->ref();
	theBoxAction = new SoGetBoundingBoxAction(pmview->viewer()->getViewportRegion());
    }

    switch(_fontSize) {
    case small:
	_root->addChild(theSmallFont->copy());
	break;
    case medium:
	_root->addChild(theMediumFont->copy());
	break;
    case large:
	_root->addChild(theLargeFont->copy());
	break;
    default:
	if (pmDebugOptions.appl1)
	    cerr << "Text::Text: Illegal size specified" << endl;
	_fontSize = medium;
	_root->addChild(theMediumFont->copy());
    }

    _translation->translation.setValue(0.0, 0.0, 0.0);
    _root->addChild(_translation);

    if (pmDebugOptions.appl1) {
	SoSeparator *sep = new SoSeparator;
	_root->addChild(sep);
	SoBaseColor *col = new SoBaseColor;
	col->rgb.setValue(1.0, 0.0, 0.0);
	sep->addChild(col);
	SoCube *cube = new SoCube;
	cube->width.setValue(3);
	cube->depth.setValue(3);
	cube->height.setValue(20);
	sep->addChild(cube);
    }

    if (_dir != vertical) {

	rot1 = new SoRotation;
	rot1->rotation.setValue(SbVec3f(1,0,0), -M_PI/2);
	_root->addChild(rot1);

	switch(_dir) {
	case left:
	    rot2 = new SoRotation;
	    rot2->rotation.setValue(SbVec3f(0,0,1), M_PI);
	    _root->addChild(rot2);
	    break;
	case down:
	    rot2 = new SoRotation;
	    rot2->rotation.setValue(SbVec3f(0,0,1), -M_PI/2);
	    _root->addChild(rot2);
	    break;
	case up:
	    rot2 = new SoRotation;
	    rot2->rotation.setValue(SbVec3f(0,0,1), M_PI/2);
	    _root->addChild(rot2);
	    break;
	default:
	    break;
	}

	if (((_dir == left || _dir == up) && !_rightJustFlag) ||
	    ((_dir == right || _dir == down) && !_rightJustFlag)) {
	    theText->justification = SoText3::RIGHT;
	}

	theText->parts = SoText3::FRONT;
	theText->string.setValue((const char *)theString.toLatin1());
	_root->addChild(theText);

	_root->ref();
	theBoxAction->apply(_root);
	SbXfBox3f box = theBoxAction->getBoundingBox();
	box.getSize(x, y, z);

	if (x < 0.0 || y < 0.0 || z < 0.0) {
	    if (pmDebugOptions.appl2)
	    	cerr << "Text::Text: Bogus bounding box returned for \""
		     << theString << "\": x = " << x << ", y = " << y
		     << ", z = " << z << endl;
	    x = 0.0;
	    y = 0.0;
	    z = 0.0;
	}

	_width = (int)ceilf(x);
	_depth = (int)ceilf(z);

	const char *hasLow = strpbrk((const char *)theString.toLatin1(), theHeightStr);

	if (pmDebugOptions.appl1)
	    cerr << "Text::Text: " << theString << ": width = " 
		 << _width << " height = " << _depth << " low = " 
		 << ((hasLow != (char *)0) ? 1 : 0) << endl;

	switch(_dir) {
	case left:
	    if (hasLow != (char *)0) {
		_translation->translation.setValue(1, 1, _depth * 0.3);
		_depth = (int)(_depth * 1.2);
	    }
	    else
		_translation->translation.setValue(1, 1, 0);
	    _width += 2;
	    break;
	case right:
	    if (hasLow != (char *)0) {
		_translation->translation.setValue(1, 1, _depth * 0.85);
		_depth = (int)(_depth * 1.2);
	    }
	    else
		_translation->translation.setValue(1, 1, _depth);
	    _width += 2;
	    break;
	case down:
	    if (hasLow != (char *)0) {
		_translation->translation.setValue(_width * 0.3, 1, 1);
		_width = (int)(_width * 1.2);
	    }
	    else
		_translation->translation.setValue(0, 1, 1);
	    _depth += 2;
	    break;
	case up:
	    if (hasLow != (char *)0) {
		_translation->translation.setValue(_width * 0.85, 1, 1);
		_width = (int)(_width * 1.2);
	    }
	    else
		_translation->translation.setValue(_width, 1, 1);
	    _depth += 2;
	    break;
	default:
	    if (pmDebugOptions.appl1)
		cerr << "Text::Text: Illegal direction specified (" 
		     << (int)_dir << ")" << endl;
	    break;
	}
    }
    else {
	char c[2] = { ' ', '\0' };
	SoSeparator *sep;
	SoTranslation *tran;
	SoRotation *rot;
	width = 0;
	height = 0;

	for (int i = 0; i < theString.length(); i++) {
	    c[0] = ((const char *)theString.toLatin1())[i];
	    sep = new SoSeparator;
	    tran = new SoTranslation;
	    rot = new SoRotation;
	    rot->rotation.setValue(SbVec3f(1,0,0), -M_PI/2);
	    theText->string.setValue(c);
	    theText->parts = SoText3::FRONT;

	    _root->addChild(sep);
	    sep->addChild(rot);
	    sep->addChild(tran);
	    sep->addChild(theText);

	    theBoxAction->apply(sep);
	    SbXfBox3f box = theBoxAction->getBoundingBox();
	    box.getSize(x, y, z);
	    
	    width = (int)ceilf(x);
	    height = (int)ceilf(z);

// TODO: This bounding box is wrong.

	    if (width*2 > _width)
		_width = width*2;

	    _depth += (height + 2) * 2;

	    tran->translation.setValue(0, -_depth, 1);

	    if (i < theString.length() - 1) {
		theText = new SoText3;
	    }
	}
    }
}


QTextStream&
operator<<(QTextStream& os, Text const& rhs)
{
    rhs.display(os);
    return os;
}

void
Text::display(QTextStream& os) const
{
    os << "Text: dir = ";
    switch(_dir) {
    case left:
	os << "left";
	break;
    case right:
	os << "right";
	break;
    case up:
	os << "up";
	break;
    case down:
	os << "down";
	break;
    default:
	break;
    }
    os << ", font size = ";
    switch(_fontSize) {
    case small:
	os << "small";
	break;
    case medium:
	os << "medium";
	break;
    case large:
	os << "large";
	break;
    }
}
