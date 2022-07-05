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

#include "qmc_indom.h"
#include "qmc_desc.h"
#include <ctype.h>
#include <QVector>
#include <QStringList>

QmcInstance::QmcInstance()
{
    my.inst = PM_IN_NULL;
    my.refCount = 0;
    my.index = -1;
    my.active = false;
}

QmcInstance::QmcInstance(const QmcInstance &base)
{
    my.inst = base.inst();
    my.name = base.name();
    my.refCount = base.refCount();
    my.index = base.index();
    my.active = base.active();
}

QmcInstance::QmcInstance(int id, const char* name)
{
    my.inst = id;
    my.name = name;
    my.refCount = 0;
    my.index = -1;
    my.active = true;
}

void
QmcInstance::deactivate(int nullIndex)
{
    my.inst = PM_IN_NULL;
    my.name = "";
    my.refCount = 0;
    my.index = nullIndex;
    my.active = false;
}

QmcInstance const&
QmcInstance::operator=(QmcInstance const& rhs)
{
    if (this != &rhs) {
	my.inst = rhs.inst();
	my.name = rhs.name();
	my.refCount = rhs.refCount();
	my.index = rhs.index();
	my.active = rhs.active();
    }
    return *this;
}

QmcIndom::QmcIndom(int type, QmcDesc &desc)
{
    int *instList;
    char **nameList;

    my.type = type;
    my.id = desc.desc().indom;
    my.profile = false;
    my.changed = false;
    my.updated = true;
    my.count = 0;
    my.nullCount = 0;
    my.nullIndex = UINT_MAX;
    my.numActive = 0;
    my.numActiveRef = 0;

    if (my.id == PM_INDOM_NULL)
	my.status = PM_ERR_INDOM;
    else if (my.type == PM_CONTEXT_HOST || my.type == PM_CONTEXT_LOCAL)
	my.status = pmGetInDom(my.id, &instList, &nameList);
    else if (my.type == PM_CONTEXT_ARCHIVE)
	my.status = pmGetInDomArchive(my.id, &instList, &nameList);
    else
	my.status = PM_ERR_NOCONTEXT;

    if (my.status > 0) {
	for (int i = 0; i < my.status; i++)
	    my.instances.append(QmcInstance(instList[i], nameList[i]));
	my.numActive = my.status;
	free(instList);
	free(nameList);

	if (pmDebugOptions.indom) {
	    QTextStream cerr(stderr);
	    cerr << "QmcIndom::QmcIndom: indom ";
	}
    }
    else if (my.status < 0 && pmDebugOptions.pmc) {
	QTextStream cerr(stderr);
	cerr << "QmcIndom::QmcIndom: unable to lookup "
	     << pmInDomStr(my.id) << " from "
	     << (my.type == PM_CONTEXT_ARCHIVE ? "archive" : "host/local")
	     << " source: " << pmErrStr(my.status) << Qt::endl;
    }
}

int
QmcIndom::lookup(QString const &name)
{
    int i;
    bool ok;
    QStringList list;

    for (i = 0; i < my.instances.size(); i++) {
	if (my.instances[i].null())
	    continue;
	if (my.instances[i].name().compare(name) == 0) {
	    if (my.instances[i].refCount() == 0) {
		my.profile = true;
		my.count++;
		if (my.instances[i].active())
		    my.numActiveRef++;
	    }
	    my.instances[i].refCountInc();
	    return i;
	}
    }

    // Match up to the first space
    // Need this for proc and similiar agents

    for (i = 0; i < my.instances.size(); i++) {
	if (my.instances[i].null())
	    continue;
	list = my.instances[i].name().split(QChar(' '));
	if (list.size() <= 1)
	    continue;
	if (name.compare(list.at(0)) == 0) {
	    if (pmDebugOptions.pmc) {
		QTextStream cerr(stderr);
		cerr << "QmcIndom::lookup: inst \"" << name << "\"(" << i
		     << ") matched to \"" << my.instances[i].name() << "\"("
		     << i << ')' << Qt::endl;
	    }
	    if (my.instances[i].refCount() == 0) {
		my.profile = true;
		my.count++;
		if (my.instances[i].active())
		    my.numActiveRef++;
	    }
	    my.instances[i].refCountInc();
	    return i;
	}
    }

    // If the instance requested is numeric, then ignore leading
    // zeros in the instance up to the first space
    int nameNumber = name.toInt(&ok);

    // The requested instance is numeric
    if (ok) {
	for (i = 0; i < my.instances.size(); i++) {
	    if (my.instances[i].null())
		continue;

	    list = my.instances[i].name().split(QChar(' '));
	    if (list.size() <= 1)
		continue;
	    int instNumber = list.at(0).toInt(&ok);
	    if (!ok)
		continue;
	    if (instNumber == nameNumber) {
		if (pmDebugOptions.pmc) {
		    QTextStream cerr(stderr);
		    cerr << "QmcIndom::lookup: numerical inst \""
			 << name << " matched to \"" << my.instances[i].name()
			 << "\"(" << i << ')' << Qt::endl;
		}
		if (my.instances[i].refCount() == 0) {
		    my.profile = true;
		    my.count++;
		    if (my.instances[i].active())
			my.numActiveRef++;
		}
		my.instances[i].refCountInc();
		return i;
	    }
	}
    }

    return -1;	// we don't know about that instance
}

