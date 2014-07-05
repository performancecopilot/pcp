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
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSwitch.h>
#include "StackMod.h"
#include "ModList.h"
#include "Metric.h"
#include "Launch.h"

//
// Use debug flag LIBPMDA to trace stack refreshes
//

const float INV_StackMod::theDefFillColor[] = { 0.35, 0.35, 0.35 };
const char INV_StackMod::theStackId = 's';

INV_StackMod::~INV_StackMod()
{
}

INV_StackMod::INV_StackMod(INV_MetricList *metrics, SoNode *obj, 
			   INV_StackMod::Height height)
: INV_Modulate(metrics),
  _blocks(),
  _switch(0),
  _height(height),
  _text(),
  _selectCount(0),
  _infoValue(0),
  _infoMetric(0),
  _infoInst(0)
{
    int		numValues = _metrics->numValues();
    int		numMetrics = _metrics->numMetrics();
    char	buf[32];
    float	initScale = 0.0;
    int		m, i, v;

    _root = new SoSeparator;

    if (numValues > 0) {
	m = numValues;
	if (_height == fixed) {
	    m++;
	    _text.appendChar('\n');
	}

	_blocks.resize(m);
	_infoValue = m+1;

	initScale = 1.0 / (float)numValues;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_StackMod::INV_StackMod: numValues = "
		 << numValues << ", num of blocks = " << m << endl 
		 << *_metrics;
#endif

	for (m = 0, v = 0; m < numMetrics; m++) {
	    const OMC_Metric &metric = _metrics->metric(m);
	    for (i = 0; i < metric.numValues(); i++, v++) {
		INV_StackBlock block;

		block._sep = new SoSeparator;
		sprintf(buf, "%c%d", theStackId, v);
		block._sep->setName((SbName)buf);
		_root->addChild(block._sep);

		block._color = new SoBaseColor;
		block._color->rgb.setValue(_errorColor.getValue());
		block._sep->addChild(block._color);

		block._scale = new SoScale;
		block._scale->scaleFactor.setValue(1.0, initScale, 1.0);
		block._sep->addChild(block._scale);

		block._sep->addChild(obj);

		block._state = INV_Modulate::start;
		block._selected = OMC_false;

		if (_height == fixed || v < numValues - 1) {		    
		    block._tran = new SoTranslation();
		    block._tran->translation.setValue(0.0, initScale, 0.0);
		    _root->addChild(block._tran);
		}
		_blocks[v] = block;
	    }
	}

	if (_height == fixed) {
	    INV_StackBlock block;
	    block._sep = new SoSeparator;
	    _root->addChild(block._sep);
	    sprintf(buf, "%c%d", theStackId, v);
	    block._sep->setName((SbName)buf);

	    _switch = new SoSwitch();
	    _switch->whichChild.setValue(SO_SWITCH_ALL);
	    block._sep->addChild(_switch);

	    block._color = new SoBaseColor;
	    block._color->rgb.setValue(theDefFillColor);
	    _switch->addChild(block._color);
	    
	    block._scale = new SoScale;
	    block._scale->scaleFactor.setValue(1.0, 0.0, 1.0);
	    block._selected = OMC_false;
	    _switch->addChild(block._scale);

	    _switch->addChild(obj);
	    _blocks[v] = block;
	}

	add();
    }

    // Invalid object
    else {

	_sts = -1;

	SoBaseColor *tmpColor = new SoBaseColor();
	tmpColor->rgb.setValue(_errorColor.getValue());
	_root->addChild(tmpColor);

	_root->addChild(obj);
    }
}

void
INV_StackMod::refresh(OMC_Bool fetchFlag)
{
    int		numValues = _metrics->numValues();
    int		numMetrics = _metrics->numMetrics();
    int		m, i, v;
    double	sum = 0.0;

    static OMC_RealVector	values;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LIBPMDA)
	cerr << endl << "StackMod::refresh" << endl;
#endif

    if (status() < 0)
	return;

    if (numValues > values.length())
	values.resizeCopy(numValues, 0.0);

    for (m = 0, v = 0; m < numMetrics; m++) {
	OMC_Metric &metric = _metrics->metric(m);
	if (fetchFlag)
	    metric.update();
	for (i = 0; i < metric.numValues(); i++, v++) {
	    
	    INV_StackBlock &block = _blocks[v];
	    double &value = values[v];

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA)
		cerr << '[' << v << "] ";
#endif

	    if (metric.error(i) <= 0) {
		if (block._state != INV_Modulate::error) {
		    block._color->rgb.setValue(_errorColor.getValue());
		    block._state = INV_Modulate::error;
		}
		value = INV_Modulate::theMinScale;
		sum += value;

#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA)
		    cerr << "Error, value set to " << value << endl;
#endif

	    }
	    else if (block._state == INV_Modulate::error ||
		     block._state == INV_Modulate::start) {
		block._state = INV_Modulate::normal;
		if (numMetrics == 1)
		    block._color->rgb.setValue(_metrics->color(v).getValue());
		else
		    block._color->rgb.setValue(_metrics->color(m).getValue());
		value = metric.value(i) * theScale;
		if (value < theMinScale)
		    value = theMinScale;
		sum += value;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA)
		    cerr << "Error->Normal, value = " << value << endl;
#endif

	    }
	    else {
		value = metric.value(i) * theScale;
		if (value < theMinScale)
		    value = theMinScale;
		sum += value;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA)
		    cerr << "Normal, value = " << value << endl;
