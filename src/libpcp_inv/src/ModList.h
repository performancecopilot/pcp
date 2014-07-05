/* -*- C++ -*- */

#ifndef _INV_MODLIST_H_
#define _INV_MODLIST_H_

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


#include <Inventor/SoLists.h>
#include "List.h"
#include "Modulate.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class SoXtViewer;
class SoSelection;
class SoEventCallback;
class SoBoxHighlightRenderAction;
class SoPath;
class SoPickedPoint;
class SoSeparator;
class INV_ModList;
class INV_Launch;
class INV_Record;

typedef void (*SelCallBack)(INV_ModList *, OMC_Bool);
typedef void (*SelInvCallBack)(INV_ModList *, SoPath *);

typedef OMC_List<INV_Modulate *> INV_ModulateList;
extern SoNodeList elementalNodeList;

class INV_ModList
{
private:

    static const OMC_String	theBogusId;
    static const char		theModListId;

    SoXtViewer			*_viewer;
    SelCallBack			_selCB;
    SelInvCallBack		_selInvCB;
    SelInvCallBack		_deselInvCB;
    SoSeparator                 *_root;
    SoSelection                 *_selection;
    SoEventCallback             *_motion;
    SoBoxHighlightRenderAction  *_rendAct;

    INV_ModulateList		_list;
    OMC_IntList			_selList;
    uint_t			_current;
    uint_t			_numSel;
    uint_t			_oneSel;

    OMC_Bool			_allFlag;
    int				_allId;

public:

    ~INV_ModList();

    INV_ModList(SoXtViewer *viewer, 
		SelCallBack selCB, 
		SelInvCallBack selInvCB = NULL,
		SelInvCallBack deselInvCB = NULL);

    uint_t length() const
	{ return _list.length(); }
    uint_t numSelected() const
	{ return _numSel; }

    SoSeparator *root()
	{ return _root; }
    void setRoot(SoSeparator *root);

    SoSelection	*selector() const
	{ return _selection; }

    const char *add(INV_Modulate *obj);

    const INV_Modulate &operator[](uint_t i) const
	{ return *(_list[i]); }
    INV_Modulate &operator[](uint_t i)
	{ return *(_list[i]); }
    
    const INV_Modulate &current() const
	{ return *(_list[_current]); }
    INV_Modulate &current()
	{ return *(_list[_current]); }

    void refresh(OMC_Bool fetchFlag);

    void infoText(OMC_String &str) const;

    void launch(INV_Launch &launch, OMC_Bool all = OMC_false) const;
    void record(INV_Record &rec) const;

    void dumpSelections(ostream &os) const;
    OMC_Bool selections() const;

    void selectAllOn()
	{ _allFlag = OMC_true; _allId = _list.length(); }
    void selectAllId(SoNode *node, uint_t count);
    void selectSingle(SoNode *node);
    void selectAllOff();

    friend ostream &operator<<(ostream &os, const INV_ModList &rhs);

    static void deselectCB(void *me, SoPath *path);

    // Sprouting support

    // Get path to single selection
    const SoPath *oneSelPath() const;

    void deselectPath(SoPath *path);

private:

    static void selCB(void *me, SoPath *path);
    static void motionCB(void *me, SoEventCallback *event);
    static int findToken(const SoPath *path);

    INV_ModList(const INV_ModList &);
    const INV_ModList &operator=(const INV_ModList &);
    // Never defined
};

extern INV_ModList *theModList;

#endif /* _INV_MODLIST_H_ */
