/*
 * Copyright (c) 1997,2005 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
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
 */

#include <strings.h>
#include "qmc_metric.h"
#include "qmc_group.h"

QmcMetricValue::QmcMetricValue()
{
    my.value = 0.0;
    my.currentValue = 0.0;
    my.previousValue = 0.0;
    my.error = PM_ERR_VALUE;
    my.currentError = PM_ERR_VALUE;
    my.previousError = PM_ERR_VALUE;
    my.instance = PM_ERR_INST;
}

QmcMetricValue const&
QmcMetricValue::operator=(QmcMetricValue const& rhs)
{
    if (this != &rhs) {
	my.instance = rhs.my.instance;
	my.value = rhs.my.value;
	my.currentValue = rhs.my.currentValue;
	my.previousValue = rhs.my.previousValue;
	my.stringValue = rhs.my.stringValue;
	my.error = rhs.my.error;
	my.currentError = rhs.my.currentError;
	my.previousError = rhs.my.previousError;
    }
    return *this;
}

QmcMetric::QmcMetric(QmcGroup *group, const char *string,
			double scale, bool active)
{
    pmMetricSpec *metricSpec;
    char *msg;

    my.status = 0;
    my.group = group;
    my.scale = scale;
    my.idIndex = UINT_MAX;
    my.indomIndex = UINT_MAX;
    my.contextIndex = UINT_MAX;
    my.explicitInst = false;
    my.active = active;

    my.status = pmParseMetricSpec(string, 0, NULL, &metricSpec, &msg);
    if (my.status < 0) {
	pmprintf("%s: Error: Unable to parse metric spec:\n%s\n", 
		 pmProgname, msg);
	my.name = QString::null;
	free(msg);
    }
    else {
	my.name = QString(metricSpec->metric);
	setup(group, metricSpec);
	free(metricSpec);
    }
}

QmcMetric::QmcMetric(QmcGroup *group, pmMetricSpec *metricSpec,
		       double scale, bool active)
{
    my.pmid = PM_ID_NULL;
    my.status = 0;
    my.name = QString(metricSpec->metric);
    my.group = group;
    my.scale = scale;
    my.contextIndex = UINT_MAX;
    my.idIndex = 0;
    my.indomIndex = UINT_MAX;
    my.explicitInst = false;
    my.active = active;
    setup(group, metricSpec);
}

void
QmcMetric::setup(QmcGroup* group, pmMetricSpec *metricSpec)
{
    if (my.status >= 0)
	setupDesc(group, metricSpec);
    if (my.status >= 0)
	setupIndom(metricSpec);
    if (my.status < 0)
	return;
    if (pmDebug & DBG_TRACE_PMC)
	dumpAll();
}

QmcMetric::~QmcMetric()
{
    if (hasInstances())
	for (int i = 0; i < my.values.size(); i++)
	    indom()->removeRef(my.values[i].instance());
}

void
QmcMetric::setupDesc(QmcGroup* group, pmMetricSpec *metricSpec)
{
    int contextType = PM_CONTEXT_HOST;
    int descType;
    char *src = NULL;
    char *name = NULL;

    if (metricSpec->isarch == 1)
	contextType = PM_CONTEXT_ARCHIVE;
    else if (metricSpec->isarch == 2)
	contextType = PM_CONTEXT_LOCAL;

    QString source = QString(metricSpec->source);
    my.status = group->use(contextType, source);

    if (my.status >= 0) {
	my.contextIndex = group->contextIndex();
	contextType = context()->source().type();
	my.status = context()->lookupPMID(metricSpec->metric, my.pmid);
	if (my.status >= 0)
	    my.status = context()->lookupInDom(my.pmid, my.indomIndex);
	if (my.status < 0) {
	    name = strdup(nameAscii());
	    src = strdup(context()->source().sourceAscii());
	    pmprintf("%s: Error: %s%c%s: %s\n", 
		     pmProgname,
		     contextType == PM_CONTEXT_LOCAL ?  "@" : src,
		     contextType == PM_CONTEXT_ARCHIVE ? '/' : ':',
		     name, pmErrStr(my.status));
	}
    }
    else  {
	// do nothing, error already reported via pmprintf from
	// QmcGroup::use()
	;
    }

    if (my.status >= 0) {
	descType = desc().desc().type;
	if (descType == PM_TYPE_NOSUPPORT) {
	    my.status = PM_ERR_CONV;
	    name = strdup(nameAscii());
	    src = strdup(context()->source().sourceAscii());
	    pmprintf("%s: Error: %s%c%s is not supported on %s\n",
		     pmProgname, contextType == PM_CONTEXT_LOCAL ? "@" : src,
		     (contextType == PM_CONTEXT_ARCHIVE ? '/' : ':'),
		     name, context()->source().hostAscii());
	}
	else if (descType == PM_TYPE_AGGREGATE ||
		 descType == PM_TYPE_AGGREGATE_STATIC ||
		 descType == PM_TYPE_UNKNOWN) {
	    my.status = PM_ERR_CONV;
	    name = strdup(nameAscii());
	    src = strdup(context()->source().sourceAscii());
	    pmprintf("%s: Error: %s%c%s has type \"%s\","
		     " which is not a number or a string\n",
		     pmProgname, contextType == PM_CONTEXT_LOCAL ? "@" : src,
		     (contextType == PM_CONTEXT_ARCHIVE ? '/' : ':'),
		     name, pmTypeStr(descType));
	}
    }

    if (name)
	free(name);
    if (src)
	free(src);
}

