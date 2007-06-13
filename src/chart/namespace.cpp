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

#include <qapplication.h>
#include <qmessagebox.h>
#include <qlistview.h>
#include <qstring.h>
#include "namespace.h"
#include "source.h"
#include "chart.h"

NameSpace::NameSpace(NameSpace *parent, const char *name, bool inst, bool arch)
    : QListViewItem(parent),
      expanded(false),
      pixelmap(NULL)
{
    back = parent;
    context = parent->context;
    basename = QApplication::tr(name);
    type = inst ? INSTANCE_NAME : UNKNOWN;
    isArchive = arch;
}

NameSpace::NameSpace(QListView *list, const PMC_Context *ctxt, bool arch)
    : QListViewItem(list, "root"),
      expanded(false)
{
    back = this;
    context = (PMC_Context *)ctxt;
    basename = Source::makeComboText(ctxt);
    if ((isArchive = arch) == TRUE) {
	pixelmap = QPixmap::fromMimeSource("archive.png");
	type = ARCHIVE_ROOT;
    }
    else {
	pixelmap = QPixmap::fromMimeSource("computer.png");
	type = HOST_ROOT;
    }
}

QString NameSpace::sourceName(void)
{
    return Source::makeSourceBaseName(context);
}

QString NameSpace::metricName(void)
{
    QString s;

    if (back->isRoot())
	s = text(1);
    else if (type == INSTANCE_NAME)
	s = back->metricName();
    else {
	s = back->metricName();
	s.append(".");
	s.append(text(1));
    }
    return s;
}

QString NameSpace::instanceName(void)
{
    if (type == INSTANCE_NAME)
	return text(1);
    return QString(NULL);
}

void NameSpace::setOpen(bool o)
{
    fprintf(stderr, "NameSpace::setOpen on %p %s (expanded=%s, open=%s)\n",
	    this, metricName().ascii(), expanded ? "y" : "n", o ? "y" : "n");

    if (!expanded) {
	pmUseContext(context->hndl());
	listView()->setUpdatesEnabled(FALSE);
	if (type == LEAF_WITH_INDOM)
	    expandInstanceNames();
	else if (type != INSTANCE_NAME)
	    expandMetricNames(isRoot() ? "" : metricName().ascii());
	listView()->setUpdatesEnabled(TRUE);
    }
    QListViewItem::setOpen(o);
}

static char *namedup(const char *name, const char *suffix)
{
    char *n;

    n = (char *)malloc(strlen(name) + 1 + strlen(suffix) + 1);
    sprintf(n, "%s.%s", name, suffix);
    return n;
}

