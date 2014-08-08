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
#ifndef _XING_H_
#define _XING_H_

#include "baseobj.h"

class SoTransform;
class SoCylinder;

class Xing : public BaseObj
{
public:
    Xing (const DefaultObj &, int, int, int, int, Alignment[4]);

    void finishedAdd ();
    void setTran (float, float, int, int);
    int width () const { return _width; }
    int depth () const { return _depth; }
    const char * name () const { return "Xing"; }

private:
    Xing();
    Xing(Xing const &);
    Xing const & operator= (Xing const &);

    int _width;
    int _depth;
    int _cellHeight, _cellDepth, _cellWidth;
    float _color[3];
    Alignment _corner[4];
    SoTransform * ctrans[4];
    SoCylinder * cyls[4];
};

#endif /* _XING_H_ */