void
QmcMetric::setupIndom(pmMetricSpec *metricSpec)
{
    int i, j;
    QmcIndom *indomPtr = indom();
    
    if (!hasIndom()) {
	if (metricSpec->ninst > 0) {
	    my.status = PM_ERR_INST;
	    dumpErr(metricSpec->inst[0]);
	}
	else
	    setupValues(1);
    }
    else if (metricSpec->ninst) {
	Q_ASSERT(hasInstances());
	setupValues(metricSpec->ninst);

	for (i = 0 ; i < metricSpec->ninst && my.status >= 0; i++) {
	    j = indomPtr->lookup(metricSpec->inst[i]);
	    if (j >= 0)
		my.values[i].setInstance(j);
	    else {
		my.status = PM_ERR_INST;
		my.values[i].setInstance(PM_ERR_INST);
		dumpErr(metricSpec->inst[i]);
	    }
	}
	my.explicitInst = true;
    }
    else {
	Q_ASSERT(hasInstances());

	if (my.active) {
	    setupValues(indomPtr->numActiveInsts());
	    indomPtr->refAll(my.active);

	    for (i = 0, j = 0; i < indomPtr->listLen(); i++)
		if (!indomPtr->nullInst(i) && indomPtr->activeInst(i))
		    my.values[j++].setInstance(i);
	}
	else {
	    setupValues(indomPtr->numInsts());
	    indomPtr->refAll(my.active);
  
	    for (i = 0, j = 0; i < indomPtr->listLen(); i++)
		if (!indomPtr->nullInst(i))
		    my.values[j++].setInstance(i);
	}
    }
}

void
QmcMetric::setupValues(int num)
{
    int i, oldLen = my.values.size();

    if (num == 0)
	my.values.clear();
    else {
	if (my.values.size() > num)
	    for (i = num; i < my.values.size(); i++)
		my.values.removeAt(i);
	for (i = oldLen; i < num; i++)
	    my.values.append(QmcMetricValue());
    }
}

QString
QmcMetric::spec(bool srcFlag, bool instFlag, uint instance) const
{
    QString str;
    int i, len = 4;

    if (srcFlag)
	len += context()->source().source().size();
    len += name().size();
    if (hasInstances() && instFlag) {
	if (instance != UINT_MAX)
	    len += instName(instance).size() + 2;
	else
	    for (i = 0; i < numInst(); i++)
		len += instName(i).size() + 4;
    }

    if (srcFlag) {
	str.append(context()->source().source());
	if (context()->source().type() == PM_CONTEXT_ARCHIVE)
	    str.append(QChar('/'));
	else
	    str.append(QChar(':'));
    }
    str.append(name());
    if (hasInstances() && instFlag) {
	str.append(QChar('['));
	str.append(QChar('\"'));
	if (instance != UINT_MAX)
	    str.append(instName(instance));
	else if (numInst()) {
	    str.append(instName(0));
	    for (i = 1; i < numInst(); i++) {
		str.append("\", \"");
		str.append(instName(i));
	    }
	}
	str.append("\"]");
    }

    return str;
}

void
QmcMetric::dumpSource(QTextStream &os) const
{
    switch(context()->source().type()) {
    case PM_CONTEXT_LOCAL:
	os << "@:";
	break;
    case PM_CONTEXT_HOST:
	os << context()->source().source() << ':';
	break;
    case PM_CONTEXT_ARCHIVE:
	os << context()->source().source() << '/';
	break;
    }
}

void
QmcMetric::dumpValue(QTextStream &stream, uint inst) const
{
    if (error(inst) < 0)
	stream << pmErrStr(error(inst));
    else if (!real())
	stream << stringValue(inst);
    else if (!event())
	stream << value(inst) << " " << desc().units();
}

void
QmcMetric::dump(QTextStream &stream, bool srcFlag, uint instance) const
{
    if (event())
	dumpEventMetric(stream, srcFlag, instance);
    else
	dumpSampledMetric(stream, srcFlag, instance);
}

