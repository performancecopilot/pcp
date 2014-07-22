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
#include <QtCore/QSettings>
#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/actions/SoBoxHighlightRenderAction.h>
#include <Inventor/Qt/viewers/SoQtExaminerViewer.h>
#include "modlist.h"

#include <iostream>
using namespace std;

const QString ModList::theBogusId = "TOP";
const char ModList::theModListId = 'i';

ModList	*theModList;
SoNodeList	elementalNodeList;

ModList::~ModList()
{
}

ModList::ModList(SoQtViewer *viewer, 
			 SelCallBack selCB, 
			 SelInvCallBack selInvCB,
			 SelInvCallBack deselInvCB)
: _viewer(viewer), 
  _selCB(selCB), 
  _selInvCB(selInvCB),
  _deselInvCB(deselInvCB),
  _root(0),
  _selection(0),
  _motion(0),
  _rendAct(0),
  _list(),
  _selList(),
  _current(1),
  _numSel(0),
  _oneSel(0),
  _allFlag(false),
  _allId(0)
{
    QSettings modSettings;
    modSettings.beginGroup(pmProgname);
    QString sval = modSettings.value("saturation", QString("")).toString();
    modSettings.endGroup();

    bool ok;
    double err = sval.toFloat(&ok);
    if (!ok || err > 0.0)
	theNormError = err;

    _root = new SoSeparator;
    _root->ref();
}

void
ModList::setRoot(SoSeparator *root)
{
    _selection = new SoSelection;
    _root->addChild(_selection);

    _selection->policy = SoSelection::SHIFT;
    _selection->addSelectionCallback(&ModList::selCB, this);
    _selection->addDeselectionCallback(&ModList::deselectCB, this);

    _motion = new SoEventCallback;
    _motion->addEventCallback(SoLocation2Event::getClassTypeId(),
                              &ModList::motionCB, this);
    _selection->addChild(_motion);

    root->setName((SbName)(const char *)theBogusId.toAscii());
    _selection->addChild(root);

    SoBoxHighlightRenderAction *rendAct = new SoBoxHighlightRenderAction;
    _viewer->setGLRenderAction(rendAct);
    _viewer->redrawOnSelectionChange(_selection);
}

const char *
ModList::add(Modulate *obj)
{
    static char buf[32];
    int		len = _list.size();
    int		zero = 0;

    _list.append(obj);
    _selList.append(zero);

    if (_current >= len)
	_current = _list.size();

    sprintf(buf, "%c%d", theModListId, _list.size() - 1);
    return buf;
}

void 
ModList::refresh(bool fetchFlag)
{
    for (int i = 0; i < _list.size(); i++)
	_list[i]->refresh(fetchFlag);
    for (int n=elementalNodeList.getLength()-1; n >= 0; n--) {
	elementalNodeList[n]->doAction(_viewer->getGLRenderAction());
    }
}

void
ModList::dumpSelections(QTextStream &os) const
{
    int		i;
    int		count = 0;

    os << _numSel << " selections (SoSelections.numSelections = "
       << _selection->getNumSelected() << "), allFlag = " 
       << (_allFlag == true ? "true" : "false") << endl;
    for (i = 0; i < _selList.size(); i++)
	if (_selList[i] > 0) {
	    count += _selList[i];
	    os << '[' << i << "]: ";
	    if (_numSel == 1 && _oneSel == i)
		os << '*';
	    os << *(_list[i]) << endl;
	}

    assert(count == _numSel);
}

QTextStream &
operator<<(QTextStream &os, const ModList &rhs)
{
    int		i;

    for (i = 0; i < rhs._list.size(); i++)
        os << '[' << i << "]: " << rhs[i] << endl;
    return os;
}

int
ModList::findToken(const SoPath *path)
{
    SoNode	*node = NULL;
    char	*str = NULL;
    int		id = -1;
    int		i;
    char	c;

    for (i = path->getLength() - 1; i >= 0; --i) {
        node = path->getNode(i);
        str = (char *)(node->getName().getString());
        if (strlen(str) && str[0] == theModListId) {
	    sscanf(str, "%c%d", &c, &id);
	    break;
	}
    }

    return id;
}

void
ModList::selCB(void *ptrToThis, SoPath *path)
{
    ModList		*me = (ModList *)ptrToThis;
    Modulate		*obj;
    int			oldCount;
    int			id;

    if (!me->_allFlag)
	id = ModList::findToken(path);
    else
	id = me->_allId;
    
    if (id < 0) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "ModList::selCB: Nothing selected" << endl;
#endif

	return;
	/*NOTREACHED*/
    }
    else if (!me->_allFlag) {

	obj = me->_list[id];
	oldCount = me->_selList[id];

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "ModList::selCB: Before Selected [" << id << "] = "
		 << *obj << endl 
		 << "oldCount = " << oldCount << ", _numSel = "
		 << me->_numSel << ", _allFlag = false" << endl; 
#endif

	me->_selList[id] = obj->select(path);

	me->_numSel += me->_selList[id] - oldCount;
	if (me->_numSel == 1)
	    me->_oneSel = id;
    }

    if (!me->_allFlag)
	(*(me->_selCB))(me, true);

    if (me->_selInvCB != NULL)
	(*(me->_selInvCB))(me, path);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	cerr << "ModList::selCB: After Selected [" << id << "] " << endl
	     << "oldCount = " << oldCount << ", _numSel = "
	     << me->_numSel << ", _allFlag = " 
	     << (me->_allFlag == true ? "true" : "false") << ", _allId = "
	     << me->_allId << endl;
	cerr << "ModList::selCB: selection state:" << endl;
	me->dumpSelections(cerr);
    }
