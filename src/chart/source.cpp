/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
 */

//
// Source Class
//
// manipulate one or more PMAPI contexts

#include <qtooltip.h>
#include <qmessagebox.h>
#include "namespace.h"
#include "main.h"

#define DESPERATE 1

typedef struct source {
    struct source	*next;
    PMC_Context		*ctx;
    NameSpace		*root;
} source_t;

Source::Source(PMC_Group *group)
{
    currentSource = NULL;
    firstSource = NULL;
    fetchGroup = group;
}

QString Source::makeSourceBaseName(const PMC_Context *cp)
{
    if (cp->source().type() == PM_CONTEXT_ARCHIVE)
	return QString(cp->source().source().ptr());
    return QString(cp->source().host().ptr());
}

QString Source::makeSourceAnnotatedName(const PMC_Context *cp)
{
    QString	t;

    if (cp->source().type() == PM_CONTEXT_HOST) {
	t = QString(cp->source().host().ptr());
	// TODO - pmproxy host support needed here.
	t.append(" (no proxy)");
    }
    else {
	t = QString(cp->source().source().ptr());
	t.append(" (");
	t.append(cp->source().host().ptr());
	t.append(")");
    }
    return t;
}

QString Source::makeComboText(const PMC_Context *ctx)
{
    return makeSourceAnnotatedName(ctx);
}

