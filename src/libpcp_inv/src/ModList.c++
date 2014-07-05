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


#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/actions/SoBoxHighlightRenderAction.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Vk/VkResource.h>

#include "ModList.h"

const OMC_String INV_ModList::theBogusId = "TOP";
const char INV_ModList::theModListId = 'i';

INV_ModList	*theModList = NULL;
SoNodeList	elementalNodeList;

INV_ModList::~INV_ModList()
{
}

INV_ModList::INV_ModList(SoXtViewer *viewer, 
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
  _allFlag(OMC_false),
  _allId(0)
{
    char *sval = VkGetResource("saturation", XmRString);
    if (sval != NULL) {
        double err = atof(sval);
        if (err > 0.0)
	    theNormError = err;
    }

    _root = new SoSeparator;
    _root->ref();
}

void
INV_ModList::setRoot(SoSeparator *root)
{
    _selection = new SoSelection;
    _root->addChild(_selection);

    _selection->policy = SoSelection::SHIFT;
    _selection->addSelectionCallback(&INV_ModList::selCB, this);
    _selection->addDeselectionCallback(&INV_ModList::deselectCB, this);

    _motion = new SoEventCallback;
    _motion->addEventCallback(SoLocation2Event::getClassTypeId(),
                              &INV_ModList::motionCB, this);
    _selection->addChild(_motion);

    root->setName((SbName)theBogusId.ptr());
    _selection->addChild(root);

    SoBoxHighlightRenderAction *rendAct = new SoBoxHighlightRenderAction;
    _viewer->setGLRenderAction(rendAct);
    _viewer->redrawOnSelectionChange(_selection);
}

const char *
INV_ModList::add(INV_Modulate *obj)
{
    static char buf[32];
    uint_t	len = _list.length();
    int		zero = 0;

    _list.append(obj);
    _selList.append(zero);

    if (_current >= len)
	_current = _list.length();
    
    sprintf(buf, "%c%d", theModListId, _list.length() - 1);
    return buf;
}

void 
INV_ModList::refresh(OMC_Bool fetchFlag)
{
    for (int i = 0; i < _list.length(); i++)
	_list[i]->refresh(fetchFlag);
    for (int n=elementalNodeList.getLength()-1; n >= 0; n--) {
	elementalNodeList[n]->doAction(_viewer->getGLRenderAction());
    }
}

void
INV_ModList::dumpSelections(ostream &os) const
{
    uint_t	i;
    uint_t	count = 0;

    os << _numSel << " selections (SoSelections.numSelections = "
       << _selection->getNumSelected() << "), allFlag = " 
       << (_allFlag == OMC_true ? "true" : "false") << endl;
    for (i = 0; i < _selList.length(); i++)
	if (_selList[i] > 0) {
	    count += _selList[i];
	    os << '[' << i << "]: ";
	    if (_numSel == 1 && _oneSel == i)
		os << '*';
	    os << *(_list[i]) << endl;
	}

    assert(count == _numSel);
}

ostream &
operator<<(ostream &os, const INV_ModList &rhs)
{
    uint_t	i;

    for (i = 0; i < rhs._list.length(); i++)
        os << '[' << i << "]: " << rhs[i] << endl;
    return os;
}

int
INV_ModList::findToken(const SoPath *path)
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
INV_ModList::selCB(void *ptrToThis, SoPath *path)
{
    INV_ModList		*me = (INV_ModList *)ptrToThis;
    INV_Modulate	*obj;
    uint_t		oldCount;
    int			id;

    if (!me->_allFlag)
	id = INV_ModList::findToken(path);
    else
	id = me->_allId;
    
    if (id < 0) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_ModList::selCB: Nothing selected" << endl;
#endif

	return;
	/*NOTREACHED*/
    }
    else if (!me->_allFlag) {

	obj = me->_list[id];
	oldCount = me->_selList[id];

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_ModList::selCB: Before Selected [" << id << "] = "
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
	(*(me->_selCB))(me, OMC_true);

    if (me->_selInvCB != NULL)
	(*(me->_selInvCB))(me, path);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	cerr << "INV_ModList::selCB: After Selected [" << id << "] " << endl
	     << "oldCount = " << oldCount << ", _numSel = "
	     << me->_numSel << ", _allFlag = " 
	     << (me->_allFlag == OMC_true ? "true" : "false") << ", _allId = "
	     << me->_allId << endl;
	cerr << "INV_ModList::selCB: selection state:" << endl;
	me->dumpSelections(cerr);
    }
#endif
}

