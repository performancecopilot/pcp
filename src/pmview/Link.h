/* -*- C++ -*- */

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

#ifndef _LINK_H
#define _LINK_H

#include "BaseObj.h"

class SoCylinder;

class Link : public BaseObj {
public:
    Link (const DefaultObj &, uint_t, uint_t, uint_t, uint_t, Alignment);

    void finishedAdd ();
    void setTran (float, float, uint_t, uint_t);
    uint_t width () const { return _width; }
    uint_t depth () const { return _depth; }
    const char * name () const { return "Link"; }
    void setTag (const char *);

    virtual INV_Modulate *modObj() { return 0; }

private:
    Link();
    Link (Link const &);
    Link const& operator=(Link const &);

    uint_t cellWidth, cellHeight, cellDepth;
    uint_t _width, _depth;
    Alignment align;

    SoCylinder * c1, * c2;
    SoTranslation * t2, * t3;
    OMC_String _tag;

    float _color[3];
};

#endif
