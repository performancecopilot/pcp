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
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoTransform.h>

#include "barobj.h"
#include "colorlist.h"
#include "yscalemod.h"
#include "colormod.h"
#include "colorscalemod.h"
#include "text.h"
#include "defaultobj.h"

#include <iostream>
using namespace std;

BarObj::~BarObj()
{
}

BarObj::BarObj(ViewObj::Shape shape,
	       BarMod::Direction dir,
	       BarMod::Modulation mod,
	       BarMod::Grouping group,
	       bool baseFlag, 
	       const DefaultObj &defaults,
	       int x, int y, 
	       int cols, int rows, 
	       BaseObj::Alignment align)
: ModObj(baseFlag, defaults, x, y, cols, rows, align),
  _shape(shape),
  _dir(dir),
  _mod(mod),
  _group(group),
  _width(0),
  _depth(0),
  _xSpace(defaults.barSpaceX()),
  _zSpace(defaults.barSpaceZ()),
  _labelSpace(defaults.barSpaceLabel()),
  _bars(0),
  _metDir(towards),
  _metLabels(new QStringList),
  _instDir(away),
  _instLabels(new QStringList)
{
    _objtype |= BAROBJ;

    int	i;
    for (i = 0; i < numSides; i++)
	_margins[i] = 0.0;
    _labelColor[0] = defaults.labelColor(0);
    _labelColor[1] = defaults.labelColor(1);
    _labelColor[2] = defaults.labelColor(2);
}

void
BarObj::finishedAdd()
{
    const ColorSpec	*colSpec = NULL;
    SoNode		*object = ViewObj::object(_shape);
    SoSeparator		*labelSep = NULL;
    SoSeparator		*metricSep = NULL;
    SoSeparator		*instSep = NULL;
    SoSeparator		*barSep = new SoSeparator;
    SoSeparator		*baseSep = new SoSeparator;
    SoTranslation	*objTran = new SoTranslation;
    SoTranslation	*barTran = new SoTranslation;
    SoTranslation	*baseTran = new SoTranslation;
    SoTranslation	*modTran = new SoTranslation;
    ColorScale		*colScale = NULL;
    LabelSide		metSide = left;
    LabelSide		instSide = left;
    Text		**metText = NULL;
    Text		**instText = NULL;
    int			i;
    int			max = 0;
    int			numMetLabels = 0;
    int			numInstLabels = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "BarObj::finishedAdd:" << endl;
#endif

    if (_metrics.numMetrics() == 0) {
	BaseObj::addBase(_root);
	pmprintf("%s: Error: Bar object has no metrics\n",
		 pmProgname);
	_length = 0;
	_width = baseWidth();
	_depth = baseDepth();
	return;
    }

    _root->addChild(objTran);

    if (_metLabels->size() || _instLabels->size()) {
	labelSep = new SoSeparator;
	_root->addChild(labelSep);
	SoBaseColor *base = new SoBaseColor;
	base->rgb.setValue(_labelColor[0], _labelColor[1], _labelColor[2]);
	labelSep->addChild(base);
    }
    _root->addChild(barSep);
    barSep->addChild(barTran);
    barSep->addChild(baseSep);
    baseSep->addChild(baseTran);
    barSep->addChild(modTran);
    BaseObj::addBase(baseSep);

    // Determine color mapping

    if (_colors.size())
	colSpec = theColorLists.list((const char *)_colors.toLatin1());

    if (colSpec != NULL) {
        if (colSpec->_scale) {
	    if (_mod == BarMod::yScale) {
		pmprintf("%s: Warning: Color scale ignored for Y-Scale Bar object.\n",
			 pmProgname);
	    }
	    else {
		if (colSpec->_list.size() == 0)
		    colScale = new ColorScale(0.0, 0.0, 1.0);
		else {
		    colScale = new ColorScale(*(colSpec->_list[0]));
		    for (i = 1; i < colSpec->_list.size(); i++)
			colScale->add(new ColorStep(*(colSpec->_list[i]),
							colSpec->_max[i]));
		}
	    }
	}
        else if (_mod == BarMod::color || _mod == BarMod::colYScale) {
	    pmprintf("%s: Warning: Expected color scale for color modulated Bar object.\n",
		     pmProgname);

	    if (colSpec->_list.size() == 0)
		colScale = new ColorScale(0.0, 0.0, 1.0);
	    else
		colScale = new ColorScale(*(colSpec->_list[0]));
	}
    }
    else {
        pmprintf("%s: Warning: No colours specified for Bar objects, defaulting to blue.\n",
                 pmProgname);

	if (_mod == BarMod::color || _mod == BarMod::colYScale)
	    colScale = new ColorScale(0.0, 0.0, 1.0);
    }

    if (_mod == BarMod::yScale) {
	if (colSpec != NULL)
	    for (i = 0; i < colSpec->_list.size(); i++)
		_metrics.add(*(colSpec->_list)[i]);
	_metrics.resolveColors(MetricList::perMetric);
    }

    // Generate Bar Modulate Object
    if (_mod == BarMod::yScale)
	_bars = new BarMod(&_metrics, object, _dir, _group,
			   (float)_length, (float)_maxHeight, (float)_length,
			   (float)_xSpace, (float)_zSpace);
    else {
	_bars = new BarMod(&_metrics, *colScale, object, _dir, _mod, _group,
			   (float)_length, (float)_maxHeight, (float)_length,
			   (float)_xSpace, (float)_zSpace);	
    }

    barSep->addChild(_bars->root());
    BaseObj::add(_bars);

    // Generate Labels

    if (_metLabels->size()) {
	if (_dir == BarMod::instPerRow)
	    if (_metDir == away)
		metSide = below;
	    else
		metSide = above;
	else
	    if (_metDir == away)
		metSide = right;
	    else
		metSide = left;

	metricSep = new SoSeparator;
	labelSep->addChild(metricSep);

	if (_metLabels->size() < _metrics.numMetrics())
	    numMetLabels = _metLabels->size();
	else
	    numMetLabels = _metrics.numMetrics();

	metText = calcLabels(*_metLabels, metSide, numMetLabels);
    }

    if (_instLabels->size()) {
	if (_dir == BarMod::instPerCol) {
	    max = _bars->cols();
	    if (_instDir == away)
		instSide = below;
	    else
		instSide = above;
	}
	else {
	    max = _bars->rows();
	    if (_instDir == away)
		instSide = right;
	    else
		instSide = left;
	}

	instSep = new SoSeparator;
	labelSep->addChild(instSep);

	if (_instLabels->size() < max)
	    numInstLabels = _instLabels->size();
	else
	    numInstLabels = max;

	instText = calcLabels(*_instLabels, instSide, numInstLabels);
    }

    // Width and depth of bars only, effects of labels added later

    _width = _bars->width();
    _depth = _bars->depth();

    // Insert the labels

    if (numMetLabels)
	metricSep->addChild(doLabels(metText, metSide, numMetLabels));

    if (numInstLabels)
	instSep->addChild(doLabels(instText, instSide, numInstLabels));

    // Work out where the bars live

    _bars->regenerate(_length, _length, _xSpace, _zSpace);
    _width = _bars->width();
    _depth = _bars->depth();

    baseTran->translation.setValue(_width / 2.0, 0.0, _depth / 2.0);
    
    _width += (u_int32_t)(baseWidth() + _margins[left] + _margins[right]+0.5);
    _depth += (u_int32_t)(baseDepth() + _margins[above] + _margins[below]+0.5);

    objTran->translation.setValue((_width / -2.0), 0.0, (_depth / -2.0));

    barTran->translation.setValue(_margins[left] + borderX(), 0.0,
				  _margins[above] + borderZ());


    modTran->translation.setValue(0.0, 
				  (BaseObj::state() ? baseHeight() : 0.0),
				  0.0);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "BarObj::finishedAdd: metric list = " << endl
	     << _metrics << endl;
#endif

    if (_metrics.numMetrics())
	ViewObj::theNumModObjects++;

    // Cleanup

    if (colScale)
	delete colScale;
    delete _metLabels;
    delete _instLabels;
}

