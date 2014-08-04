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
#ifndef _TEXT_H_
#define _TEXT_H_

#include <QtCore/QTextStream>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoBaseColor.h>

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
    bool				_rightJustFlag;

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

    Text(const QString &theText, 
	 Direction theDir, 
	 FontSize theFontSize, 
	 bool rightJust = false);

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

    friend QTextStream& operator<<(QTextStream& os, Text const& rhs);

    void display(QTextStream& os) const;

 private:
    
    Text();
    Text(Text const &);
    Text const& operator=(Text const&);
};

#endif /* _TEXT_H_ */
