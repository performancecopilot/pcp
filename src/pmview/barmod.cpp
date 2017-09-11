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
#include <Inventor/SoPath.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSelection.h>
#include "barmod.h"
#include "modlist.h"
#include "launch.h"

#include <iostream>
using namespace std;

//
// Use debug flag LIBPMDA to trace Bar refreshes
//

const char BarMod::theBarId = 'b';

BarMod::~BarMod()
{
}

BarMod::BarMod(MetricList *metrics, 
	       SoNode *obj, 
	       BarMod::Direction dir,
	       BarMod::Grouping group,
	       float xScale, float yScale, float zScale,
	       float xSpace, float zSpace)
: Modulate(metrics),
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

BarMod::BarMod(MetricList *metrics, 
	       const ColorScale &colScale,
	       SoNode *obj, 
	       BarMod::Direction dir,
	       BarMod::Modulation mod,
	       BarMod::Grouping group,
	       float xScale, float yScale, float zScale,
	       float xSpace, float zSpace)
: Modulate(metrics),
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
    int	numMetrics = _metrics->numMetrics();
    int	numValues = _metrics->numValues();
    int	maxInst = 0;
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
	    const QmcMetric &metric = _metrics->metric(m);
	    for (i = 0; i < metric.numValues(); i++, v++) {
		BarBlock &block = _blocks[v];
		pmsprintf(buf, sizeof(buf), "%c%d", theBarId, v);
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
BarMod::refresh(bool fetchFlag)
{
    int m, i, v;

    if (status() < 0)
	return;

    for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	QmcMetric &metric = _metrics->metric(m);

	if (fetchFlag)
	    metric.update();

	for (i = 0; i < metric.numValues(); i++, v++) {

	    BarBlock &block = _blocks[v];

	    if (metric.error(i) <= 0) {

		if (block._state != Modulate::error) {
		    block._color->rgb.setValue(_errorColor.getValue());
		    if (_mod != color)
			block._scale->scaleFactor.setValue(_xScale, 
							   theMinScale,
							   _zScale);
		    block._state = Modulate::error;
		}
	    }
	    else {
		double  unscaled    = metric.value(i);
		double  value       = unscaled * theScale;
                
		if (value > theNormError) {
		    if (block._state != Modulate::saturated) {
			block._color->rgb.setValue(Modulate::_saturatedColor);
			if (_mod != color)
			    block._scale->scaleFactor.setValue(_xScale, 
							       _yScale, 
							       _zScale);
			block._state = Modulate::saturated;
		    }
		}
		else {
		    if (block._state != Modulate::normal) {
			block._state = Modulate::normal;
			if (_mod == yScale)
			    block._color->rgb.setValue(_metrics->color(m).getValue());
		    }
		    else if (_mod != yScale)
			block._color->rgb.setValue(_colScale.step(unscaled).color().getValue());
		    if (_mod != color) {
			if (value < Modulate::theMinScale)
			    value = Modulate::theMinScale;
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
    int	i;

    if (_selectCount == _blocks.size())
	return;

    theModList->selectAllId(_root, _blocks.size());

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "BarMod::selectAll" << endl;
#endif

    for (i = 0; i < _blocks.size(); i++) {
	if (_blocks[i]._selected == false) {
	    _selectCount++;
	    theModList->selectSingle(_blocks[i]._sep);
	    _blocks[i]._selected = true;
	}
    }
}

int
BarMod::select(SoPath *path)
{
    int	metric, inst, value;

    findBlock(path, metric, inst, value, false);
    if (value < _blocks.size() && _blocks[value]._selected == false) {
	_blocks[value]._selected = true;
	_selectCount++;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "BarMod::select: value = " << value
		 << ", count = " << _selectCount << endl;
#endif
    }
    return _selectCount;
}

int
BarMod::remove(SoPath *path)
{
    int	metric, inst, value;

    findBlock(path, metric, inst, value, false);
    if (value < _blocks.size() && _blocks[value]._selected == true) {
	_blocks[value]._selected = false;
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

void BarMod::infoText(QString &str, bool selected) const
{
    int		m = _infoMetric;
    int		i = _infoInst;
    int		v = _infoValue;
    bool	found = false;

    if (selected && _selectCount == 1) {
	for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	    const QmcMetric &metric = _metrics->metric(m);
	    for (i = 0; i < metric.numValues(); i++, v++)
		if (_blocks[v]._selected) {
		    found = true;
		    break;
		}
	    if (found)
		break;
	}
    }

    if (v >= _blocks.size()) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "BarMod::infoText: infoText requested but nothing selected"
		 << endl;
#endif
	str = "";
    }
    else {
	const QmcMetric &metric = _metrics->metric(m);
	str = metric.spec(true, true, i);
	str.append(QChar('\n'));

	if (_blocks[v]._state == Modulate::error)
	    str.append(theErrorText);
	else if (_blocks[v]._state == Modulate::start)
	    str.append(theStartText);
	else {
	    QString value;
	    str.append(value.setNum(metric.realValue(i), 'g', 4));
	    str.append(QChar(' '));
	    if (metric.desc().units().size() > 0)
		str.append(metric.desc().units());
	    str.append(" [");
	    str.append(value.setNum(metric.value(i) * 100.0, 'g', 4));
	    str.append("% of expected max]");
	}
    }
}

void BarMod::launch(Launch &launch, bool all) const
{
    int		m, i, v;
    bool	needClose;
    bool	always = all;
    bool	keepGoing = true;

    if (status() < 0)
	return;

    if (_selectCount == _blocks.size())
	always = true;

    // Group by metric
    if (_group == groupByMetric || 
	(_group == groupByRow && _dir == instPerCol) ||
	(_group == groupByCol && _dir == instPerRow)) {

	for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	    QmcMetric &metric = _metrics->metric(m);

	    // Do we have to check that an instance of this metric has
	    // been selected?
	    if (!always) {
		needClose = false;
		for (i = 0; i < metric.numValues(); i++, v++) {
		    if (_blocks[v]._selected) {
			if (needClose == false) {
			    launch.startGroup("point");
			    needClose = true;
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
	    needClose = false;
	    keepGoing = false;
	    for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
		QmcMetric &metric = _metrics->metric(m);
		if (metric.numValues() > i) {
		    if (always || _blocks[v+i]._selected) {
			if (needClose == false) {
			    launch.startGroup("point");
			    needClose = true;
			}
			if (_mod == yScale)
			    launch.addMetric(metric, _metrics->color(m), i);
			else
			    launch.addMetric(metric, _colScale, i);
		    }
		    keepGoing = true;
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
    _infoValue = _blocks.size();
    _infoMetric = _infoInst = 0;
}

void
BarMod::dump(QTextStream &os) const
{
    int		m, i, v;

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
	    QmcMetric &metric = _metrics->metric(m);
	    for (i = 0; i < metric.numValues(); i++, v++) {
		os << "    [" << v << "]: ";
		if (_blocks[v]._selected == true)
		    os << '*';
		else
		    os << ' ';
		dumpState(os, _blocks[v]._state);
		os << ": ";
		metric.dump(os, true, i);
	    }
	}
    }
}

void
BarMod::findBlock(SoPath *path, int &metric, int &inst, 
			int &value, bool idMetric)
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
	
	if (value == 0 || idMetric == false) {
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
	value = _blocks.size();
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
    int		m, i, v;
    float	halfX = xScale / 2.0;
    float	halfZ = zScale / 2.0;

    if (status() < 0)
	return;

    _xScale = xScale;
    _zScale = zScale;

    _width = (unsigned int)((_cols * (_xScale + xSpace)) - xSpace);
    _depth = (unsigned int)((_rows * (_zScale + zSpace)) - zSpace);

    for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	const QmcMetric &metric = _metrics->metric(m);
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
	    block._state = Modulate::start;
	    block._selected = false;
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
