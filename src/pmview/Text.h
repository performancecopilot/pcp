/* -*- C++ -*- */

#ifndef _TEXT_H_
#define _TEXT_H_

/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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


#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoBaseColor.h>

#include "Bool.h"
#include "String.h"

class Text
{
 public:

    enum Direction { left, right, up, down, vertical };
    enum FontSize  { small, medium, large };

 private:

    int					_width;
    int					_depth;
    Direction				_dir;
    FontSize				_fontSize;
    OMC_Bool				_rightJustFlag;

    SoSeparator				*_root;
    SoTranslation			*_translation;

    static const char			*theHeightStr;
    static SoFont			*theSmallFont;
    static SoFont			*theMediumFont;
    static SoFont			*theLargeFont;

    static SbVec3f			theColor;
    static SoGetBoundingBoxAction	*theBoxAction;

 public:

    ~Text();

    Text(const OMC_String &theText, 
	 Direction theDir, 
	 FontSize theFontSize, 
	 OMC_Bool rightJust = OMC_false);

    int width() const
	{ return _width; }
    int depth() const
	{ return _depth; }
    Direction dir() const
	{ return _dir; }
    FontSize size() const
	{ return _fontSize; }

    SoSeparator* root() const
	{ return _root; }

    friend ostream& operator<<(ostream& os, Text const& rhs);

    void display(ostream& os) const;

 private:
    
    Text();
    Text(Text const &);
    Text const& operator=(Text const&);
};

#endif /* _TEXT_H_ */
