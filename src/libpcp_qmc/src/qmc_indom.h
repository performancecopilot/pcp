/*
 * Copyright (c) 1997-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef QMC_INDOM_H
#define QMC_INDOM_H

#include "qmc.h"

#include <qlist.h>
#include <qstring.h>
#include <qtextstream.h>

class QmcInstance
{
public:
    QmcInstance();
    QmcInstance(int id, const char* name);
    QmcInstance const& operator=(QmcInstance const&);

    void deactivate(int index);
    int inst() const { return my.inst; }
    bool null() const { return (my.inst == (int)PM_IN_NULL); }
    QString name() const { return my.name; }
    int refCount() const { return my.refCount; }
    int refCountInc() { return ++my.refCount; }
    int refCountDec() { return --my.refCount; }
    bool active() const { return my.active; }
    void setActive(bool active) { my.active = active; }
    int index() const { return my.index; }
    void setIndex(int index) { my.index = index; }

private:
    struct {
	int inst;		// Instance internal id
	QString name;		// Instance external id
	int refCount;
	int index;		// Index into pmResult of last fetch
				// May also be used to index the next NULL inst
	bool active;		// Instance was listed in last indom lookup
    } my;
};

class QmcIndom
{
public:
    QmcIndom(int type, QmcDesc &desc);

    int status() const { return my.status; }
    int id() const { return my.id; }

    int numInsts() const { return my.instances.size() - my.nullCount; }
    int numActiveInsts() const { return my.numActive; }

    // Length of instance list - some of the instances may be NULL hence
    // this may be larger than numInsts()
    int listLen() const { return my.instances.size(); }

    // Internal instance id for instance <index>
    int inst(uint index) const { return my.instances[index].inst(); }

    // External instance name for instance <index>
    const QString name(uint index) const { return my.instances[index].name(); }

    bool nullInst(uint index) const { return my.instances[index].null(); }

    // Was this instance listed in the last update?
    bool activeInst(uint index) const { return my.instances[index].active(); }

    // Is this instance referenced by any metrics?
    bool refInst(uint index) const { return my.instances[index].refCount()>0; }

    // Return index into table for instance with external <name>
    // Also adds a reference to this instance for the profile
    int lookup(QString const& name);

    // Add a reference to all instances in this indom
    // Id <active> is set, only reference active instances
    void refAll(bool active = false);

    // Remove a reference to an instance
    void removeRef(uint index);

    // Number of instances referenced
    uint refCount() const { return my.count; }

    // Was the indom different on the last fetch?
    bool changed() const { return my.changed; }

    // About to fetch, so mark this indom as unchanged
    void newFetch() { my.changed = false; my.updated = true; }

    // Mark that the indom was different in a previous fetch
    void hasChanged() { my.changed = true; my.updated = false; }

    int update();	// Update indom with latest instances

    // Has profile changed since last call to genProfile
    bool diffProfile() const { return my.profile; }

    int genProfile();	// Generate profile for current context

    // Likely index into pmResult for instance <inst>
    int index(uint inst) const { return my.instances[inst].index(); }
    void setIndex(uint inst, int index) { my.instances[inst].setIndex(index); }
    
    // Dump some debugging output for this indom
    void dump(QTextStream &os) const;

private:
    struct {
	int status;
	int type;
	pmInDom id;
	QList<QmcInstance> instances;	// Sparse list of instances
	bool profile;			// Does the profile need to be updated
	bool changed;			// Did indom change in the last fetch?
	bool updated;			// Has the indom been updated?
	uint count;			// Number of referenced instances
	uint nullCount;			// Count of NULL instances
	uint nullIndex;			// Index to first NULL instance
	uint numActive;			// Number of active instances
	uint numActiveRef;		// Number of active referenced insts
    } my;
};

#endif	// QMC_INDOM_H
