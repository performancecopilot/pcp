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
#ifndef _SCENEFILEOBJ_H_
#define _SCENEFILEOBJ_H_

#include <Inventor/SoLists.h>
#include "modobj.h"
#include "modlist.h"

class SceneFileObj : public ModObj
{
public:
    virtual ~SceneFileObj () {}
    SceneFileObj(const DefaultObj &, int, int, int, int, Alignment);

    int width () const { return _width; }
    int depth () const { return _depth; }

    const char * name() const { return "SceneFile"; }
    SoSeparator *readSceneFile(void);
    void setSceneFileName(char *fname) { strcpy(_sceneFileName, fname); };

    void finishedAdd ();
    void setTag (const char *);

private:
    SceneFileObj ();
    SceneFileObj (SceneFileObj const &);
    SceneFileObj const& operator=(SceneFileObj const &);

    int _width, _depth;
    int cellWidth, cellDepth, cellHeight;
    Alignment _align;
    float _stackHeight;
    QString _tag;

    float _color[3];
    char _sceneFileName[MAXPATHLEN];
};

#endif /* _SCENEFILEOBJ_H_ */
