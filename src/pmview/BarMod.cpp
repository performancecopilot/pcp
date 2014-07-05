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


#include <Inventor/SoPath.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSelection.h>
#include "BarMod.h"
#include "ModList.h"
#include "Metric.h"
#include "Launch.h"

//
// Use debug flag LIBPMDA to trace Bar refreshes
//

const char BarMod::theBarId = 'b';

BarMod::~BarMod()
{
}

BarMod::BarMod(INV_MetricList *metrics, 
	       SoNode *obj, 
	       BarMod::Direction dir,
	       BarMod::Grouping group,
	       float xScale, float yScale, float zScale,
	       float xSpace, float zSpace)
: INV_Modulate(metrics),
  _blocks(),
  _dir(dir),
  _mod(BarMod::yScale),
  _group(group),
  _colScale(0.0, 0.0, 0.0),
  _selectCount(0),
  _infoValue(0),
  _infoMetric(0),
  _infoInst(0),
  _xScale(xScale),
  _yScale(yScale),
  _zScale(zScale)
{
    generate(obj, xSpace, zSpace);
}

BarMod::BarMod(INV_MetricList *metrics, 
	       const INV_ColorScale &colScale,
	       SoNode *obj, 
	       BarMod::Direction dir,
	       BarMod::Modulation mod,
	       BarMod::Grouping group,
	       float xScale, float yScale, float zScale,
	       float xSpace, float zSpace)
: INV_Modulate(metrics),
  _blocks(),
  _dir(dir),
  _mod(mod),
  _group(group),
  _colScale(colScale),
  _selectCount(0),
  _infoValue(0),
  _infoMetric(0),
  _infoInst(0),
  _xScale(xScale),
  _yScale(yScale),
  _zScale(zScale)
{
    generate(obj, xSpace, zSpace);
}

void
BarMod::generate(SoNode *obj, float xSpace, float zSpace)
{
    uint_t	numMetrics = _metrics->numMetrics();
    uint_t	numValues = _metrics->numValues();
    uint_t	maxInst = 0;
    char	buf[32];
    int		m, i, v;

    _root = new SoSeparator;

    if (numValues > 0) {

	for (m = 0; m < numMetrics; m++)
	    if (_metrics->metric(m).numValues() > maxInst)
		maxInst = _metrics->metric(m).numValues();

	if (_dir == instPerCol) {
	    _cols = maxInst;
	    _rows = numMetrics;
	}
	else {
	    _cols = numMetrics;
	    _rows = maxInst;
	}

	_blocks.resize(numValues);

	for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	    const OMC_Metric &metric = _metrics->metric(m);
	    for (i = 0; i < metric.numValues(); i++, v++) {
		BarBlock &block = _blocks[v];
		sprintf(buf, "%c%d", theBarId, v);
		block._sep = new SoSeparator;
		block._sep->setName((SbName)buf);
		_root->addChild(block._sep);

		block._tran = new SoTranslation;
		block._sep->addChild(block._tran);

		block._color = new SoBaseColor;
		block._sep->addChild(block._color);

		block._scale = new SoScale;
		block._sep->addChild(block._scale);

		block._sep->addChild(obj);
	    }
	}

	regenerate(_xScale, _zScale, xSpace, zSpace);
	_infoValue = numValues;
	    
	add();
#ifdef PCP_DEBUG
        if (pmDebug & DBG_TRACE_APPL2)
            cerr << "BarMod::generate: Added " << numValues << " in " << _cols
		 << " cols and " << _rows << " rows." << endl;
#endif

    }

    // Invalid object
    else {
	_sts = -1;
    }
}

