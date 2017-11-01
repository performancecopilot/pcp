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
#include <Inventor/nodes/SoSwitch.h>
#include "stackmod.h"
#include "modlist.h"
#include "launch.h"

//
// Use debug flag LIBPMDA to trace stack refreshes
//

const float StackMod::theDefFillColor[] = { 0.35, 0.35, 0.35 };
const char StackMod::theStackId = 's';

StackMod::~StackMod()
{
}

StackMod::StackMod(MetricList *metrics, SoNode *obj, StackMod::Height height)
: Modulate(metrics),
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
	    _text.append(QChar('\n'));
	}

	_blocks.resize(m);
	_infoValue = m+1;

	initScale = 1.0 / (float)numValues;

	if (pmDebugOptions.appl2)
	    cerr << "StackMod::StackMod: numValues = "
		 << numValues << ", num of blocks = " << m << endl 
		 << *_metrics;

	for (m = 0, v = 0; m < numMetrics; m++) {
	    const QmcMetric &metric = _metrics->metric(m);
	    for (i = 0; i < metric.numValues(); i++, v++) {
		StackBlock block;

		block._sep = new SoSeparator;
		pmsprintf(buf, sizeof(buf), "%c%d", theStackId, v);
		block._sep->setName((SbName)buf);
		_root->addChild(block._sep);

		block._color = new SoBaseColor;
		block._color->rgb.setValue(_errorColor.getValue());
		block._sep->addChild(block._color);

		block._scale = new SoScale;
		block._scale->scaleFactor.setValue(1.0, initScale, 1.0);
		block._sep->addChild(block._scale);

		block._sep->addChild(obj);

		block._state = Modulate::start;
		block._selected = false;

		if (_height == fixed || v < numValues - 1) {		    
		    block._tran = new SoTranslation();
		    block._tran->translation.setValue(0.0, initScale, 0.0);
		    _root->addChild(block._tran);
		}
		else {
		    block._tran = NULL;
		}
		_blocks[v] = block;
	    }
	}

	if (_height == fixed) {
	    StackBlock block;
	    block._sep = new SoSeparator;
	    _root->addChild(block._sep);
	    pmsprintf(buf, sizeof(buf), "%c%d", theStackId, v);
	    block._sep->setName((SbName)buf);

	    _switch = new SoSwitch();
	    _switch->whichChild.setValue(SO_SWITCH_ALL);
	    block._sep->addChild(_switch);

	    block._color = new SoBaseColor;
	    block._color->rgb.setValue(theDefFillColor);
	    _switch->addChild(block._color);

	    block._tran = NULL;
	    block._scale = new SoScale;
	    block._scale->scaleFactor.setValue(1.0, 0.0, 1.0);
	    block._state = Modulate::start;
	    block._selected = false;
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
StackMod::refresh(bool fetchFlag)
{
    int		numValues = _metrics->numValues();
    int		numMetrics = _metrics->numMetrics();
    int		m, i, v;
    double	sum = 0.0;

    static QVector<double> values;

    if (pmDebugOptions.libpmda)
	cerr << endl << "StackMod::refresh" << endl;

    if (status() < 0)
	return;

    if (numValues > values.size())
	values.resize(numValues);

    for (m = 0, v = 0; m < numMetrics; m++) {
	QmcMetric &metric = _metrics->metric(m);
	if (fetchFlag)
	    metric.update();
	for (i = 0; i < metric.numValues(); i++, v++) {
	    
	    StackBlock &block = _blocks[v];
	    double &value = values[v];

	    if (pmDebugOptions.libpmda)
		cerr << '[' << v << "] ";

	    if (metric.error(i) <= 0) {
		if (block._state != Modulate::error) {
		    block._color->rgb.setValue(_errorColor.getValue());
		    block._state = Modulate::error;
		}
		value = Modulate::theMinScale;
		sum += value;

		if (pmDebugOptions.libpmda)
		    cerr << "Error, value set to " << value << endl;
	    }
	    else if (block._state == Modulate::error ||
		     block._state == Modulate::start) {
		block._state = Modulate::normal;
		if (numMetrics == 1)
		    block._color->rgb.setValue(_metrics->color(v).getValue());
		else
		    block._color->rgb.setValue(_metrics->color(m).getValue());
		value = metric.value(i) * theScale;
		if (value < theMinScale)
		    value = theMinScale;
		sum += value;
		if (pmDebugOptions.libpmda)
		    cerr << "Error->Normal, value = " << value << endl;
	    }
	    else {
		value = metric.value(i) * theScale;
		if (value < theMinScale)
		    value = theMinScale;
		sum += value;
		if (pmDebugOptions.libpmda)
		    cerr << "Normal, value = " << value << endl;
	    }
	}
    }
    
    if (pmDebugOptions.libpmda)
	cerr << "sum = " << sum << endl;
    
    if (sum > theNormError && _height != util) {
	if (_blocks[0]._state != Modulate::saturated) {
	    for (v = 0; v < numValues; v++) {
		StackBlock &block = _blocks[v];
		if (block._state != Modulate::error) {
		    block._color->rgb.setValue(Modulate::_saturatedColor);
		    block._state = Modulate::saturated;
		}
	    }
	}
    }
    else {
	for (m = 0, v = 0; m < numMetrics; m++) {
	    QmcMetric &metric = _metrics->metric(m);
	    for (i = 0; i < metric.numValues(); i++, v++) {
		StackBlock &block = _blocks[v];
		if (block._state == Modulate::saturated) {
		    block._state = Modulate::normal;
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

	StackBlock &block = _blocks[v];
	double &value = values[v];
 
	if (pmDebugOptions.libpmda)
	    cerr << '[' << v << "] scale = " << value << endl;

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
StackMod::dump(QTextStream &os) const
{
    int		m, i, v;

    os << "StackMod: ";

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
StackMod::infoText(QString &str, bool selected) const
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
	if (pmDebugOptions.appl2)
	    cerr << "StackMod::infoText: infoText requested but nothing selected"
		 << endl;
	str = "";
    }
    else if (_height == fixed && v == _blocks.size() - 1) {
	str = _text;
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
	    if (metric.desc().units().length() > 0)
		str.append(metric.desc().units());
	    str.append(" [");
	    str.append(value.setNum(metric.value(i) * 100.0, 'g', 4));
	    str.append("% of expected max]");
	}
    }
}

void
StackMod::launch(Launch &launch, bool all) const
{
    int		m, i, v;
    bool	launchAll = all;

    if (status() < 0)
	return;

    // If the filler block is selected, launch all metrics
    if (!launchAll && _height == fixed && 
	_blocks.last()._selected == true) {
	launchAll = true;
    }

    if (_height == StackMod::util)
	launch.startGroup("util");
    else
	launch.startGroup("stack");

    for (m = 0, v = 0; m < _metrics->numMetrics(); m++) {
	QmcMetric &metric = _metrics->metric(m);
	for (i = 0; i < metric.numValues(); i++, v++) {
	    if ((_selectCount > 0 && _blocks[v]._selected == true) ||
		_selectCount == 0 || launchAll == true) {

		launch.addMetric(_metrics->metric(m),
				 _metrics->color(m), 
				 i);
	    }
	}
    }

    launch.endGroup();
}

void
StackMod::selectAll()
{
    int		i;

    if (_selectCount == _blocks.size())
	return;

    theModList->selectAllId(_root, _blocks.size());

    if (pmDebugOptions.appl2)
	cerr << "StackMod::selectAll" << endl;

    for (i = 0; i < _blocks.size(); i++) {
	if (_blocks[i]._selected == false) {
	    _selectCount++;
	    theModList->selectSingle(_blocks[i]._sep);
	    _blocks[i]._selected = true;
	}
    }
}

int
StackMod::select(SoPath *path)
{
    int		metric, inst, value;

    findBlock(path, metric, inst, value, false);
    if (value < _blocks.size() && _blocks[value]._selected == false) {
	_blocks[value]._selected = true;
	_selectCount++;

	if (pmDebugOptions.appl2)
	    cerr << "StackMod::select: value = " << value
		 << ", count = " << _selectCount << endl;
    }
    return _selectCount;
}

int
StackMod::remove(SoPath *path)
{
    int		metric, inst, value;

    findBlock(path, metric, inst, value, false);
    if (value < _blocks.size() && _blocks[value]._selected == true) {
	_blocks[value]._selected = false;
	_selectCount--;

	if (pmDebugOptions.appl2)
	    cerr << "StackMod::remove: value = " << value
		 << ", count = " << _selectCount << endl;
    }
    else if (pmDebugOptions.appl2)
	cerr << "StackMod::remove: did not remove " << value 
	     << ", count = " << _selectCount << endl;

    return _selectCount;
}

void 
StackMod::selectInfo(SoPath *path)
{
    findBlock(path, _infoMetric, _infoInst, _infoValue);
}

void
StackMod::removeInfo(SoPath *)
{
    _infoValue = _blocks.size();
    _infoMetric = _infoInst = 0;
}

void
StackMod::findBlock(SoPath *path, int &metric, int &inst, 
			int &value, bool idMetric)
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

	if (pmDebugOptions.appl2)
	    cerr << "StackMod::findBlock: stack id = " << str << endl;

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

    if (pmDebugOptions.appl2)
	cerr << "StackMod::findBlock: metric = " << metric
	     << ", inst = " << inst << ", value = " << value << endl;
}

void
StackMod::setFillColor(const SbColor &col)
{
    if (_sts >= 0 && _height == fixed)
	_blocks.last()._color->rgb.setValue(col.getValue());
}
 
void
StackMod::setFillColor(int packedcol)
{
    SbColor	col;
    float	dummy = 0;

    col.setPackedValue(packedcol, dummy);
    setFillColor(col);
}