void
QmcMetric::dumpSampledMetric(QTextStream &stream, bool srcFlag, uint instance) const
{
    Q_ASSERT(!event());

    stream << name();

    if (srcFlag == true)
	dumpSource(stream);

    if (my.status < 0)
	stream << ": " << pmErrStr(my.status) << endl;
    else if (hasInstances()) {
	if (instance == UINT_MAX) {
	    if (numInst() == 1)
		stream << ": 1 instance";
	    else
		stream << ": " << numInst() << " instances";
	    if (indom()->changed())
		stream << " (indom has changed)";
	    stream << endl;

	    for (int i = 0; i < numInst(); i++) {
		stream << "  [" << instID(i) << " or \"" << instName(i)
		       << "\" (" << my.values[i].instance() << ")] = ";
		dumpValue(stream, i);
		stream << endl;
	    }
	}
	else {
	    stream << '[' << instID(instance) << " or \"" << instName(instance) 
		   << "\" (" << my.values[instance].instance() << ")] = ";
	    dumpValue(stream, instance);
	    stream << endl;
	}
    }
    else {
	stream << " = ";
	dumpValue(stream, 0);
	stream << endl;
    }
}

QTextStream&
operator<<(QTextStream &stream, const QmcMetric &metric)
{
    metric.dumpSource(stream);
    stream << metric.name();
    if (metric.numInst()) {
	stream << "[\"" << metric.instName(0);
	for (int i = 1; i < metric.numValues(); i++)
	    stream << "\", \"" << metric.instName(i);
	stream << "\"]";
    }
    return stream;
}

void
QmcMetric::setScaleUnits(pmUnits const& units)
{
    QmcContext *context = my.group->context(my.contextIndex);
    QmcDesc &desc = context->desc(my.pmid);
    desc.setScaleUnits(units);
}

int
QmcMetric::update()
{
    uint i, err = 0;
    uint num = numValues();
    int sts;
    pmAtomValue ival, oval;
    double delta = context()->timeDelta();
    static int wrap = -1;

    if (num == 0 || my.status < 0)
	return my.status;

    // PCP_COUNTER_WRAP in environment enables "counter wrap" logic
    if (wrap == -1)
	wrap = (getenv("PCP_COUNTER_WRAP") != NULL);

    for (i = 0; i < num; i++) {
	my.values[i].setError(my.values[i].currentError());
	if (my.values[i].error() < 0)
	    err++;
	if (pmDebug & DBG_TRACE_VALUE) {
	    QTextStream cerr(stderr);
	    if (my.values[i].error() < 0)
		cerr << "QmcMetric::update: " << spec(true, true, i) 
		     << ": " << pmErrStr(my.values[i].error()) << endl;
	}
    }

    if (!real())
	return err;

    if (desc().desc().sem == PM_SEM_COUNTER) {
	for (i = 0; i < num; i++) {
	    QmcMetricValue& value = my.values[i];

	    if (value.error() < 0) {		// we already know we
		value.setValue(0.0);		// don't have this value
		continue;
	    }
	    if (value.previousError() < 0) {	// we need two values
		value.setValue(0.0);		// for a rate
		value.setError(value.previousError());
		err++;
		if (pmDebug & DBG_TRACE_VALUE) {
		    QTextStream cerr(stderr);
		    cerr << "QmcMetric::update: Previous: " 
			 << spec(true, true, i) << ": "
			 << pmErrStr(value.error()) << endl;
		}
		continue;
	    }

	    value.setValue(value.currentValue() - value.previousValue());

	    // wrapped going forwards
	    if (value.value() < 0 && delta > 0) {
		if (wrap) {
		    switch(desc().desc().type) {
		    case PM_TYPE_32:
		    case PM_TYPE_U32:
			value.addValue((double)UINT_MAX+1);
			break;
		    case PM_TYPE_64:
		    case PM_TYPE_U64:
			value.addValue((double)ULONGLONG_MAX+1);
			break;
		    }
		}
		else {			// counter not monotonic
		    value.setValue(0.0);	// increasing
		    value.setError(PM_ERR_VALUE);
		    err++;
		    continue;
		}
	    }
	    // wrapped going backwards
	    else if (value.value() > 0 && delta < 0) {
		if (wrap) {
		    switch(desc().desc().type) {
		    case PM_TYPE_32:
		    case PM_TYPE_U32:
			value.subValue((double)UINT_MAX+1);
			break;
		    case PM_TYPE_64:
		    case PM_TYPE_U64:
			value.subValue((double)ULONGLONG_MAX+1);
			break;
		    }
		}
		else {			// counter not monotonic
		    value.setValue(0.0);	// increasing
		    value.setError(PM_ERR_VALUE);
		    err++;
		    continue;
		}
	    }

	    if (delta != 0)			// sign of delta and v
		value.divValue(delta);		// should be the same
	    else
		value.setValue(0.0);		// nothing can have happened
	}
    }
    else {
	for (i = 0; i < num; i++) {
	    QmcMetricValue& value = my.values[i];
	    if (value.error() < 0)
		value.setValue(0.0);
	    else
		value.setValue(value.currentValue());
	}
    }

    if (my.scale != 0.0) {
	for (i = 0; i < num; i++) {
	    if (my.values[i].error() >= 0)
		my.values[i].divValue(my.scale);
	}
    }

    if (desc().useScaleUnits()) {
	for (i = 0; i < num; i++) {
	    if (my.values[i].error() < 0)
		continue;
	    ival.d = my.values[i].value();
	    pmUnits units = desc().desc().units;
	    sts = pmConvScale(PM_TYPE_DOUBLE, &ival, &units,
			      &oval, (pmUnits *)&(desc().scaleUnits()));
	    if (sts < 0)
		my.values[i].setError(sts);
	    else {
		my.values[i].setValue(oval.d);
		if (pmDebug & DBG_TRACE_VALUE) {
		    QTextStream cerr(stderr);
		    cerr << "QmcMetric::update: scaled " << my.name
			 << " from " << ival.d << " to " << oval.d
			 << endl;
		}
	    }
	}
    }

    return err;
}

