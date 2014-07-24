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
#ifndef _MODLIST_H_
#define _MODLIST_H_

#include <Inventor/SoLists.h>
#include "modulate.h"

class SoQtViewer;
class SoSelection;
class SoEventCallback;
class SoBoxHighlightRenderAction;
class SoPath;
class SoPickedPoint;
class SoSeparator;
class ModList;
class Launch;
class Record;

typedef void (*SelCallBack)(ModList *, bool);
typedef void (*SelInvCallBack)(ModList *, SoPath *);

typedef QList<Modulate *> ModulateList;
extern SoNodeList elementalNodeList;

class ModList
{
private:

    static const QString	theBogusId;
    static const char		theModListId;

    SoQtViewer			*_viewer;
    SelCallBack			_selCB;
    SelInvCallBack		_selInvCB;
    SelInvCallBack		_deselInvCB;
    SoSeparator                 *_root;
    SoSelection                 *_selection;
    SoEventCallback             *_motion;
    SoBoxHighlightRenderAction  *_rendAct;

    ModulateList		_list;
    QList<int>			_selList;
    int				_current;
    int				_numSel;
    int				_oneSel;

    bool			_allFlag;
    int				_allId;

public:

    ~ModList();

    ModList(SoQtViewer *viewer, SelCallBack selCB, 
	    SelInvCallBack selInvCB = NULL, SelInvCallBack deselInvCB = NULL);

    int size() const
	{ return _list.size(); }
    int numSelected() const
	{ return _numSel; }

    SoSeparator *root()
	{ return _root; }
    void setRoot(SoSeparator *root);

    SoSelection	*selector() const
	{ return _selection; }

    const char *add(Modulate *obj);

    const Modulate &operator[](int i) const
	{ return *(_list[i]); }
    Modulate &operator[](int i)
	{ return *(_list[i]); }
    
    const Modulate &current() const
	{ return *(_list[_current]); }
    Modulate &current()
	{ return *(_list[_current]); }

    void refresh(bool fetchFlag);

    void infoText(QString &str) const;

    void launch(Launch &launch, bool all = false) const;
    void record(Record &rec) const;

    void dumpSelections(QTextStream &os) const;
    bool selections() const;

    void selectAllOn()
	{ _allFlag = true; _allId = _list.size(); }
    void selectAllId(SoNode *node, int count);
    void selectSingle(SoNode *node);
    void selectAllOff();

    friend QTextStream &operator<<(QTextStream &os, const ModList &rhs);

    static void deselectCB(void *me, SoPath *path);

    // Sprouting support

    // Get path to single selection
    const SoPath *oneSelPath() const;

    void deselectPath(SoPath *path);

private:

    static void selCB(void *me, SoPath *path);
    static void motionCB(void *me, SoEventCallback *event);
    static int findToken(const SoPath *path);

    ModList(const ModList &);
    const ModList &operator=(const ModList &);
    // Never defined
};

extern ModList *theModList;

#endif /* _MODLIST_H_ */