void
BarObj::setTran(float xTran, float zTran, int setWidth, int setDepth)
{
    BaseObj::setBaseSize(width() - _margins[left] - _margins[right],
			 depth() - _margins[above] - _margins[below]);
    BaseObj::setTran(xTran + (width() / 2.0),
		     zTran + (depth() / 2.0),
		     setWidth, setDepth);
}

QTextStream&
operator<<(QTextStream& os, BarObj const& rhs)
{
    rhs.display(os);
    return os;
}

void
BarObj::display(QTextStream& os) const
{
    BaseObj::display(os);

    if (_bars == NULL) {
	os << "No valid metrics" << endl; 
	return;
    }

    os << ", dir = " 
       << (_dir == BarMod::instPerCol ? "instPerCol" : "instPerRow")
       << ", length = " << _length << ", xSpace = " << _xSpace << ", zSpace = "
       << _zSpace << ", labelSpace = " << _labelSpace << ", rows = " << _rows 
       << ", cols = " << _cols << ", num bars = " << _bars->numBars()
       << ", shape = ";
    ViewObj::dumpShape(os, _shape);
    os << ", margins: left = " << _margins[left] << ", right = "
       << _margins[right] << ", above = " << _margins[above]
       << ", below = " << _margins[below];
}

const char*
BarObj::name() const
{
    static QString myName;

    if (myName.size() == 0) {
	if (_bars == NULL)
	    myName = "Invalid bar object";
	else {
	    myName = _bars->modStr();
	    myName.append(" Bar Object (");
	    myName.append(_bars->dirStr());
	    myName.append(QChar(')'));
	}
    }

    return (const char *)myName.toLatin1();
}