void
QmcMetric::dumpAll() const
{
    QTextStream cerr(stderr);
    cerr << *this << " from " << context()->source().desc() 
	 << " with scale = "  << my.scale << " and units = " << desc().units() 
	 << endl;
}

void
QmcMetric::dumpErr() const
{
    pmprintf("%s: Error: %s: %s\n", pmProgname,
	     (const char *)spec(true).toAscii(), pmErrStr(my.status));
}

// Instance list may not be valid, so pass inst as a string rather than
// as an index

void
QmcMetric::dumpErr(const char *inst) const
{
    pmprintf("%s: Error: %s[%s]: %s\n", pmProgname,
	     (const char *)spec(true).toAscii(), inst, pmErrStr(my.status));
}

const char *
QmcMetric::formatNumber(double value)
{
    static char buf[8];

    if (value >= 0.0) {
	if (value > 99950000000000.0)
	    strcpy(buf, "  inf?");
	else if (value > 99950000000.0)
	    sprintf(buf, "%5.2fT", (double)((long double)value / (long double)1000000000000LL));
	else if (value > 99950000.0)
	    sprintf(buf, "%5.2fG", (double)((long double)value / (long double)1000000000));
	else if (value > 99950.0)
	    sprintf(buf, "%5.2fM", (double)((long double)value / (long double)1000000));
	else if (value > 99.95)
	    sprintf(buf, "%5.2fK", value / 1000.0);
	else if (value > 0.005)
	    sprintf(buf, "%5.2f ", value);
	else
	    strcpy(buf, " 0.00 ");
    }
    else {
	if (value < -9995000000000.0)
	    strcpy(buf, " -inf?");
	else if (value < -9995000000.0)
	    sprintf(buf, "%.2fT", (double)((long double)value / (long double)1000000000000LL));
	else if (value < -9995000.0)
	    sprintf(buf, "%.2fG", (double)((long double)value / (long double)1000000000));
	else if (value < -9995.0)
	    sprintf(buf, "%.2fM", (double)((long double)value / (long double)1000000));
	else if (value < -9.995)
	    sprintf(buf, "%.2fK", value / 1000.0);
	else if (value < -0.005)
	    sprintf(buf, "%.2f ", value);
	else
	    strcpy(buf, " 0.00  ");
    }

    return buf;
}

void
QmcMetric::shiftValues()
{
    for (int i = 0; i < my.values.size(); i++)
	my.values[i].shiftValues();
}

void
QmcMetric::setError(int sts)
{
    for (int i = 0; i < numValues(); i++) {
	QmcMetricValue &value = my.values[i];
	value.setCurrentError(sts);
    }
}

