/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 */


#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/SoPath.h>
#include "Inv.h"
#include "ToggleMod.h"

INV_ToggleMod::~INV_ToggleMod()
{
}

INV_ToggleMod::INV_ToggleMod(SoNode *obj, const char *label)
: INV_Modulate(NULL),
  _label(label)
{
    _root = new SoSeparator;
    _root->addChild(obj);

    if (_label.length() == 0)
	_label = "\n";

    add();
}

void
INV_ToggleMod::dump(ostream &os) const
{
    os << "INV_ToggleMod: \"" << _label << "\" has " << _list.length() 
       << " objects" << endl;
}

void
INV_ToggleMod::selectAll()
{
    uint_t	i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ToggleMod::selectAll: \"" << _label << '"' << endl;
#endif

    for (i = 0; i < _list.length(); i++) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_ToggleMod::selectAll: Selecting [" << i << ']' 
		 << endl;
#endif

    	_list[i]->selectAll();
    }
}

uint_t
INV_ToggleMod::select(SoPath *path)
{
    uint_t	i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ToggleMod::select: \"" << _label << '"' << endl;
#endif

    theModList->selectAllOn();

    for (i = 0; i < _list.length(); i++) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_ToggleMod::select: Selecting [" << i << ']' << endl;
#endif

	_list[i]->selectAll();
    }

    theModList->selectAllOff();

    theModList->selector()->deselect(path->getNodeFromTail(0));

    return 0;
}

uint_t
INV_ToggleMod::remove(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ToggleMod::remove: " << _label << endl;
#endif
    return 0;
}