void
BarMod::refresh(OMC_Bool fetchFlag)
{
    uint_t m, i, v;

    if (status() < 0)
	return;

    for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	OMC_Metric &metric = _metrics->metric(m);

	if (fetchFlag)
	    metric.update();

	for (i = 0; i < metric.numValues(); i++, v++) {

	    BarBlock &block = _blocks[v];

	    if (metric.error(i) <= 0) {

		if (block._state != INV_Modulate::error) {
		    block._color->rgb.setValue(_errorColor.getValue());
		    if (_mod != color)
			block._scale->scaleFactor.setValue(_xScale, 
							   theMinScale,
							   _zScale);
		    block._state = INV_Modulate::error;
		}
	    }
	    else {
		double  unscaled    = metric.value(i);
		double  value       = unscaled * theScale;
                
		if (value > theNormError) {
		    if (block._state != INV_Modulate::saturated) {
			block._color->rgb.setValue(INV_Modulate::_saturatedColor);
			if (_mod != color)
			    block._scale->scaleFactor.setValue(_xScale, 
							       _yScale, 
							       _zScale);
			block._state = INV_Modulate::saturated;
		    }
		}
		else {
		    if (block._state != INV_Modulate::normal) {
			block._state = INV_Modulate::normal;
			if (_mod == yScale)
			    block._color->rgb.setValue(_metrics->color(m).getValue());
		    }
		    else if (_mod != yScale)
			block._color->rgb.setValue(_colScale.step(unscaled).color().getValue());
		    if (_mod != color) {
			if (value < INV_Modulate::theMinScale)
			    value = INV_Modulate::theMinScale;
			else if (value > 1.0)
			    value = 1.0;
			block._scale->scaleFactor.setValue(_xScale,
							   _yScale * value,
							   _zScale);
		    }

		}
	    }
	}
    }
}

void
BarMod::selectAll()
{
    uint_t	i;

    if (_selectCount == _blocks.length())
	return;

    theModList->selectAllId(_root, _blocks.length());

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "BarMod::selectAll" << endl;
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
BarMod::select(SoPath *path)
{
    uint_t	metric, inst, value;

    findBlock(path, metric, inst, value, OMC_false);
    if (value < _blocks.length() && _blocks[value]._selected == OMC_false) {
	_blocks[value]._selected = OMC_true;
	_selectCount++;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "BarMod::select: value = " << value
		 << ", count = " << _selectCount << endl;
#endif
    }
    return _selectCount;
}

uint_t
BarMod::remove(SoPath *path)
{
    uint_t	metric, inst, value;

    findBlock(path, metric, inst, value, OMC_false);
    if (value < _blocks.length() && _blocks[value]._selected == OMC_true) {
	_blocks[value]._selected = OMC_false;
	_selectCount--;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "BarMod::remove: value = " << value
		 << ", count = " << _selectCount << endl;
#endif

    }

#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_APPL2)
	cerr << "BarMod::remove: did not remove " << value 
	     << ", count = " << _selectCount << endl;
#endif

    return _selectCount;
}

void BarMod::infoText(OMC_String &str, OMC_Bool selected) const
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
	    cerr << "BarMod::infoText: infoText requested but nothing selected"
		 << endl;
#endif
	str = "";
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

void BarMod::launch(INV_Launch &launch, OMC_Bool all) const
{
    uint_t	m, i, v;
    OMC_Bool	needClose;
    OMC_Bool	always = all;
    OMC_Bool	keepGoing = OMC_true;

    if (status() < 0)
	return;

    if (_selectCount == _blocks.length())
	always = OMC_true;

    // Group by metric
    if (_group == groupByMetric || 
	(_group == groupByRow && _dir == instPerCol) ||
	(_group == groupByCol && _dir == instPerRow)) {

	for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	    OMC_Metric &metric = _metrics->metric(m);

	    // Do we have to check that an instance of this metric has
	    // been selected?
	    if (!always) {
		needClose = OMC_false;
		for (i = 0; i < metric.numValues(); i++, v++) {
		    if (_blocks[v]._selected) {
			if (needClose == OMC_false) {
			    launch.startGroup("point");
			    needClose = OMC_true;
			}
			if (_mod == yScale)
			    launch.addMetric(metric, _metrics->color(m), i);
			else
			    launch.addMetric(metric, _colScale, i);
		    }
		}
		if (needClose)
		    launch.endGroup();
	    }
	    else {
		launch.startGroup("point");
		for (i = 0; i < metric.numValues(); i++, v++) {
		    if (_mod == yScale)
			launch.addMetric(metric, _metrics->color(m), i);
		    else
			launch.addMetric(metric, _colScale, i);
		}
		launch.endGroup();
	    }
	}
    }

    // Group by instance, this gets a little tricky
    else {
	for (i = 0; keepGoing ; i++) {
	    needClose = OMC_false;
	    keepGoing = OMC_false;
	    for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
		OMC_Metric &metric = _metrics->metric(m);
		if (metric.numValues() > i) {
		    if (always || _blocks[v+i]._selected) {
			if (needClose == OMC_false) {
			    launch.startGroup("point");
			    needClose = OMC_true;
			}
			if (_mod == yScale)
			    launch.addMetric(metric, _metrics->color(m), i);
			else
			    launch.addMetric(metric, _colScale, i);
		    }
		    keepGoing = OMC_true;
		}
		v += metric.numValues();
	    }
	    if (needClose)
		launch.endGroup();
	}
    }
}