void
QmcMetric::extractValues(pmValueSet const* set)
{
    int i, j, index, inst;
    pmValue const *value = NULL;
    bool found;
    QmcIndom *indomPtr = indom();

    Q_ASSERT(set->pmid == desc().id());

    if (set->numval > 0) {
	if (hasIndom()) {
	    // If the number of instances are not the expected number
	    // then mark the indom as changed
	    if (!my.explicitInst && (my.values.size() != set->numval)) {
		if (pmDebug & DBG_TRACE_INDOM) {
		    QTextStream cerr(stderr);
		    cerr << "QmcMetric::extractValues: implicit indom "
			 << pmInDomStr(indomPtr->id()) << " changed ("
			 << set->numval << " != " << my.values.size() << ')'
			 << endl;
		}
		indomPtr->hasChanged();
		updateIndom();
	    }

	    for (i = 0; i < numInst(); i++) {
		QmcMetricValue &valueRef = my.values[i];
		inst = my.values[i].instance();
		index = indomPtr->index(inst);
		found = false;

		// If the index is within range, try it first
		if (index >= 0 && index < set->numval) {
		    value = &(set->vlist[index]);

		    // Found it in the same spot as last time
		    if (value->inst == indomPtr->inst(inst))
			found = true;
		}

		// Search for it from the top
		for (j = 0; found == false && j < set->numval; j++) {
		    if (set->vlist[j].inst == indomPtr->inst(inst)) {
			index = j;
			value = &(set->vlist[j]);
			indomPtr->setIndex(inst, j);
			found = true;
		    }
		}

		if (found) {
		    if (real())
			extractNumericMetric(set, value, valueRef);
		    else if (!event())
			extractArrayMetric(set, value, valueRef);
		    else
			extractEventMetric(set, index, valueRef);
		}
		else {	// Cannot find it
		    if (pmDebug & DBG_TRACE_OPTFETCH) {
			QTextStream cerr(stderr);
			cerr << "QmcMetric::extractValues: "
			     << spec(true, true, i) << ": "
			     << pmErrStr(PM_ERR_VALUE) << endl;
		    }

		    if (valueRef.previousError() != PM_ERR_VALUE)
			indomPtr->hasChanged();

		    valueRef.setCurrentError(PM_ERR_VALUE);
		}
	    }
	}
	else if (set->numval == 1) {
	    // We have no instances at this point in time
	    if (my.values.size() == 0 && hasInstances())
		indomPtr->hasChanged();
	    else {
		QmcMetricValue &valueRef = my.values[0];
		value = &(set->vlist[0]);

		if (real())
		    extractNumericMetric(set, value, valueRef);
		else if (!event())
		    extractArrayMetric(set, value, valueRef);
		else
		    extractEventMetric(set, 0, valueRef);
	    }
	}
	else {	// Did not expect any instances
	    if (pmDebug & DBG_TRACE_OPTFETCH) {
		QTextStream cerr(stderr);
		cerr << "QmcMetric::extractValues: " << spec(true) 
		     << " is a singular metric but result contained "
		     << set->numval << " values" << endl;
	    }
	    setError(PM_ERR_VALUE);
	}
    }
    else if (set->numval == 0) {
	if (!(hasInstances() && numInst() == 0)) {
	    if (pmDebug & DBG_TRACE_OPTFETCH) {
		QTextStream cerr(stderr);
		cerr << "QmcMetric::extractValues: numval == 0: "
		     << spec(true, false) << ": " << pmErrStr(PM_ERR_VALUE)
		     << endl;
	    }
	    setError(PM_ERR_VALUE);
	    if (hasInstances())
		indomPtr->hasChanged();
	}
    }
    else {
	if (pmDebug & DBG_TRACE_OPTFETCH) {
	    QTextStream cerr(stderr);
	    cerr << "QmcMetric::extractValues: numval < 0: "
		 << spec(true, false)
		 << ": " << pmErrStr(set->numval) << endl;
	}
	setError(set->numval);
	if (hasInstances())
	    indomPtr->hasChanged();
    }
}

/*
 * Display-able aggregate (for diagnostics) - up to
 * first 16 characters displayed.  Uses a similar
 * approach to that taken in pmPrintValue().
 */
void
QmcMetric::aggregateAsString(pmValue const *vp, char *buffer, int buflen)
{
    char *p = &vp->value.pval->vbuf[0];

    memset(buffer, '.', buflen);
    for (int i = 0; i < (buflen/2)-1; i++, p++) {
	if (i < vp->value.pval->vlen - PM_VAL_HDR_SIZE)
	    sprintf(buffer + (i*2), "%02x", *p & 0xff);
    }
    buffer[buflen-1] = '\0';
}

void
QmcMetric::extractArrayMetric(pmValueSet const *set, pmValue const *vp, QmcMetricValue &valueRef)
{
    pmAtomValue result;
    int sts, type = desc().desc().type;

    if (aggregate(type)) {
	char buffer[32];
	aggregateAsString(vp, buffer, sizeof(buffer));
	valueRef.setStringValue(buffer);
    }
    else if ((sts = pmExtractValue(set->valfmt, vp,
		    type, &result, PM_TYPE_STRING)) >= 0) {
	valueRef.setStringValue(result.cp);
	if (result.cp)
	    free(result.cp);
    }
    else {
	valueRef.setCurrentError(sts);
    }
}