void
INV_ModList::deselectCB(void *ptrToThis, SoPath *path)
{
    INV_ModList		*me = (INV_ModList *)ptrToThis;
    INV_Modulate	*obj;
    uint_t		oldCount;
    uint_t		i;
    int			id;

    id = findToken(path);

    if (id < 0) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_ModList::deselectCB: Nothing deselected" << endl;
#endif

	return;
	/*NOTREACHED*/
    }

    obj = me->_list[id];

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_ModList::deselectCB: Deselected [" << id << "] = "
	     << *obj << endl;
#endif

    oldCount = me->_selList[id];
    me->_selList[id] = obj->remove(path);
    me->_numSel -= oldCount - me->_selList[id];
    if (me->_numSel == 1) {
	for (i = 0; i < me->_selList.length(); i++)
	    if (me->_selList[i] == 1)
		me->_oneSel = i;
    }

    if (me->_numSel == 0)
	me->_current = me->_list.length();

    if (me->_numSel < 2)
	(*(me->_selCB))(me, OMC_true);

    if (me->_deselInvCB != NULL)
	(*(me->_deselInvCB))(me, path);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	cerr << "INV_ModList::deselectCB: selection state:" << endl;
	me->dumpSelections(cerr);
    }
#endif
}

void
INV_ModList::motionCB(void *ptrToThis, SoEventCallback *theEvent)
{
    INV_ModList		*me = (INV_ModList *)ptrToThis;
    // uint_t		old = me->_current;
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
	    id = INV_ModList::findToken(path);
    }

    // Nothing selected that we are interested in
    if (id < 0) {
	// Deselect anything selected
	if (me->_current < me->length()) {
	    (*me)[me->_current].removeInfo(path);
	    me->_current = me->length();

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		cerr << "INV_ModList::motionCB: remove object " << id << endl;
#endif
	}
    }
    else if (me->_current != id) {
	if (me->_current < me->length())
	    (*me)[me->_current].removeInfo(path);
	me->_current = id;
	(*me)[me->_current].selectInfo(path);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_ModList::motionCB: new object " << id << endl;
#endif
    }
    else {
	(*me)[me->_current].selectInfo(path);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_ModList::motionCB: same object " << id << endl;
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
    (*(me->_selCB))(me, OMC_false);
}
            
void
INV_ModList::infoText(OMC_String &str) const
{
    if (_current >= _list.length() && _numSel != 1)
	str = "";
    else if (_numSel == 1)
	_list[_oneSel]->infoText(str, OMC_true);
    else
	_list[_current]->infoText(str, OMC_false);
}

void
INV_ModList::launch(INV_Launch &launch, OMC_Bool all) const
{
    uint_t	i;

    if (all == OMC_false && _numSel > 0) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_ModList::launch: launching for " << _numSel
		 << " objects" << endl;
#endif

	for (i = 0; i < _selList.length(); i++) {
	    if (_selList[i] > 0)
		_list[i]->launch(launch, OMC_false);
	}
    }
    else {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_ModList::launch: launching for all objects" << endl;
#endif

	for (i = 0; i < _list.length(); i++)
	    _list[i]->launch(launch, OMC_true);
    }
}

void
INV_ModList::record(INV_Record &rec) const
{
    uint_t	i;

    for (i = 0; i < _list.length(); i++)
	_list[i]->record(rec);
}

OMC_Bool
INV_ModList::selections() const
{
    if (_numSel)
	return OMC_true;

    return OMC_false;
}

const SoPath *
INV_ModList::oneSelPath() const
{
    return (_numSel == 1 ? _selection->getPath(0) : NULL);
}

void
INV_ModList::deselectPath(SoPath *path)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_ModList::deselectPath:" << endl;
#endif

    _selection->deselect(path);
    deselectCB(this, path);
}

void
INV_ModList::selectAllId(SoNode *node, uint_t count)
{
    SoPath *path = new SoPath(node);

    _allId = findToken(path);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_ModList::selectAllId: Select All on " << _allId << endl;
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
INV_ModList::selectSingle(SoNode *node)
{
    _selection->select(node);
}

void 
INV_ModList::selectAllOff()
{ 
    _allFlag = OMC_false; 
    _allId = _list.length();
    (*_selCB)(this, OMC_true);
}
