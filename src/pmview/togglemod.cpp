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
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/SoPath.h>
#include "main.h"
#include "togglemod.h"

#include <iostream>
using namespace std;

ToggleMod::~ToggleMod()
{
}

ToggleMod::ToggleMod(SoNode *obj, const char *label)
: Modulate(NULL),
  _label(label)
{
    _root = new SoSeparator;
    _root->addChild(obj);

    if (_label.length() == 0)
	_label = "\n";

    add();
}

void
ToggleMod::dump(QTextStream &os) const
{
    os << "ToggleMod: \"" << _label << "\" has " << _list.size()
       << " objects" << endl;
}

void
ToggleMod::selectAll()
{
    int		i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "ToggleMod::selectAll: \"" << _label << '"' << endl;
#endif

    for (i = 0; i < _list.size(); i++) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "ToggleMod::selectAll: Selecting [" << i << ']' 
		 << endl;
#endif

    	_list[i]->selectAll();
    }
}

int
ToggleMod::select(SoPath *path)
{
    int		i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "ToggleMod::select: \"" << _label << '"' << endl;
#endif

    theModList->selectAllOn();

    for (i = 0; i < _list.size(); i++) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "ToggleMod::select: Selecting [" << i << ']' << endl;
#endif

	_list[i]->selectAll();
    }

    theModList->selectAllOff();

    theModList->selector()->deselect(path->getNodeFromTail(0));

    return 0;
}

int
ToggleMod::remove(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "ToggleMod::remove: " << _label << endl;
#endif
    return 0;
}