void
QmcMetric::extractNumericMetric(pmValueSet const *set, pmValue const *value, QmcMetricValue &valueRef)
{
    pmAtomValue result;
    int sts;

    if ((sts = pmExtractValue(set->valfmt, value,
		    desc().desc().type, &result, PM_TYPE_DOUBLE)) >= 0)
	valueRef.setCurrentValue(result.d);
    else
	valueRef.setCurrentError(sts);
}

void
QmcMetricValue::extractEventRecords(QmcContext *context, int recordCount, pmResult **result)
{
    pmID parameterID;
    pmID missedID = QmcEventRecord::eventMissed();
    pmID flagsID = QmcEventRecord::eventFlags();
    int i, p, r, parameterCount;

    my.eventRecords.resize(recordCount);

    for (r = 0; r < recordCount; r++) {
	QmcEventRecord &record = my.eventRecords[r];

	// count is the size of my.parameter vector (less flags/missed)
	parameterCount = 0;
	for (i = 0; i < result[r]->numpmid; i++) {
	    parameterID = result[r]->vset[i]->pmid;
	    if (parameterID != flagsID && parameterID != missedID)
		parameterCount++;
        }

	// initialise this record
	record.setTimestamp(&result[r]->timestamp);
	record.setParameterCount(parameterCount);
	record.setMissed(0);
	record.setFlags(0);

	// i indexes into result[r], p indexes into my.parameter vector
	for (i = p = 0; i < result[r]->numpmid; i++) {
	    pmValueSet *valueSet = result[r]->vset[i];
	    parameterID = valueSet->pmid;
	    if (parameterID == flagsID)
		record.setFlags(valueSet->vlist[0].value.lval);
	    else if (parameterID == missedID)
		record.setMissed(valueSet->vlist[0].value.lval);
	    else
		record.setParameter(p++, parameterID, context, valueSet);
	}

	if (pmDebug & DBG_TRACE_VALUE) {
	    QTextStream cerr(stderr);
	    pmValueSet *valueSet = result[r]->vset[0];
	    record.dump(cerr, valueSet->vlist[0].inst, r);
	}
    }
}

void
QmcMetric::extractEventMetric(pmValueSet const *valueSet, int index, QmcMetricValue &valueRef)
{
    pmValueSet *values = (pmValueSet *)valueSet;
    pmResult **result;
    int sts;

    if ((sts = pmUnpackEventRecords(values, index, &result)) >= 0) {
	valueRef.extractEventRecords(context(), sts, result);
	pmFreeEventResult(result);
    }
    else {
	valueRef.setCurrentError(sts);
    }
}

int
QmcEventRecord::setParameter(int index, pmID pmid, QmcContext *context, pmValueSet const *vsp)
{
    QString *name;
    QmcDesc *desc;
    QmcIndom *indom;
    int sts, type;

    if (vsp->numval <= 0)	// no value or an error
	return vsp->numval;

    if ((sts = context->lookup(pmid, &name, &desc, &indom)) < 0)
	return sts;

    QmcEventParameter &parameter = my.parameters[index];
    parameter.setPMID(pmid);
    parameter.setNamePtr(name);
    parameter.setDescPtr(desc);
    parameter.setIndomPtr(indom);
    parameter.setValueCount(vsp->numval);

    type = desc->desc().type;
    for (int i = 0; i < vsp->numval; i++) {
	pmAtomValue result;
	const pmValue *vp = &vsp->vlist[i];
	QmcMetricValue *value = parameter.valuePtr(i);

	sts = PM_ERR_TYPE;	// no nesting events
	if (QmcMetric::real(type) == true) {
	    if ((sts = pmExtractValue(vsp->valfmt, vp,
			    type, &result, PM_TYPE_DOUBLE)) >= 0)
		value->setCurrentValue(result.d);
	} else if (QmcMetric::aggregate(type) == true) {
	    char buffer[32];
	    QmcMetric::aggregateAsString(vp, buffer, sizeof(buffer));
	    value->setStringValue(buffer);
	} else if (QmcMetric::event(type) == false) {
	    if ((sts = pmExtractValue(vsp->valfmt, vp,
			    type, &result, PM_TYPE_STRING)) >= 0) {
		value->setStringValue(result.cp);
		free(result.cp);
	    }
	}
	value->setInstance(vp->inst);
	if (sts < 0)
	    value->setCurrentError(sts);
    }
    return 0;
}

QString
QmcEventRecord::parameterAsString(int index) const
{
    QString identifier;

    if (index >= my.parameters.size())
	return QString::null;

    int type = my.parameters[index].type();
    if (QmcMetric::real(type))
	return QString::number(my.parameters.at(index).value(0));
    if (QmcMetric::event(type))
	return QString::null;
    return my.parameters.at(index).stringValue(0);
}