Text **
BarObj::calcLabels(const QStringList &labels, LabelSide side, int numLabels)
{
    Text		**text = NULL;
    int			i;
    int			maxWidth = 0;
    int			maxDepth = 0;

    text= new Text*[numLabels];

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	cerr << "BarObj::calcLabels: " << numLabels << " labels on the ";
	switch(side) {
	case left:
	    cerr << "left";
	    break;
	case right:
	    cerr << "right";
	    break;
	case above:
	    cerr << "above";
	    break;
	case below:
	    cerr << "below";
	    break;
	}
	cerr << " side" << endl;
    }
#endif

    // Create the text objects so that we know how big they are

    for (i = 0; i < numLabels; i++) {
	if (side == above || side == below)
	    text[i] = new Text(labels[i], Text::down, Text::medium);
	else
	    text[i] = new Text(labels[i], Text::right, Text::medium);

	if (text[i]->width() > maxWidth)
	    maxWidth = text[i]->width();
	if (text[i]->depth() > maxDepth)
	    maxDepth = text[i]->depth();
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "BarObj::calcLabels: maxWidth = " << maxWidth
	     << ", maxDepth = " << maxDepth << endl;
#endif

    // Determine if the size of the bars will need to be increased

    if (side == above || side == below) {
	_margins[side] = maxDepth + _labelSpace;
	if (maxWidth > _length) {
	    _length = maxWidth;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		cerr << "BarObj::calcLabels: length (width) increased to "
		     << _length << endl;
#endif

	}
    }
    else {
	_margins[side] = maxWidth + _labelSpace;
	if (maxDepth > _length) {
	    _length = maxDepth;


#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		cerr << "BarObj::calcLabels: length (depth) increased to " 
		     << _length << endl;
#endif
	}
    }
    return text;
}

SoNode *
BarObj::doLabels(Text **text, LabelSide side, int numLabels)
{
    SoSeparator		*sep = new SoSeparator;
    SoTranslation	*tran = new SoTranslation;
    int			i;
    int			maxWidth = 0;
    int			maxDepth = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	cerr << "BarObj::doLabels: " << numLabels << " labels on the ";
	switch(side) {
	case left:
	    cerr << "left";
	    break;
	case right:
	    cerr << "right";
	    break;
	case above:
	    cerr << "above";
	    break;
	case below:
	    cerr << "below";
	    break;
	}
	cerr << " side" << endl;
    }
#endif

    sep->addChild(tran);

    // Determine the translation to the first label, subsequent labels
    // are translated from the first

    maxWidth = _length + _xSpace;
    maxDepth = _length + _zSpace;

    switch (side) {
    case left:
	tran->translation.setValue(_margins[left] - _labelSpace, 0.0, 
				   _margins[above] + borderZ());
	break;
    case right:
	tran->translation.setValue(
			 _margins[left] + _width + baseWidth() + _labelSpace, 
				   0.0, _margins[above] + borderZ());
	break;
    case above:
	tran->translation.setValue(_margins[left] + borderX(), 0.0, 
				   _margins[above] - _labelSpace);
	break;
    case below:
	tran->translation.setValue(_margins[left] + borderX(), 0.0,
			 _margins[above] + _depth + baseDepth() + _labelSpace);
	break;
    default:
	break;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	float x, y, z;
	tran->translation.getValue().getValue(x, y, z);
	cerr << "BarObj::doLabels: translation set to " << x << ',' << y
	     << ',' << z << endl;
    }
#endif

    // Add each label to the scene graph

    for (i = 0; i < numLabels; i++) {
	SoSeparator *labelSep = new SoSeparator;
	sep->addChild(labelSep);

	SoTranslation *labelTran = new SoTranslation;
	labelSep->addChild(labelTran);

	switch (side) {
	case left:
	    labelTran->translation.setValue(0.0,
					    0.0, 
		  (maxDepth * i) + ((_length - (float)text[i]->depth())/ 2.0));
	    break;
	case right:
	    labelTran->translation.setValue(text[i]->width(), 0.0, 
		  (maxDepth * i) + ((_length - (float)text[i]->depth())/ 2.0));
	    break;
	case above:
	    labelTran->translation.setValue(
		  (maxWidth * i) + ((_length - (float)text[i]->width())/ 2.0),
					    0.0, 0.0);
	    break;
	case below:
	    labelTran->translation.setValue(
		  (maxWidth * i) + ((_length - (float)text[i]->width())/ 2.0),
					    0.0, text[i]->depth());
	    break;
	default:
	    break;
	}

	labelSep->addChild(text[i]->root());
    }

    // Do not delete contents, just the array pointer
    delete [] text;

    return sep;
}
