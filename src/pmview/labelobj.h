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
#ifndef _LABELOBJ_H_
#define _LABELOBJ_H_

#include "text.h"
#include "viewobj.h"

class SoNode;
class SoSeparator;
class SoTranslation;

class LabelObj : public ViewObj
{
protected:

    QString		_str;
    Text		*_text;
    Text::Direction	_dir;
    Text::FontSize	_fontSize;
    int			_margin;
    float		_color[3];

public:

    virtual ~LabelObj();

    LabelObj(Text::Direction dir,
	     Text::FontSize fontSize,
	     const DefaultObj &defaults,
	     int x, int y, 
	     int cols = 1, int rows = 1, 
	     Alignment align = center);

    LabelObj(const DefaultObj &defaults,
	     int x, int y, 
	     int cols = 1, int rows = 1, 
	     Alignment align = center);

    const QString &str() const
	{ return _str; }
    Text::Direction dir() const
	{ return _dir; }
    Text::FontSize size() const
	{ return _fontSize; }
    int margin() const
	{ return _margin; }
    float color(int i) const
    	{ return _color[i]; }

    // Local Changes
    QString &str()
	{ return _str; }
    Text::Direction &dir()
	{ return _dir; }
    Text::FontSize &size()
	{ return _fontSize; }
    int &margin()
	{ return _margin; }
    void color(float r, float g, float b)
    	{ _color[0] = r; _color[1] = g; _color[2] = b; }

    virtual int width() const
	{ return _text->width() + (_margin * 2); }
    virtual int depth() const
	{ return _text->depth() + (_margin * 2); }

    virtual void setTran(float xTran, float zTran, int width, int depth);

    virtual void finishedAdd();

    // Output
    virtual void display(QTextStream& os) const;

    virtual const char* name() const
	{ return "Label"; }

    friend QTextStream& operator<<(QTextStream& os, LabelObj const& rhs);

private:

    LabelObj();
    LabelObj(LabelObj const &);
    LabelObj const& operator=(LabelObj const &);
};

#endif /* _LABELOBJ_H_ */