QString
QmcEventRecord::identifier() const
{
    //
    // If the ID flag is set, the identifier is always the
    // first parameter.
    //
    if (my.flags & PM_EVENT_FLAG_ID)
	return parameterAsString(0);
    return QString::null;
}

QString
QmcEventRecord::parent() const
{
    //
    // If PARENT flag is set, then parent identifier is either
    // the first parameter (if no ID, bit wierd) or the second
    // (thats expected typical usage anyway).
    //
    if (my.flags & PM_EVENT_FLAG_PARENT)
	return parameterAsString((my.flags & PM_EVENT_FLAG_ID) != 0);
    return QString::null;
}

void
QmcEventRecord::parameterSummary(QString &os, int instID) const
{
    for (int i = 0; i < my.parameters.size(); i++)
	my.parameters.at(i).summary(os, instID);
}

void
QmcEventParameter::summary(QString &os, int instID) const
{
    pmDesc desc = my.desc->desc();

    for (int i = 0; i < my.values.size(); i++) {
	QmcMetricValue const &value = my.values.at(i);

        os.append("    ").append(*my.name);
        if (desc.indom != PM_INDOM_NULL) {
            if (my.values.size() > 1)
                os.append("\n").append("        ");
            QString name = my.indom->name(instID);
            if (name == QString::null)
                os.append("[").append(instID).append("]");
            else
                os.append("[\"").append(name).append("\"]");
        }
        os.append(" ");

        if (QmcMetric::real(desc.type))
            os.append(QString::number(value.currentValue()));
        else if (QmcMetric::aggregate(desc.type))
            os.append("[").append(value.stringValue()).append("]");
        else if (QmcMetric::event(desc.type) == false)
            os.append("\"").append(value.stringValue()).append("\"");
        os.append("\n");
    }
}

void
QmcEventParameter::setValueCount(int numInst)
{
    my.values.resize(numInst);
}

QmcMetricValue *
QmcEventParameter::valuePtr(int inst)
{
    return &my.values[inst];
}

int
QmcEventParameter::type() const
{
    return my.desc->desc().type;
}

double
QmcEventParameter::value(int inst) const
{
    return my.values[inst].currentValue();
}

QString
QmcEventParameter::stringValue(int inst) const
{
    return my.values[inst].stringValue();
}

void
QmcEventParameter::dump(QTextStream &os, int instID) const
{
    pmDesc desc = my.desc->desc();

    for (int i = 0; i < my.values.size(); i++) {
	QmcMetricValue const &value = my.values.at(i);

	os << "    " << *my.name;
	if (desc.indom != PM_INDOM_NULL) {
	    if (my.values.size() > 1)
		os << endl << "        ";
	    QString name = my.indom->name(instID);
	    if (name == QString::null)
		os << "[" << instID << "]";
	    else
		os << "[\"" << name << "\"]";
	}
	os << " ";

	if (QmcMetric::real(desc.type))
	    os << value.currentValue();
	else if (QmcMetric::aggregate(desc.type))
	    os << "[" << value.stringValue() << "]";
	else if (QmcMetric::event(desc.type) == false)
	    os << "\"" << value.stringValue() << "\"";
	os << endl;
    }
}

void
QmcEventRecord::dump(QTextStream &os, int instID, uint recordID) const
{
    os << "  " << QmcSource::timeStringBrief(&my.timestamp);
    os << " --- event record [" << recordID << "]";
    if (my.flags) {
	os.setIntegerBase(16);
	os << " flags 0x" << (uint)my.flags << " (" << pmEventFlagsStr(my.flags) << ")";
	os.setIntegerBase(10);
    }
    os << " ---" << endl;
    if (my.flags & PM_EVENT_FLAG_MISSED)
	os << " ==> " << my.missed << " missed event records" << endl;
    for (int i = 0; i < my.parameters.size(); i++)
	my.parameters.at(i).dump(os, instID);
}

void
QmcMetricValue::dumpEventRecords(QTextStream &os, int instID) const
{
    os << my.eventRecords.size() << " event records" << endl;
    for (int i = 0; i < my.eventRecords.size(); i++)
	my.eventRecords.at(i).dump(os, instID, i);
}

void
QmcMetric::dumpEventMetric(QTextStream &os, bool srcFlag, uint instance) const
{
    Q_ASSERT(event());

    for (int i = 0; i < my.values.size(); i++) {
	int instID;

	if (srcFlag == true)
	    dumpSource(os);
	os << name();

	instID = PM_IN_NULL;
	if (hasInstances()) {
	    if (instance != UINT_MAX)
		instID = (int)instance;
	    else
		instID = my.values[i].instance();

	    QString inst = instName(instID);
	    if (inst == QString::null)
		os << "[" << instID << "]";
	    else
		os << "[\"" << inst << "\"]";
	}
	os << ": ";
	my.values[i].dumpEventRecords(os, instID);
    }
}

