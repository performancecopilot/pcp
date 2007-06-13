/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007, Nathan Scott.  All Rights Reserved.
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
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 *                      Nathan Scott, nathans At debian DoT org
 */
#ifndef NAMESPACE_H
#define NAMESPACE_H

/*
 * NameSpace class to support the PMNS lists in the SourceDialog
 */

#include <qstring.h>
#include <qpixmap.h>
#include <qlistview.h>
#include <pcp/pmapi.h>

typedef enum {
    UNKNOWN,
    ARCHIVE_ROOT,
    HOST_ROOT,
    NONLEAF_NAME,
    LEAF_NULL_INDOM,
    LEAF_WITH_INDOM,
    INSTANCE_NAME,
} NameSpaceType;

class PMC_Context;

class NameSpace : public QListViewItem
{
public:
    NameSpace(QListView *, const PMC_Context *, bool);	// for root nodes only
    NameSpace(NameSpace *, const char *, bool, bool);	// other nodes

    QString text(int) const;
    const QPixmap *pixmap(int) const;
    void setPixmap(int, const QPixmap &);
    void setup(void);
    void setOpen(bool);
    void setExpanded(bool x) { expanded = x; }

    QString sourceName(void);
    QString metricName(void);
    QString instanceName(void);
    PMC_Context *metricContext(void) { return context; }
    void setType(NameSpaceType t) { type = t; }
    bool isRoot(void) { return type==HOST_ROOT || type==ARCHIVE_ROOT; }
    bool isLeaf(void) { return type==INSTANCE_NAME || type==LEAF_NULL_INDOM; }
    bool isInst(void) { return type==INSTANCE_NAME; }
    bool isArchiveMode(void) { return isArchive; }

    void addToList(QListView *);	// add (leaf) node into Selected list
    void removeFromList(QListView *);	// take (leaf) nodes from Selected list

    QColor currentColor() { return current; }
    void setCurrentColor(QColor, QListView *);	// 2nd parameter optional
    QColor originalColor() { return original; }
    void setOriginalColor(QColor c) { original = c; }

private:
    void expandMetricNames(const char *);
    void expandInstanceNames(void);
    bool cmp(QListViewItem *);
    NameSpace *dup(QListView *);	// copies the root node in addToList
    NameSpace *dup(QListView *, NameSpace *);	// copies nodes in addToList

    bool		isArchive;
    bool		expanded;	// pmGet{ChildrenStatus,Indom} done
    pmDesc		desc;		// metric descriptor for metric leaves
    int			instid;		// for instance names only
    PMC_Context		*context;	// PMC metric context
    QColor		current;	// color we'll use if OK'd
    QColor		original;	// color we started with
    QPixmap		pixelmap;
    QString		basename;
    NameSpace		*back;
    NameSpaceType	type;
};

#endif	/* NAMESPACE_H */