void NameSpace::expandMetricNames(const char *name)
{
    char	**offspring = NULL;
    int		*status = NULL;
    pmID	*pmidlist = NULL;
    int		i, nleaf = 0;
    int		sts, noffspring;
    NameSpace	*m, **leaflist = NULL;

    sts = pmGetChildrenStatus(name, &offspring, &status);
    if (sts < 0) {
	QString msg = QString();
	if (isRoot())
	    msg.sprintf("Cannot get metric names from source\n%s: %s.\n\n",
		basename.ascii(), pmErrStr(sts));
	else
	    msg.sprintf("Cannot get children of node\n\"%s\".\n%s.\n\n",
		name, pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
	return;
    }
    else {
	noffspring = sts;
    }

    for (i = 0; i < noffspring; i++) {
	m = new NameSpace(this, offspring[i], false, isArchive);

	if (status[i] == PMNS_NONLEAF_STATUS) {
	    m->setSelectable(FALSE);
	    m->setExpandable(TRUE);
	    m->type = NONLEAF_NAME;
	}
	else {
	    // type still UNKNOWN, could be LEAF_NULL_INDOM or LEAF_WITH_INDOM
	    // here we add this NameSpace pointer to the leaf list, and also
	    // modify the offspring list to only contain names (when done).
	    leaflist = (NameSpace **)realloc(leaflist,
					    (nleaf + 1) * sizeof(*leaflist));
	    leaflist[nleaf] = m;
	    offspring[nleaf] = namedup(name, offspring[i]);
	    nleaf++;
	}
    }

    if (nleaf == 0) {
	expanded = TRUE;
	goto done;
    }

    pmidlist = (pmID *)malloc(nleaf * sizeof(*pmidlist));
    if ((sts = pmLookupName(nleaf, offspring, pmidlist)) < 0) {
	QString msg = QString();
	msg.sprintf("Cannot find PMIDs for \"%s\".\n%s.\n\n",
		name, pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
	return;
    }
    else {
	for (i = 0; i < nleaf; i++) {
	    m = leaflist[i];
	    sts = pmLookupDesc(pmidlist[i], &m->desc);
	    if (sts < 0) {
		QString msg = QString();
		msg.sprintf("Cannot find metric descriptor at \"%s\".\n%s.\n\n",
			offspring[i], pmErrStr(sts));
		QMessageBox::warning(NULL, pmProgname, msg,
			QMessageBox::Ok | QMessageBox::Default |
				QMessageBox::Escape,
			QMessageBox::NoButton, QMessageBox::NoButton);
		return;
	    }
	    if (m->desc.indom == PM_INDOM_NULL) {
		m->type = LEAF_NULL_INDOM;
		m->setExpandable(FALSE);
		m->setSelectable(TRUE);
	    }
	    else {
		m->type = LEAF_WITH_INDOM;
		m->setExpandable(TRUE);
		m->setSelectable(FALSE);
	    }
	}
	expanded = TRUE;
    }
    for (i = 0; i < nleaf; i++)
	free(offspring[i]);

done:
    if (pmidlist)
	free(pmidlist);
    if (leaflist)
	free(leaflist);
    if (offspring)
	free(offspring);
    if (status)
	free(status);
}

void NameSpace::expandInstanceNames(void)
{
    int		sts, i;
    int		ninst = 0;
    int		*instlist = NULL;
    char	**namelist = NULL;

    sts = !isArchive ? pmGetInDom(desc.indom, &instlist, &namelist) :
		pmGetInDomArchive(desc.indom, &instlist, &namelist);
    if (sts < 0) {
	QString msg = QString();
	msg.sprintf("Error fetching instance domain at node \"%s\".\n%s.\n\n",
		metricName().ascii(), pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default |
			QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	ninst = sts;
	expanded = TRUE;
    }

    for (i = 0; i < ninst; i++) {
	NameSpace *m = new NameSpace(this, namelist[i], true, isArchive);

	m->setExpandable(FALSE);
	m->setSelectable(TRUE);
	m->instid = instlist[i];
	m->type = INSTANCE_NAME;
    }

    if (instlist)
	free(instlist);
    if (namelist)
	free(namelist);
}

void NameSpace::setup()
{
    QListViewItem::setup();
}

QString NameSpace::text(int column) const
{
    if (column > 1)
	return "";
    return basename;
}

const QPixmap *NameSpace::pixmap(int column) const
{
    if (column > 1)
	return NULL;
    return &pixelmap;
}

void NameSpace::setPixmap(int column, const QPixmap & pm)
{
    if (column > 1)
	return;
    pixelmap = pm;
}

NameSpace *NameSpace::dup(QListView *list)
{
    NameSpace *n;

    n = new NameSpace(list, context, type == ARCHIVE_ROOT);
    n->expanded = TRUE;
    n->setExpandable(TRUE);
    n->setSelectable(FALSE);
    return n;
}

NameSpace *NameSpace::dup(QListView *target, NameSpace *tree)
{
    NameSpace *n;

    n = new NameSpace(tree, basename.ascii(), type == INSTANCE_NAME, isArchive);
    n->expanded = TRUE;
    n->context = context;
    n->instid = instid;
    n->desc = desc;
    n->type = type;
    if (!isLeaf()) {
	n->setExpandable(TRUE);
	n->setSelectable(FALSE);
    }
    else {
	n->setExpandable(FALSE);
	n->setSelectable(TRUE);

	QColor c = Chart::defaultColor(-1);
	n->setOriginalColor(c);
	n->setCurrentColor(c, NULL);

	// this is a leaf so walk back up to the root, opening each node up
	NameSpace *up;
	for (up = tree; up->back != up; up = up->back)
	    up->setOpen(TRUE);
	up->setOpen(TRUE);	// add the host/archive root as well.

	target->setSelected(n, TRUE);
    }
    return n;
}

bool NameSpace::cmp(QListViewItem *item)
{
    if (!item)	// empty list
	return FALSE;
    return (item->text(1) == basename);
}

void NameSpace::addToList(QListView *target)
{
    QListViewItem *last;
    QPtrList<NameSpace> nodelist;
    NameSpace *node, *tree = NULL;

    // Walk through each component of this name, creating them in the
    // target list (if not there already), right down to the leaf.
    // We hold everything in a temporary list since we need to add to
    // the target in root-to-leaf order but we're currently at a leaf.

    for (node = this; node->back != node; node = node->back)
	nodelist.prepend(node);
    nodelist.prepend(node);	// add the host/archive root as well.

    last = target->firstChild();
    for (node = nodelist.first(); node; node = nodelist.next()) {
	do {
	    if (node->cmp(last)) {
		// no insert at this level necessary, move down a level
		if (!node->isLeaf()) {
		    tree = (NameSpace *)last;
		    last = last->firstChild();
		}
		else {	// already there, just select the existing name
		    target->setSelected(last, TRUE);
		}
		break;
	    }
	    // else keep scanning the direct children.
	} while (last && (last = last->nextSibling()) != NULL);

	/* when no more children and no match so far, we dup & insert */
	if (!last) {
	    if (node->isRoot())
		tree = node->dup(target);
	    else if (tree)
		tree = node->dup(target, tree);
	}
    }
}

void NameSpace::removeFromList(QListView *target)
{
    NameSpace *self, *tree, *node = this;

    target->setSelected(this, FALSE);
    do {
	self = node;
	tree = node->back;
	if (node->childCount() == 0)
	    delete node;
	node = tree;
    } while (self != tree);
}

void NameSpace::setCurrentColor(QColor color, QListView *listview)
{
    QPixmap pix(8, 8);

    // Set the current metric color, and (optionally) move on to the next
    // leaf node in the view (if given listview parameter is non-NULL).
    // We're taking a punt here that the user will move down through the
    // metrics just added, and set their prefered color on each one; so,
    // by doing the next selection automatically, we save some clicks.

    current = color;
    pix.fill(color);
    setPixmap(1, pix);

    if (listview) {
	QListViewItemIterator it(this, QListViewItemIterator::Selectable);
	if ((++it).current()) {
	    listview->setSelected(it.current(), TRUE);
	    listview->setSelected(this, FALSE);
	} else
	    repaint();
    } else
	repaint();
}