#endif
}

void
ModList::deselectCB(void *ptrToThis, SoPath *path)
{
    ModList		*me = (ModList *)ptrToThis;
    Modulate		*obj;
    int			oldCount;
    int			i;
    int			id;

    id = findToken(path);

    if (id < 0) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "ModList::deselectCB: Nothing deselected" << endl;
#endif

	return;
	/*NOTREACHED*/
    }

    obj = me->_list[id];

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "ModList::deselectCB: Deselected [" << id << "] = "
	     << *obj << endl;
#endif

    oldCount = me->_selList[id];
    me->_selList[id] = obj->remove(path);
    me->_numSel -= oldCount - me->_selList[id];
    if (me->_numSel == 1) {
	for (i = 0; i < me->_selList.size(); i++)
	    if (me->_selList[i] == 1)
		me->_oneSel = i;
    }

    if (me->_numSel == 0)
	me->_current = me->_list.size();

    if (me->_numSel < 2)
	(*(me->_selCB))(me, true);

    if (me->_deselInvCB != NULL)
	(*(me->_deselInvCB))(me, path);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	cerr << "ModList::deselectCB: selection state:" << endl;
	me->dumpSelections(cerr);
    }
#endif
}

void
ModList::motionCB(void *ptrToThis, SoEventCallback *theEvent)
{
    ModList		*me = (ModList *)ptrToThis;
    const SoPickedPoint	*pick = NULL;
    SoPath		*path = NULL;
    int			id = -1;

    // If one item is selected, return as we aren't interested
    if (me->_numSel == 1)
	return;

    pick = theEvent->getPickedPoint();
    if (pick != NULL) {
	path = pick->getPath();    
	if (path != NULL)
	    id = ModList::findToken(path);
    }

    // Nothing selected that we are interested in
    if (id < 0) {
	// Deselect anything selected
	if (me->_current < me->size()) {
	    (*me)[me->_current].removeInfo(path);
	    me->_current = me->size();

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		cerr << "ModList::motionCB: remove object " << id << endl;
#endif
	}
    }
    else if (me->_current != id) {
	if (me->_current < me->size())
	    (*me)[me->_current].removeInfo(path);
	me->_current = id;
	(*me)[me->_current].selectInfo(path);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "ModList::motionCB: new object " << id << endl;
#endif
    }
    else {
	(*me)[me->_current].selectInfo(path);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "ModList::motionCB: same object " << id << endl;
#endif
    }

    // Note: the call to _selCB below used to only be done if the guard
    //    if (old != me->_current)
    // is true. But this does not work for stacked bars because
    // the object is the same even though the mouse has moved over 
    // a different block in the same stack. Hence the metric info
    // text window was not being updated for the mouse motion CB.
    // Since the render method for the metricLabel only updates the
    // text widget if the text has actually changed, it seems to me
    // that it is safe to call the selCB unconditionally and there
    // wont be any "flicker" problems.
    //    -- markgw 15 oct 1997
    //
    (*(me->_selCB))(me, false);
}
            
void
ModList::infoText(QString &str) const
{
    if (_current >= _list.size() && _numSel != 1)
	str = "";
    else if (_numSel == 1)
	_list[_oneSel]->infoText(str, true);
    else
	_list[_current]->infoText(str, false);
}

void
ModList::launch(Launch &launch, bool all) const
{
    int		i;

    if (all == false && _numSel > 0) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "ModList::launch: launching for " << _numSel
		 << " objects" << endl;
#endif

	for (i = 0; i < _selList.size(); i++) {
	    if (_selList[i] > 0)
		_list[i]->launch(launch, false);
	}
    }
    else {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "ModList::launch: launching for all objects" << endl;
#endif

	for (i = 0; i < _list.size(); i++)
	    _list[i]->launch(launch, true);
    }
}

void
ModList::record(Record &rec) const
{
    int		i;

    for (i = 0; i < _list.size(); i++)
	_list[i]->record(rec);
}

bool
ModList::selections() const
{
    if (_numSel)
	return true;

    return false;
}

const SoPath *
ModList::oneSelPath() const
{
    return (_numSel == 1 ? _selection->getPath(0) : NULL);
}

void
ModList::deselectPath(SoPath *path)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "ModList::deselectPath:" << endl;
#endif

    _selection->deselect(path);
    deselectCB(this, path);
}

void
ModList::selectAllId(SoNode *node, int count)
{
    SoPath *path = new SoPath(node);

    _allId = findToken(path);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "ModList::selectAllId: Select All on " << _allId << endl;
#endif

    if (_allId > 0) {
	_numSel += count - _selList[_allId];
	_selList[_allId] = count;
	if (_numSel == 1)
	    _oneSel = _allId;
    }

    path->unref();
}

void
ModList::selectSingle(SoNode *node)
{
    _selection->select(node);
}

void 
ModList::selectAllOff()
{ 
    _allFlag = false; 
    _allId = _list.size();
    (*_selCB)(this, true);
}