#endif

	    }
	}
    }
    
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LIBPMDA)
	cerr << "sum = " << sum << endl;
#endif
    
    if (sum > theNormError && _height != util) {
	if (_blocks[0]._state != INV_Modulate::saturated) {
	    for (v = 0; v < numValues; v++) {
		INV_StackBlock &block = _blocks[v];
		if (block._state != INV_Modulate::error) {
		    block._color->rgb.setValue(INV_Modulate::_saturatedColor);
		    block._state = INV_Modulate::saturated;
		}
	    }
	}
    }
    else {
	for (m = 0, v = 0; m < numMetrics; m++) {
	    OMC_Metric &metric = _metrics->metric(m);
	    for (i = 0; i < metric.numValues(); i++, v++) {
		INV_StackBlock &block = _blocks[v];
		if (block._state == INV_Modulate::saturated) {
		    block._state = INV_Modulate::normal;
		    if (numMetrics == 1)
			block._color->rgb.setValue(_metrics->color(v).getValue());
		    else
			block._color->rgb.setValue(_metrics->color(m).getValue());
		}
	    }
	}
    }

    // Scale values to the range [0,1].
    // Ensure that each block always has the minimum height to
    // avoid planes clashing

    if (sum > 1.0 || _height == util) {
	double oldSum = sum;
	double max = 1.0 - (theMinScale * (numValues - 1));
	sum = 0.0;
	for (v = 0; v < numValues; v++) {
	    double &value = values[v];
	    value /= oldSum;
	    sum += value;
	    if (sum > max) {
		value -= sum - max;
		sum -= sum - max;
	    }
	    if (value < theMinScale)
		value = theMinScale;
	    max += theMinScale;
	}
    }

    for (v = 0; v < numValues; v++) {

	INV_StackBlock &block = _blocks[v];
	double &value = values[v];
 
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA)
	    cerr << '[' << v << "] scale = " << value << endl;
#endif

	block._scale->scaleFactor.setValue(1.0, value, 1.0);
	
	if (v < numValues-1 || _height == fixed)
	    block._tran->translation.setValue(0.0, value, 0.0);
    }

    if (_height == fixed) {
	sum = 1.0 - sum;
	if (sum >= theMinScale) {
	    _switch->whichChild.setValue(SO_SWITCH_ALL);
	    _blocks[v]._scale->scaleFactor.setValue(1.0, sum, 1.0);
	}
	else {
	    _switch->whichChild.setValue(SO_SWITCH_NONE);
	    _blocks[v]._scale->scaleFactor.setValue(1.0, theMinScale, 1.0);
	}
    }
}

void
INV_StackMod::dump(ostream &os) const
{
    uint_t	m, i, v;

    os << "INV_StackMod: ";

    if (status() < 0)
        os << "Invalid metrics: " << pmErrStr(status()) << endl;
    else {
	os << endl;
	for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	    OMC_Metric &metric = _metrics->metric(m);
	    for (i = 0; i < metric.numValues(); i++, v++) {
		os << "    [" << v << "]: ";
		if (_blocks[v]._selected == OMC_true)
		    os << '*';
		else
		    os << ' ';
		dumpState(os, _blocks[v]._state);
		os << ": ";
		metric.dump(os, OMC_true, i);
	    }
	}
    }
}

void
INV_StackMod::infoText(OMC_String &str, OMC_Bool selected) const
{
    uint_t	m = _infoMetric;
    uint_t	i = _infoInst;
    uint_t	v = _infoValue;
    OMC_Bool	found = OMC_false;

    if (selected && _selectCount == 1) {
	for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	    const OMC_Metric &metric = _metrics->metric(m);
	    for (i = 0; i < metric.numValues(); i++, v++)
		if (_blocks[v]._selected) {
		    found = OMC_true;
		    break;
		}
	    if (found)
		break;
	}
    }

    if (v >= _blocks.length()) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_StackMod::infoText: infoText requested but nothing selected"
		 << endl;