void
QmcIndom::refAll(bool active)
{
    my.numActiveRef = 0;

    for (int i = 0; i < my.instances.size(); i++) {
	if (my.instances[i].null() || (active && !my.instances[i].active()))
	    continue;

	if (my.instances[i].refCount() == 0)
	    my.profile = true;
	if (my.instances[i].active())
	    my.numActiveRef++;

	my.instances[i].refCountInc();
    }
    my.count = my.instances.size() - my.nullCount;
}

void
QmcIndom::removeRef(uint index)
{
    Q_ASSERT(my.instances[index].refCount());

    my.instances[index].refCountDec();
    if (my.instances[index].refCount() == 0) {
	my.profile = true;
	my.count--;
	if (my.instances[index].active())
	    my.numActiveRef--;
    }
}

int
QmcIndom::genProfile()
{
    int i, j;
    int sts = 0;
    int *ptr = NULL;
    QVector<int> list;
    const char *action = NULL;

    // If all instances are referenced or there are no instances
    // then request all instances
    if (my.numActiveRef == my.numActive || my.numActive == 0) {
	sts = pmAddProfile(my.id, 0, NULL);
	action = "ALL";
    }
    // If the number of referenced instances is less than the number
    // of unreferenced active instances, then the smallest profile
    // is to add all the referenced instances
    else if (my.count < (my.numActive - my.numActiveRef)) {
	action = "ADD";
	sts = pmDelProfile(my.id, 0, NULL);
	if (sts >= 0) {
	    list.resize(my.count);
	    for (i = 0, j = 0; i < my.instances.size(); i++)
		if (!my.instances[i].null() && my.instances[i].refCount())
		    list[j++] = my.instances[i].inst();
	    ptr = list.data();
	    sts = pmAddProfile(my.id, list.size(), ptr);
	}
    }
    // Delete those active instances that are not referenced
    else {
	action = "DELETE";
	sts = pmAddProfile(my.id, 0, NULL);
	if (sts >= 0) {
	    list.resize(my.instances.size() - my.count);
	    for (i = 0, j = 0; i < my.instances.size(); i++)
		if (!my.instances[i].null() && 
		    my.instances[i].refCount() == 0 &&
		    my.instances[i].active())
		    list[j++] = my.instances[i].inst();
	    ptr = list.data();
	    sts = pmDelProfile(my.id, list.size(), ptr);
	}
    }

    if (pmDebugOptions.pmc || pmDebugOptions.indom || pmDebugOptions.profile) {
	QTextStream cerr(stderr);
	cerr << "QmcIndom::genProfile: indom = " << pmInDomStr(my.id) << ", count = " 
	     << my.count << ", numInsts = " << numInsts() << ", active = "
	     << my.numActive << ", activeRef = " << my.numActiveRef
	     << ": " << action << " ptr = " << ptr;
	if (sts < 0)
	    cerr << ", sts = " << sts << ": " << pmErrStr(sts);
	cerr << Qt::endl;
    }

    if (sts >= 0)
	my.profile = false;
    return sts;
}

void
QmcIndom::dump(QTextStream &os) const
{
    os << pmInDomStr(my.id) << ": " << numInsts() << " instances ("
       << my.nullCount << " NULL)" << Qt::endl;
    for (int i = 0; i < my.instances.size(); i++)
	if (!my.instances[i].null())
	    os << "  [" << my.instances[i].inst() << "] = \""
	       << my.instances[i].name() << "\" ("
	       << my.instances[i].refCount() << " refs) "
	       << (my.instances[i].active() ? "active" : "inactive")
	       << Qt::endl;
	else
	    os << "  NULL -> " << my.instances[i].index() << Qt::endl;
}

