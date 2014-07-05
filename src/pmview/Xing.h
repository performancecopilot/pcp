/*-*- C++ -*-*/
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


#ifndef _XING_H_
#define _XING_H_

#include "BaseObj.h"

class SoTransform;
class SoCylinder;

class Xing : public BaseObj {
public:
    Xing (const DefaultObj &, uint_t, uint_t, uint_t, uint_t, Alignment[4]);

    void finishedAdd ();
    void setTran (float, float, uint_t, uint_t);
    uint_t width () const { return _width; }
    uint_t depth () const { return _depth; }
    const char * name () const { return "Xing"; }

private:
    Xing();
    Xing(Xing const &);
    Xing const & operator= (Xing const &);

    uint_t _width;
    uint_t _depth;
    uint_t _cellHeight, _cellDepth, _cellWidth;
    float _color[3];
    Alignment _corner[4];
    SoTransform * ctrans[4];
    SoCylinder * cyls[4];
};
#endif