pmID
QmcEventRecord::eventMissed(void)
{
    static pmID eventMissed = PM_IN_NULL;
    static const char *nameMissed[] = { "event.missed" };

    if (eventMissed == PM_ID_NULL)
        if (pmLookupName(1, (char **)nameMissed, &eventMissed) < 0)
            eventMissed = PM_ID_NULL;
    return eventMissed;
}

pmID
QmcEventRecord::eventFlags(void)
{
    static pmID eventFlags = PM_IN_NULL;
    static const char *nameFlags[] = { "event.flags" };

    if (eventFlags == PM_ID_NULL)
	if (pmLookupName(1, (char **)nameFlags, &eventFlags) < 0)
	    eventFlags = PM_ID_NULL;
    return eventFlags;
}

bool
QmcMetric::updateIndom(void)
{
    int i = 0, j, oldNum = numInst(), newNum, newInst;
    QmcIndom *indomPtr = indom();

    if (status() < 0 || !hasIndom())
	return false;

    if (indomPtr->changed())
	indomPtr->update();

    my.explicitInst = false;

    newNum = my.active ? indomPtr->numActiveInsts() : indomPtr->numInsts();

    // If the number of instances are the same then we know that no
    // modifications to the metric instance list is required as these
    // instances are all referenced in the indom
    //
    // If the instance list is only active instances, then we need to
    // check all the instances as the number may be the same
    //
    if (newNum == oldNum) {
	if (my.active) {
	    for (i = 0; i < my.values.size(); i++)
		if (!indomPtr->activeInst(my.values[i].instance()))
		    break;
	}
	if (!my.active || i == my.values.size()) {
	    if (pmDebug & DBG_TRACE_INDOM) {
		QTextStream cerr(stderr);
		cerr << "QmcMetric::updateIndom: No change required" << endl;
	    }
	    return false;
	}
    }

    // Duplicate the current values
    // Replace the old index into the indom instance list with the
    // internal instance identifiers so that these can be correlated
    // if the order of instances changes
    QList<QmcMetricValue> oldValues = my.values;
    for (i = 0; i < oldNum; i++) {
	oldValues[i].setInstance(indomPtr->inst(my.values[i].instance()));
	indomPtr->removeRef(my.values[i].instance());
    }

    setupValues(newNum);
    indomPtr->refAll(my.active);

    if (my.active) {
	for (i = 0, j = 0; i < indomPtr->listLen(); i++)
	    if (!indomPtr->nullInst(i) && indomPtr->activeInst(i))
		my.values[j++].setInstance(i);
    }
    else {
	for (i = 0, j = 0; i < indomPtr->listLen(); i++)
	    if (!indomPtr->nullInst(i))
		my.values[j++].setInstance(i);
    }

    // Copy values of instances that have not gone away
    // Note that their position may have changed
    for (i = 0; i < my.values.size(); i++) {
	if (i < oldValues.size() &&
	    indomPtr->inst(my.values[i].instance()) ==
					oldValues[i].instance()) {
	    newInst = my.values[i].instance();
	    my.values[i] = oldValues[i];
	    my.values[i].setInstance(newInst);
	    continue;
	}
	for (j = 0; j < oldValues.size(); j++)
	    if (indomPtr->inst(my.values[i].instance()) ==
					oldValues[j].instance()) {
		newInst = my.values[i].instance();
		my.values[i] = oldValues[j];
		my.values[i].setInstance(newInst);
		break;
	    }

	// Need to set all error flags to avoid problems with rate conversion
	if (j == oldValues.size())
	    my.values[i].setAllErrors(PM_ERR_VALUE);
    }

    if (pmDebug & DBG_TRACE_PMC) {
	QTextStream cerr(stderr);
	cerr << "QmcMetric::updateIndom: " << spec(true) << ": Had " 
	     << oldNum << " instances, now have " << numInst() << endl;
    }

    indomPtr->update();

    return true;
}

int
QmcMetric::addInst(QString const& name)
{
    if (my.status < 0)
	return my.status;

    if (!hasInstances())
	return PM_ERR_INDOM;

    int i = indom()->lookup(name);
    if (i >= 0) {
	setupValues(my.values.size() + 1);
	my.values.last().setInstance(i);
    }

    return i;
}

void
QmcMetric::removeInst(uint index)
{
    Q_ASSERT(hasInstances());
    indom()->removeRef(my.values[index].instance());
    my.values.removeAt(index);
}