int
QmcIndom::update()
{
    int *instList = NULL;
    char **nameList = NULL;
    int i, j, count;
    int oldLen = my.instances.size();
    uint oldNullCount = my.nullCount;
    int sts = 0;

    // If the indom has already been updated, just check that all instances
    // are referenced and remove any that have gone away.
    if (!my.changed || my.updated) {
	for (i = 0; i < oldLen; i++) {
	    QmcInstance &inst = my.instances[i];
	    if (inst.refCount() || inst.null() || inst.active())
		continue;
	    inst.deactivate(my.nullIndex);
	    my.nullIndex = i;
	    my.nullCount++;
	    my.profile = true;
	}
	if (pmDebugOptions.indom && my.nullCount != oldNullCount) {
	    QTextStream cerr(stderr);
	    cerr << "QmcIndom::update: Cleaning indom " << pmInDomStr(my.id)
		 << ": Removed " << my.nullCount - oldNullCount 
		 << " instances" << Qt::endl;
	}
	return 0;
    }

    my.updated = true;

    if (my.type == PM_CONTEXT_ARCHIVE)
	return 0;

    if (my.type == PM_CONTEXT_HOST || my.type == PM_CONTEXT_LOCAL)
	sts = pmGetInDom(my.id, &instList, &nameList);

    my.numActive = 0;
    my.numActiveRef = 0;

    if (sts > 0) {
	count = sts;
	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    cerr << "QmcIndom::update: Updating indom " << pmInDomStr(my.id)
		 << ": Got " << count << " instances (vs " << numInsts()
		 << ")" << Qt::endl;
	}

	// Any instances which are not in the new indom AND are not
	// referenced can be removed
	for (i = 0; i < oldLen; i++) {
	    QmcInstance &inst = my.instances[i];
	    inst.setActive(false);

	    if (inst.refCount() || inst.null())
		continue;
	    j = 0;
	    if (i < count && inst.inst() == instList[i]) {
		if (inst.name().compare(nameList[i]) == 0)
		    continue;
		else
		    j = count;
	    }
	    for (; j < count; j++) {
		if (inst.inst() == instList[j]) {
		    if (inst.name().compare(nameList[j]) == 0)
			break;
		    else
			j = count;
		}
	    }

	    // If j >= count, then instance i has either changed or gone away
	    if (j >= count) {
		inst.deactivate(my.nullIndex);
		my.nullIndex = i;
		my.nullCount++;
		my.profile = true;
	    }
	}

	for (i = 0; i < count; i++) {
	    // Quick check to see if they are the same
	    if (i < my.instances.size() && 
		my.instances[i].inst() == instList[i] &&
		my.instances[i].name().compare(nameList[i]) == 0) {
		if (pmDebugOptions.indom) {
		    QTextStream cerr(stderr);
		    cerr << "QmcIndom::update: Unchanged \"" << nameList[i]
			 << "\"(" << instList[i] << ')' << Qt::endl;
		}
		my.instances[i].setActive(true);
		my.numActive++;
		if (my.instances[i].refCount())
		    my.numActiveRef++;
		continue;
	    }

	    for (j = 0; j < oldLen; j++) {
		if (my.instances[j].null())
		    continue;

		if (my.instances[j].inst() == instList[i]) {
		    // Same instance and same external name but different
		    // order, mark as active. If it has a different
		    // external name just ignore it
		    if (my.instances[j].name().compare(nameList[i]) == 0) {
			my.instances[j].setActive(true);
			my.numActive++;
			if (my.instances[j].refCount())
			    my.numActiveRef++;
		    }
		    else if (pmDebugOptions.pmc) {
			QTextStream cerr(stderr);
			cerr << "QmcIndom::update: Ignoring \""
			     << nameList[i] 
			     << "\" with identical internal identifier ("
			     << instList[i] << ")" << Qt::endl;
		    }
		    break;
		}
	    }

	    if (j == oldLen) {
		if (pmDebugOptions.indom) {
		    QTextStream cerr(stderr);
		    cerr << "QmcIndom::update: Adding \"" << nameList[i] 
			 << "\"(" << instList[i] << ")" << Qt::endl;
		}
		if (my.nullCount) {
		    uint newindex = my.instances[my.nullIndex].index();
		    my.instances[my.nullIndex] = QmcInstance(instList[i],
							  nameList[i]);
		    my.nullIndex = newindex;
		    my.nullCount--;
		}
		else
		    my.instances.append(QmcInstance(instList[i], nameList[i]));
		my.profile = true;
		my.numActive++;
	    }
	}

	if (pmDebugOptions.indom) {
	    QTextStream cerr(stderr);
	    if (my.instances.size() == oldLen && my.nullCount == oldNullCount)
		cerr << "QmcIndom::update: indom size unchanged" << Qt::endl;
	    else {
		cerr << "QmcIndom::update: indom changed from "
		     << oldLen - oldNullCount << " to " << numInsts()
		     << Qt::endl;
		dump(cerr);
	    }
	}
    }
    else {
	for (i = 0; i < my.instances.size(); i++)
	    my.instances[i].setActive(false);

	if (pmDebugOptions.pmc) {
	    QTextStream cerr(stderr);
	    if (sts == 0)
		cerr << "QmcIndom::update: indom empty!" << Qt::endl;
	    else
		cerr << "QmcIndom::update: unable to lookup "
		     << pmInDomStr(my.id) << " from host/local source: "
		     << pmErrStr(sts) << Qt::endl;
	}
    }

    if (instList)
	free(instList);
    if (nameList)
	free(nameList);

    return sts;
}