int useSourceContext(QWidget *parent, QString &source)
{
    int		sts;
    PMC_Group	*group = activeTab->group();
    uint_t	ctxcount = group->numContexts();
    const char	*src = source.ascii();
    char	*end, *proxy, *host = strdup(src);

    // TODO: proxy support needed (we toss proxy hostname atm)
    end = host;
    strsep(&end, " ");
    proxy = strsep(&end, ")") + 1;
    *end = '\0';

    fprintf(stderr, "%s: trying new source: %s; host=%s proxy=%s\n",
	    __FUNCTION__, src, host, proxy);

    if (strcmp(proxy, "no proxy") == 0)
	unsetenv("PMPROXY_HOST");
    else
	setenv("PMPROXY_HOST", proxy, 1);
    if ((sts = group->use(activeSources->type(), src)) < 0) {
	QString msg = QString();
	msg.sprintf("Failed to %s \"%s\".\n%s.\n\n",
		    (activeSources->type() == PM_CONTEXT_HOST) ?
		    "connect to pmcd on" : "open archive", host, pmErrStr(sts));
	QMessageBox::warning(parent, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else if (group->numContexts() > ctxcount)
	activeSources->add(group->which());
    free(host);
    return sts;
}

int Source::useComboContext(QWidget *parent, QComboBox *combo)
{
    QString source = combo->currentText();
    int sts = useSourceContext(parent, source);
    if (sts < 0)
	combo->removeItem(0);
    return sts;
}

int Source::type()
{
    return currentSource == NULL ? -1 : currentSource->ctx->source().type();
}

QString Source::host()
{
    // TODO: nathans - these aint QString's, theyre char* ... hmm.
    return currentSource == NULL ?
		NULL : currentSource->ctx->source().host().ptr();
}

const char *Source::source()
{
#if DESPERATE
    fprintf(stderr, "Source::source(): currentSource=%p", currentSource);
    if (currentSource != NULL)
	fprintf(stderr, " ctx=%d", currentSource->ctx->hndl());
    else
	fputc('\n', stderr);
#endif
    if (currentSource == NULL)
    	return  NULL;
    else {
	PMC_Context	*cp = currentSource->ctx;
#if DESPERATE
	fprintf(stderr, " source=%s\n", cp->source().source().ptr());
#endif
	return cp->source().source().ptr();
    }
}

NameSpace *Source::root(void)
{
    if (currentSource == NULL)
	return NULL;
    else
	return currentSource->root;
}

void Source::setRoot(NameSpace *root)
{
    if (currentSource != NULL)
	currentSource->root = root;
    else {
	fprintf(stderr, "Source::setRoot: botch currentSource is NULL\n");
	dump(stderr);
    }
}

void Source::add(PMC_Context *ctx)
{
    source_t	*sp;
    source_t	*lastsp;
    bool	send_bounds = (ctx->source().type() == PM_CONTEXT_ARCHIVE);

    sp = (source_t *)malloc(sizeof(*sp));	// TODO - error check?
    sp->root = NULL;
    sp->ctx = ctx;
    sp->next = NULL;
    currentSource = sp;
    for (lastsp = NULL, sp = firstSource; sp; sp = sp->next)
	lastsp = sp;
    if (lastsp == NULL)
	firstSource = currentSource;
    else
	lastsp->next = currentSource;
#if DESPERATE
    dump(stderr);
#endif

    // For archives, send a message to kmtime to grow its time window.
    // This is already done if we're the first, so don't do it again;
    // we also don't have a kmtime connection yet if processing args.
    
    if (kmtime && send_bounds) {
	const PMC_Source *source = &ctx->source();
	PMC_String tz = source->timezone();
	PMC_String host = source->host();
	struct timeval logStartTime = source->start();
	struct timeval logEndTime = source->end();
	kmtime->addArchive(&logStartTime, &logEndTime, tz.ptr(),
				tz.length(), host.ptr(), host.length());
    }
}

void Source::dump(FILE *f)
{
    int		i;
    source_t	*sp;

    fprintf(f, "Source::dump: current=%p\n", currentSource);
    for (i = 0, sp = firstSource; sp; i++, sp = sp->next) {
	fprintf(f, "fetchGroup=%p\n", fetchGroup);
	fprintf(f, "whichIndex=%d\n", fetchGroup->whichIndex());
	fprintf(f, "numContexts=%d\n", fetchGroup->numContexts());
	fprintf(f, "ctxp=%p\n", sp->ctx);
	fprintf(f, "[%d]%p: type=%d source=%s host=%s ctx=%d root=%p\n",
		i, sp, sp->ctx->source().type(),
		sp->ctx->source().source().ptr(),
		sp->ctx->source().host().ptr(), sp->ctx->hndl(), sp->root);
    }
}

void Source::setupCombo(QComboBox *combo)
{
    int		i;
    source_t	*sp;
    QString	src;

    combo->clear();
    for (i = 0, sp = firstSource; sp; i++, sp = sp->next) {
	src = makeComboText(sp->ctx);
	combo->insertItem(src);
	if (sp == currentSource)
	    combo->setCurrentItem(i);
    }
}

#if 1	// TODO: nuke:
void Source::setCurrentInCombo(QComboBox *combo)
{
    QString cn;

    if (currentSource == NULL) {
	fprintf(stderr, "Source::setCurrentCombo: botch currentSource is NULL\n");
	dump(stderr);
	return;
    }

    cn = makeComboText(currentSource->ctx);
    for (int i = 0; i < combo->count(); i++) {
	if (combo->text(i) == cn) {
	    combo->setCurrentItem(i);
	    return;
	}
    }

    fprintf(stderr, "Source::setCurrentInCombo: botch failed to find name=%s\n", cn.ascii());
    dump(stderr);
    fprintf(stderr, "Combo list ...\n");
    for (int i = 0; i < combo->count(); i++) {
	fprintf(stderr, "[%d] %s", i, combo->text(i).ascii());
	if (combo->currentItem() == i)
	    fprintf(stderr, " [current]");
	fputc('\n', stderr);
    }
}

// only called from a QComboBox widget, where the text() has been
// constructed by Source::setupCombo
//
void Source::setCurrentFromCombo(const QString name)
{
    source_t	*sp;

    fprintf(stderr, "Source::setCurrentFromCombo(%s) called\n", name.ascii());
    for (sp = firstSource; sp; sp = sp->next) {
	if (name == makeComboText(sp->ctx)) {
	    currentSource = sp;
	    return;
	}
    }
    fprintf(stderr, "Source::setCurrentSource: botch failed to find name=%s\n", name.ascii());
    dump(stderr);
}
#endif

void Source::setupListView(QListView *listview)
{
    source_t	*source;
    QString	string;

    listview->clear();
    for (source = firstSource; source; source = source->next) {
	NameSpace *name = new NameSpace(listview, source->ctx,
					activeTab->isArchiveMode());
	name->setOpen(TRUE);
	name->setSelectable(FALSE);
	if (source == currentSource)
	    listview->setCurrentItem(name);
    }
}

ArchiveDialog::ArchiveDialog(QWidget *parent)
    : QFileDialog(parent, "archiveDialog", true)
{
    logButton = new QToolButton(this, "logButton");
    logButton->setPixmap(QPixmap::fromMimeSource("filearchive.png"));
    logButton->setToggleButton(FALSE);
    logButton->setFixedSize(20, 20);
    QToolTip::add(logButton, tr("System Archives"));
    connect(logButton, SIGNAL(clicked()), this, SLOT(logDirClicked()));
    addToolButton(logButton, TRUE);
    setMode(QFileDialog::ExistingFiles);
}

ArchiveDialog::~ArchiveDialog()
{
    delete logButton;
}

void ArchiveDialog::logDirClicked()
{
    QString dir = tr(pmGetConfig("PCP_LOG_DIR"));
    dir.append("/pmlogger");
    setDir(dir);
}
