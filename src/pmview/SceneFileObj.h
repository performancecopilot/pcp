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


#ifndef _SCENEFILEOBJ_H_
#define _SCENEFILEOBJ_H_

#include <Inventor/SoLists.h>
#include "ModObj.h"
#include "ModList.h"
#include "../libpcp_omc/src/String.h"

class SceneFileObj : public ModObj {
public:
    virtual ~SceneFileObj () {}
    SceneFileObj (const DefaultObj &, uint_t, uint_t, uint_t, uint_t, Alignment);

    uint_t width () const { return _width; }
    uint_t depth () const { return _depth; }

    const char * name() const { return "SceneFile"; }
    SoSeparator *readSceneFile(void);
    void setSceneFileName(char *fname) { strcpy(_sceneFileName, fname); };

    void finishedAdd ();
    void setTag (const char *);

private:
    SceneFileObj ();
    SceneFileObj (SceneFileObj const &);
    SceneFileObj const& operator=(SceneFileObj const &);

    uint_t _width, _depth;
    uint_t cellWidth, cellDepth, cellHeight;
    Alignment _align;
    float _stackHeight;
    OMC_String _tag;

    float _color[3];
    char _sceneFileName[MAXPATHLEN];
};

#endif