#endif
	str = "";
    }
    else if (_height == fixed && v == _blocks.length() - 1) {
	str = _text;
    }
    else {
	const OMC_Metric &metric = _metrics->metric(m);
	str = metric.spec(OMC_true, OMC_true, i);
	str.appendChar('\n');

	if (_blocks[v]._state == INV_Modulate::error)
	    str.append(theErrorText);
	else if (_blocks[v]._state == INV_Modulate::start)
	    str.append(theStartText);
	else {
	    str.appendReal(metric.realValue(i), 4);
	    str.appendChar(' ');
	    if (metric.units().length() > 0)
		str.append(metric.units());
	    str.append(" [");
	    str.appendReal(metric.value(i) * 100.0, 4);
	    str.append("% of expected max]");
	}
    }
    }

void
INV_StackMod::launch(INV_Launch &launch, OMC_Bool all) const
{
    uint_t	m, i, v;
    OMC_Bool	launchAll = all;

    if (status() < 0)
	return;

    // If the filler block is selected, launch all metrics
    if (!launchAll && _height == fixed && 
	_blocks.last()._selected == OMC_true) {
	launchAll = OMC_true;
    }

    if (_height == INV_StackMod::util)
	launch.startGroup("util");
    else
	launch.startGroup("stack");

    for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	OMC_Metric &metric = _metrics->metric(m);
	for (i = 0; i < metric.numValues(); i++, v++) {
	    if ((_selectCount > 0 && _blocks[v]._selected == OMC_true) ||
		_selectCount == 0 || launchAll == OMC_true) {

		launch.addMetric(_metrics->metric(m),
				 _metrics->color(m), 
				 i);
	    }
	}
    }

    launch.endGroup();
}

void
INV_StackMod::selectAll()
{
    uint_t	i;

    if (_selectCount == _blocks.length())
	return;

    theModList->selectAllId(_root, _blocks.length());

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_StackMod::selectAll" << endl;
#endif

    for (i = 0; i < _blocks.length(); i++) {
	if (_blocks[i]._selected == OMC_false) {
	    _selectCount++;
	    theModList->selectSingle(_blocks[i]._sep);
	    _blocks[i]._selected = OMC_true;
	}
    }
}

uint_t
INV_StackMod::select(SoPath *path)
{
    uint_t	metric, inst, value;

    findBlock(path, metric, inst, value, OMC_false);
    if (value < _blocks.length() && _blocks[value]._selected == OMC_false) {
	_blocks[value]._selected = OMC_true;
	_selectCount++;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_StackMod::select: value = " << value
		 << ", count = " << _selectCount << endl;
#endif
    }
    return _selectCount;
}

uint_t
INV_StackMod::remove(SoPath *path)
{
    uint_t	metric, inst, value;

    findBlock(path, metric, inst, value, OMC_false);
    if (value < _blocks.length() && _blocks[value]._selected == OMC_true) {
	_blocks[value]._selected = OMC_false;
	_selectCount--;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_StackMod::remove: value = " << value
		 << ", count = " << _selectCount << endl;
#endif

    }

#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_StackMod::remove: did not remove " << value 
	     << ", count = " << _selectCount << endl;
#endif

    return _selectCount;
}

void 
INV_StackMod::selectInfo(SoPath *path)
{
    findBlock(path, _infoMetric, _infoInst, _infoValue);
}

void
INV_StackMod::removeInfo(SoPath *)
{
    _infoValue = _blocks.length();
    _infoMetric = _infoInst = 0;
}

void
INV_StackMod::findBlock(SoPath *path, uint_t &metric, uint_t &inst, 
			uint_t &value, OMC_Bool idMetric)
{
    SoNode	*node;
    char	*str;
    int		m, i, v;
    char	c;

    for (i = path->getLength() - 1; i >= 0; --i) {
	node = path->getNode(i);
	str = (char *)(node->getName().getString());
	if (strlen(str) && str[0] == theStackId)
	    break;
    }

    if (i >= 0) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2) 
	    cerr << "INV_StackMod::findBlock: stack id = " << str << endl;
#endif

	sscanf(str, "%c%d", &c, &value);
	
	if (value == 0 || idMetric == OMC_false) {
	    metric = 0;
	    inst = 0;
	}
	else {
	    m = 0;
	    v = value;
	    while (m < _metrics->numMetrics()) {
		i = _metrics->metric(m).numValues();
		if (v < i) {
		    metric = m;
		    inst = v;
		    break;
		}
		else {
		    v -= i;
		    m++;
		}
	    }
	}
    }
    else {
	value = _blocks.length();
	metric = inst = 0;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	cerr << "INV_StackMod::findBlock: metric = " << metric
	     << ", inst = " << inst << ", value = " << value << endl;
    }
#endif

    return;
}

void
INV_StackMod::setFillColor(const SbColor &col)
{
    if (_sts >= 0 && _height == fixed)
	_blocks.last()._color->rgb.setValue(col.getValue());
}
 
void
INV_StackMod::setFillColor(uint32_t packedcol)
{
    SbColor	col;
    float	dummy = 0;

    col.setPackedValue(packedcol, dummy);
    setFillColor(col);
}
