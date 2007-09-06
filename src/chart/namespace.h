/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <QtCore/QString>
#include <QtGui/QTreeWidget>
#include <QtGui/QTreeWidgetItem>
#include <pcp/pmapi.h>

class QmcContext;

class NameSpace : public QTreeWidgetItem
{
public:
    typedef enum {
	NoType,
	ArchiveRoot,
	HostRoot,
	NonLeafName,
	LeafNullIndom,
	LeafWithIndom,
	InstanceName,
    } Type;

    NameSpace(QTreeWidget *, const QmcContext *, bool);// for root nodes only
    NameSpace(NameSpace *, QString, bool, bool);	// for all other nodes

    QString text(int) const;
    QIcon icon(int) const;
    void setIcon(int, const QIcon &);
    void setOpen(bool);
    void setExpanded(bool expanded)
	{ my.expanded = expanded; setOpen(expanded); }

    QString sourceName();
    QString metricName();
    QString instanceName();
    QmcContext *metricContext() { return my.context; }
    void setType(Type type) { my.type = type; }
    bool isRoot() { return my.type == HostRoot || my.type == ArchiveRoot; }
    bool isLeaf() { return my.type == InstanceName||my.type == LeafNullIndom; }
    bool isInst() { return my.type == InstanceName; }
    bool isArchiveMode() { return my.isArchive; }

    void addToTree(QTreeWidget *);	// add (leaf) node into Selected set
    void removeFromTree(QTreeWidget *);	// take (leaf) nodes from Selected set

    QColor currentColor() { return my.current; }
    void setCurrentColor(QColor, QTreeWidget *);
    QColor originalColor() { return my.original; }
    void setOriginalColor(QColor original) { my.original = original; }

    void setSelectable(bool selectable)
	{
	    if (selectable)
		setFlags(flags() | Qt::ItemIsSelectable);
	    else
		setFlags(flags() & ~Qt::ItemIsSelectable);
	}
    void setExpandable(bool expandable) { (void)expandable; /*TODO*/ }

private:
    void expandMetricNames(QString);
    void expandInstanceNames();
    bool cmp(QTreeWidgetItem *);
    NameSpace *dup(QTreeWidget *);	// copies the root node in addToTree
    NameSpace *dup(QTreeWidget *, NameSpace *);	// copies nodes in addToTree

    struct {
	bool isArchive;
	bool expanded;		// pmGet{ChildrenStatus,Indom} done
	pmDesc desc;		// metric descriptor for metric leaves
	int instid;		// for instance names only
	QmcContext *context;	// metrics class metric context
	QIcon iconic;
	QColor current;		// color we'll use if OK'd
	QColor original;	// color we started with
	QString basename;
	NameSpace *back;
	NameSpace::Type type;
    } my;
};

#endif	// NAMESPACE_H