void 
BarMod::selectInfo(SoPath *path)
{
    findBlock(path, _infoMetric, _infoInst, _infoValue);
}

void
BarMod::removeInfo(SoPath *)
{
    _infoValue = _blocks.length();
    _infoMetric = _infoInst = 0;
}

void
BarMod::dump(ostream &os) const
{
    uint_t	m, i, v;

    os << "BarMod: ";

    if (_dir == instPerCol)
	os << "inst per col";
    else
	os << "inst per row";

    if (_mod == yScale)
	os << ", Y-Scale: ";
    else if (_mod == color)
	os << ", Color Only: ";
    else
	os << ", Color & Y-Scale: ";

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
BarMod::findBlock(SoPath *path, uint_t &metric, uint_t &inst, 
			uint_t &value, OMC_Bool idMetric)
{
    SoNode	*node;
    char	*str;
    int		m, i, v;
    char	c;

    for (i = path->getLength() - 1; i >= 0; --i) {
	node = path->getNode(i);
	str = (char *)(node->getName().getString());
	if (strlen(str) && str[0] == theBarId)
	    break;
    }

    if (i >= 0) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2) 
	    cerr << "BarMod::findBlock: Bar id = " << str << endl;
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
	cerr << "BarMod::findBlock: metric = " << metric
	     << ", inst = " << inst << ", value = " << value << endl;
    }
#endif

    return;
}

void
BarMod::regenerate(float xScale, float zScale, float xSpace, float zSpace)
{
    uint_t m, i, v;
    float halfX = xScale / 2.0;
    float halfZ = zScale / 2.0;

    if (status() < 0)
	return;

    _xScale = xScale;
    _zScale = zScale;

    _width = (unsigned int)((_cols * (_xScale + xSpace)) - xSpace);
    _depth = (unsigned int)((_rows * (_zScale + zSpace)) - zSpace);

    for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	const OMC_Metric &metric = _metrics->metric(m);
	for (i = 0; i < metric.numValues(); i++, v++) {
	    BarBlock &block = _blocks[v];

	    if (_dir == instPerCol)
		block._tran->translation.setValue(i * (_xScale+xSpace) + halfX,
						  0,
						  m * (_zScale+zSpace) + halfZ);
	    else
		block._tran->translation.setValue(m * (_xScale+xSpace) + halfX,
						  0,
						  i * (_zScale+zSpace) + halfZ);

	    block._color->rgb.setValue(_errorColor.getValue());
	    block._scale->scaleFactor.setValue(_xScale, _yScale, _zScale);
	    block._state = INV_Modulate::start;
	    block._selected = OMC_false;
	}
    }
}

const char *
BarMod::dirStr() const
{
    const char *str = NULL;

    if (_dir == instPerCol)
	str = "instances in columns";
    else
	str = "instances in rows";

    return str;
}

const char *
BarMod::modStr() const
{
    const char *str = NULL;

    switch (_mod) {
    case yScale:
	str = "Y-Scale";
	break;
    case color:
	str = "Colored";
	break;
    case colYScale:
	str = "Colored Y-Scale";
	break;
    }
    return str;
}
