/* -*- C++ -*- */

#ifndef _LABELOBJ_H_
#define _LABELOBJ_H_

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


#include "Text.h"
#include "ViewObj.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class SoNode;
class SoSeparator;
class SoTranslation;

class LabelObj : public ViewObj
{
protected:

    OMC_String		_str;
    Text		*_text;
    Text::Direction	_dir;
    Text::FontSize	_fontSize;
    uint_t		_margin;
    float		_color[3];

public:

    virtual ~LabelObj();

    LabelObj(Text::Direction dir,
	     Text::FontSize fontSize,
	     const DefaultObj &defaults,
	     uint_t x, uint_t y, 
	     uint_t cols = 1, uint_t rows = 1, 
	     Alignment align = center);

    LabelObj(const DefaultObj &defaults,
	     uint_t x, uint_t y, 
	     uint_t cols = 1, uint_t rows = 1, 
	     Alignment align = center);

    const OMC_String &str() const
	{ return _str; }
    Text::Direction dir() const
	{ return _dir; }
    Text::FontSize size() const
	{ return _fontSize; }
    uint_t margin() const
	{ return _margin; }
    float color(uint_t i) const
    	{ return _color[i]; }

    // Local Changes
    OMC_String &str()
	{ return _str; }
    Text::Direction &dir()
	{ return _dir; }
    Text::FontSize &size()
	{ return _fontSize; }
    uint_t &margin()
	{ return _margin; }
    void color(float r, float g, float b)
    	{ _color[0] = r; _color[1] = g; _color[2] = b; }

    virtual uint_t width() const
	{ return _text->width() + (_margin * 2); }
    virtual uint_t depth() const
	{ return _text->depth() + (_margin * 2); }

    virtual void setTran(float xTran, float zTran, uint_t width, uint_t depth);

    virtual void finishedAdd();

    // Output
    virtual void display(ostream& os) const;

    virtual const char* name() const
	{ return "Label"; }

    friend ostream& operator<<(ostream& os, LabelObj const& rhs);

private:

    LabelObj();
    LabelObj(LabelObj const &);
    LabelObj const& operator=(LabelObj const &);
};

#endif /* _LABELOBJ_H_ */
