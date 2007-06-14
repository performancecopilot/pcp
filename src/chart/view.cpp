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

#define PCP_DEBUG 1

#include "main.h"
#include "hostdialog.h"
#include <qcolor.h>
#include <qlabel.h>
#include <qstring.h>
#include <qlistbox.h>
#include <qtooltip.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <regex.h>

#define DESPERATE 0

FileIconProvider::FileIconProvider(QObject *parent, const char *name)
    : QFileIconProvider(parent, name)
{
    // kmchart/PCP-specific images
    fileView = QPixmap::fromMimeSource("fileview.png");
    fileFolio = QPixmap::fromMimeSource("filefolio.png");
    fileArchive = QPixmap::fromMimeSource("filearchive.png");

    // images for several other common file types
    fileHtml = QPixmap::fromMimeSource("filehtml.png");
    fileImage = QPixmap::fromMimeSource("fileimage.png");
    fileGeneric = QPixmap::fromMimeSource("filegeneric.png");
    filePackage = QPixmap::fromMimeSource("filepackage.png");
    fileSpreadSheet = QPixmap::fromMimeSource("filespreadsheet.png");
    fileWordProcessor = QPixmap::fromMimeSource("filewordprocessor.png");
}

const QPixmap *FileIconProvider::pixmap(const QFileInfo &fi)
{
#if DESPERATE
    fprintf(stderr, "%s: file %s\n", __FUNCTION__, fi.filePath().ascii());
#endif

    if (fi.isFile()) {
	QFile file(fi.filePath());
	file.open(IO_ReadOnly);
	char block[9];
	int count = file.readBlock(block, sizeof(block)-1);
	if (count == sizeof(block)-1) {
	    static char viewmagic[] = "#kmchart";
	    static char foliomagic[] = "PCPFolio";
	    static char archmagic[] = "\0\0\0\204\120\5\46\2"; //PM_LOG_MAGIC|V2

	    if (memcmp(viewmagic, block, sizeof(block)-1) == 0)
		return &fileView;
	    if (memcmp(foliomagic, block, sizeof(block)-1) == 0)
		return &fileFolio;
	    if (memcmp(archmagic, block, sizeof(block)-1) == 0)
		return &fileArchive;
	}
#if DESPERATE
	fprintf(stderr, "%s: Got %d bytes from %s: \"%c%c%c%c%c%c%c%c\"\n",
		__FUNCTION__, count, fi.filePath().ascii(), block[0], block[1],
		block[2], block[3], block[4], block[5], block[6], block[7]);
#endif
	QString ext = fi.extension();
	if (ext == "htm" || ext == "html")
	    return &fileHtml;
	if (ext == "png" || ext == "gif" || ext == "jpg" || ext == "jpeg" ||
	    ext == "xpm" || ext == "odg" /* ... */ )
	    return &fileImage;
	if (ext == "tar" || ext == "tgz" || ext == "deb" || ext == "rpm" ||
	    ext == "zip" || ext == "bz2" || ext == "gz")
	    return &filePackage;
	if (ext == "ods" || ext == "xls")
	    return &fileSpreadSheet;
	if (ext == "odp" || ext == "doc")
	    return &fileWordProcessor;
	return &fileGeneric;	// catch-all for every other regular file
    }
    return QFileIconProvider::pixmap(fi);
}

OpenViewDialog::OpenViewDialog(QWidget *parent)
    : QFileDialog(parent, "openViewDialog", false)
{
    setMode(QFileDialog::ExistingFiles);

    usrDir = QDir::homeDirPath();
    usrDir.append("/.pcp/kmchart");
    sysDir = tr(pmGetConfig("PCP_VAR_DIR"));
    sysDir.append("/config/kmchart");

    usrButton = new QToolButton(this, "usrButton");
    sysButton = new QToolButton(this, "sysButton");
    usrButton->setPixmap(QPixmap::fromMimeSource("fileusers.png"));
    sysButton->setPixmap(QPixmap::fromMimeSource("fileview.png"));
    usrButton->setToggleButton(TRUE);
    sysButton->setToggleButton(TRUE);
    QToolTip::add(usrButton, tr("User Views"));
    QToolTip::add(sysButton, tr("System Views"));
    connect(usrButton, SIGNAL(clicked()), this, SLOT(usrDirClicked()));
    connect(sysButton, SIGNAL(clicked()), this, SLOT(sysDirClicked()));
    connect(usrButton, SIGNAL(toggled(bool)), this, SLOT(usrDirToggled(bool)));
    connect(sysButton, SIGNAL(toggled(bool)), this, SLOT(sysDirToggled(bool)));
    addToolButton(usrButton, FALSE);
    addToolButton(sysButton, FALSE);

    srcLabel = new QLabel(this, "srcLabel");
    srcLabel->setFixedWidth(75);
    srcButton = new QPushButton(this, "srcButton");
    srcButton->setFixedSize(80, 32);
    connect(srcButton, SIGNAL(clicked()), this, SLOT(sourceAdd()));
    srcCombo = new QComboBox(FALSE, this, "srcCombo");
    srcCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed, FALSE);
    connect(srcCombo, SIGNAL(activated(int)), this, SLOT(sourceChange(int)));

    connect(this, SIGNAL(filesSelected(const QStringList&)), this,
		  SLOT(openViewFiles(const QStringList&)));
    connect(this, SIGNAL(dirEntered(const QString&)), this,
		  SLOT(viewDirEntered(const QString&)));

    addWidgets(srcLabel, srcCombo, srcButton);
}

void OpenViewDialog::reset()
{
    if ((archiveMode = activeTab->isArchiveMode())) {
	srcLabel->setText(tr("Archive:"));
	srcButton->setPixmap(QPixmap::fromMimeSource("archive.png"));
    } else {
	srcLabel->setText(tr("Host:"));
	srcButton->setPixmap(QPixmap::fromMimeSource("computer.png"));
    }
    activeSources->setupCombo(srcCombo);
    setDir(sysDir);
}

void OpenViewDialog::usrDirClicked()
{
   if (dirPath() == usrDir) {
	usrButton->setOn(TRUE);
	sysButton->setOn(FALSE);
    }
}

void OpenViewDialog::sysDirClicked()
{
   if (dirPath() == sysDir) {
	sysButton->setOn(TRUE);
	usrButton->setOn(FALSE);
   }
}

void OpenViewDialog::usrDirToggled(bool on)
{
    if (!on)
	return;
    QDir dir;
    if (!dir.exists(usrDir))
	dir.mkdir(usrDir);
    setDir(usrDir);
}

void OpenViewDialog::sysDirToggled(bool on)
{
    if (on)
	setDir(sysDir);
}

void OpenViewDialog::viewDirEntered(const QString &dir)
{
    usrButton->setOn(dir == usrDir);
    sysButton->setOn(dir == sysDir);
}

void OpenViewDialog::sourceAdd()
{
    int	sts;

    if (activeTab->isArchiveMode()) {
	ArchiveDialog *a = new ArchiveDialog(this);
	QStringList al;

	if (a->exec() == QDialog::Accepted)
	    al = a->selectedFiles();
	for (QStringList::Iterator it = al.begin(); it != al.end(); ++it) {
	    QString ar = (*it).ascii();
	    if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE, ar.ascii())) < 0) {
		ar.prepend(tr("Cannot open PCP archive: "));
		ar.append(tr("\n"));
		ar.append(tr(pmErrStr(sts)));
		QMessageBox::warning(this, pmProgname, ar,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    QMessageBox::NoButton, QMessageBox::NoButton);
	    } else {
		archiveSources->add(archiveGroup->which());
		archiveSources->setupCombo(srcCombo);
		archiveGroup->updateBounds();
	    }
	}
	delete a;
    } else {
	HostDialog *h = new HostDialog(this);

	if (h->exec() == QDialog::Accepted) {
	    QString proxy = h->proxyLineEdit->text().stripWhiteSpace();
	    if (proxy.isEmpty())
		unsetenv("PMPROXY_HOST");
	    else
		setenv("PMPROXY_HOST", proxy.ascii(), 1);
	    QString host = h->hostLineEdit->text().stripWhiteSpace();
	    if ((sts = liveGroup->use(PM_CONTEXT_HOST, host.ascii())) < 0) {
		host.prepend(tr("Cannot connect to host: "));
		host.append(tr("\n"));
		host.append(tr(pmErrStr(sts)));
		QMessageBox::warning(this, pmProgname, host,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    QMessageBox::NoButton, QMessageBox::NoButton);
	    } else {
		liveSources->add(liveGroup->which());
		liveSources->setupCombo(srcCombo);
	    }
	}
	delete h;
    }
}

void OpenViewDialog::sourceChange(int idx)
{
    (void)idx;	// using currentText()
    Source::useComboContext(this, srcCombo);
}

void OpenViewDialog::openViewFiles(const QStringList &fl)
{
    if (activeTab->isArchiveMode() != archiveMode) {
	QString msg;

	if (activeTab->isArchiveMode())
	    msg = tr("Cannot open Host View(s) in an Archive Tab\n");
	else
	    msg = tr("Cannot open Archive View(s) in a Host Tab\n");
	QMessageBox::warning(this, pmProgname, msg,
	    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
	    QMessageBox::NoButton, QMessageBox::NoButton);
    } else {
	QStringList files = fl;
	for (QStringList::Iterator it = files.begin(); it != files.end(); ++it)
	    openView((*it).ascii());
	kmchart->enableUI();
    }
}

SaveViewDialog::SaveViewDialog(QWidget *parent)
    : QFileDialog(parent, "saveViewDialog", false)
{
    setMode(QFileDialog::AnyFile);
    usrDir = QDir::homeDirPath();
    usrDir.append("/.pcp/kmchart");
    connect(this, SIGNAL(fileSelected(const QString&)), this,
		  SLOT(saveViewFile(const QString&)));
}

void SaveViewDialog::reset()
{
    QDir d;
    if (!d.exists(usrDir))
	d.mkdir(usrDir);
    setDir(usrDir);
    hostDynamic = TRUE;
}

void SaveViewDialog::saveViewFile(const QString &filename)
{
    saveView(filename.ascii(), hostDynamic);
}

RecordViewDialog::RecordViewDialog(QWidget *parent)
    : QFileDialog(parent, "recordViewDialog", false)
{
    setMode(QFileDialog::AnyFile);
}

void RecordViewDialog::setFileName(QString path)
{
    setSelection(path);
}


/*
 * View file parsing routines and global variables follow.  These are
 * currently not part of any class, and may need to be reworked a bit
 * to use more portable (Qt) file IO interfaces.
 */
static char	_fname[MAXPATHLEN];
static int	_line;
static int	_errors;

#define MAXWDSZ 256

// parser states
#define S_BEGIN		0
#define S_VERSION	1
#define S_TOP		2
#define S_CHART		3
#define S_PLOT		4

// config file styles (mode)
#define M_UNKNOWN	0
#define M_PMCHART	1
#define M_KMCHART	2

// host mode
#define H_DYNAMIC	1
#define H_LITERAL	2

// error severity
#define E_INFO	0
#define E_CRIT	1
#define E_WARN	2

// version numbers we're willing to support or at least acknowledge
#define P1_1	101
#define P1_2	102
#define P2_0	200
#define P2_1	201
#define K1	1

// instance / matching / not-matching
#define IM_NONE		0
#define IM_INST		1
#define IM_MATCH	2
#define IM_NOT_MATCH	3

#ifdef PCP_DEBUG
char *_style[] = { "None", "Line", "Bar", "Stack", "Area", "Util" };
#define stylestr(x) _style[(int)x]
#endif

static void
err(int severity, int do_where, QString msg)
{
    if (do_where) {
	QString	where = QString();
	where.sprintf("%s[%d] ", _fname, _line);
	msg.prepend(where);
    }
    if (Cflag) {
	if (severity == E_CRIT)
	    msg.prepend("Error: ");
	else if (severity == E_WARN)
	    msg.prepend("Warning: ");
	else
	    // do nothing for E_INFO
	    ;
	msg.append("\n");
	fflush(stderr);
	pmprintf(msg.ascii());
	pmflush();
    }
    else {
	if (severity == E_CRIT)
	    QMessageBox::critical(kmchart, pmProgname,  msg);
	else if (severity == E_WARN)
	    QMessageBox::warning(kmchart, pmProgname,  msg);
	else
	    QMessageBox::information(kmchart, pmProgname,  msg);
    }
    _errors++;
}

static void
nomem(void)
{
    // no point trying to report anything ... dump core is the best bet
    abort();
}

static char *
getwd(FILE *f)
{
    static char	buf[MAXWDSZ];
    static int	lastc = 0;
    char	*p;
    int		c;
    int		quote = 0;

    if ((char)lastc == '\n') {
eol:
	buf[0] = '\n';
	buf[1] = '\0';
	_line++;
	lastc = 0;
	goto done;
    }

    // skip to first non-white space
    p = buf;
    while ((c = fgetc(f)) != EOF) {
	if ((char)c == '\n')
	    goto eol;
	if (!isspace((char)c)) {
	    // got one
	    if ((char)c == '"') {
		quote = 1;
	    }
	    else {
		*p++ = c;
	    }
	    break;
	}
    }
    if (feof(f)) return NULL;

    for ( ; p < &buf[MAXWDSZ]; ) {
	if ((c = fgetc(f)) == EOF) break;
	if ((char)c == '\n') {
	    lastc = c;
	    break;
	}
	if (quote == 0 && isspace((char)c)) break;
	if (quote && (char)c == '"') break;
	*p++ = c;
    }

    if (p == &buf[MAXWDSZ]) {
	QString	msg = QString();
	p[-1] = '\0';
	msg.sprintf("Word truncated after %d characters!\n\"%20.20s ... %20.20s\"", (int)sizeof(buf)-1, buf, &p[-21]);
	err(E_CRIT, TRUE, msg);
    }
    else
	*p = '\0';


done:
#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_APPL0) && (pmDebug & DBG_TRACE_APPL2)) {
	if (buf[0] == '\n')
	    fprintf(stderr, "loadView getwd=EOL\n");
	else
	    fprintf(stderr, "loadView getwd=\"%s\"\n", buf);
    }
#endif

    return buf;
}

static QColor *
rgbi2qcolor(char *str)
{
    QColor	*c = new QColor(QString("white"));
    float	fr, fg, fb;
    int		sts;

    if ((sts = sscanf(str, "rgbi:%f/%f/%f", &fr, &fg, &fb)) == 3) {
#define hexval(f) ((int)(0.5 + f*256) < 256 ? (int)(0.5 + f*256) : 256)
	c->setRgb(hexval(fr), hexval(fg), hexval(fb));
    }
    else {
	QString	msg;
	msg.sprintf("rgbi2qcolor: botch scanf->%d not 3 from \"%s\"\n", sts, str);
	err(E_CRIT, TRUE, msg);
	// fallthrough to return "white"
    }

    return c;
}

static void
eol(FILE *f)
{
    char	*w;

    while ((w = getwd(f)) != NULL && w[0] != '\n') {
	QString	msg = QString("Syntax error: unexpected word \"");
	msg.append(w);
	msg.append("\"");
	err(E_CRIT, TRUE, msg);
    }
}

static void
skip2eol(FILE *f)
{
    char	*w;

    while ((w = getwd(f)) != NULL && w[0] != '\n') {
	;
    }
}

static void
xpect(char *want, char *got)
{
    QString     msg = QString("Syntax error: expecting \"");
    msg.append(want);
    msg.append("\", found ");
    if (got == NULL)
	msg.append("End-of-File");
    else if (got[0] == '\n')
	msg.append("End-of-Line");
    else {
	msg.append("\"");
	msg.append(got);
	msg.append("\"");
    }
    err(E_CRIT, TRUE, msg);
}

void OpenViewDialog::openView(const char *path)
{
    Chart		*cp = NULL;
    pmMetricSpec	pms;
    int			m;
    QColor		*c;
    FILE		*f;
    int			is_popen = 0;
    char		*w;
    int			state = S_BEGIN;
    int			mode = M_UNKNOWN;
    int			h_mode;
    int			version;
    QString		errmsg = QString();
    int			sts = 0;	// pander to g++

    if (strcmp(path, "-") == 0) {
	// standard input
	f = stdin;
	strcpy(_fname, "stdin");
    }
    else {
	strcpy(_fname, path);
	if ((f = fopen(_fname, "r")) == NULL) {
	    // not found, start the great hunt
	    // try user's kmchart dir ...
	    strcpy(_fname, getenv("HOME"));
	    strcat(_fname, "/.pcp/kmchart/");
	    strcat(_fname, path);
	    if ((f = fopen(_fname, "r")) == NULL) {
		// try system kmchart dir
		strcpy(_fname, pmGetConfig("PCP_VAR_DIR"));
		strcat(_fname, "/config/kmchart/");
		strcat(_fname, path);
		if ((f = fopen(_fname, "r")) == NULL) {
		    // try user's pmchart dir
		    strcpy(_fname, getenv("HOME"));
		    strcat(_fname, "/.pcp/pmchart/");
		    strcat(_fname, path);
		    if ((f = fopen(_fname, "r")) == NULL) {
			// try system pmchart dir
			strcpy(_fname, pmGetConfig("PCP_VAR_DIR"));
			strcat(_fname, "/config/pmchart/");
			strcat(_fname, path);
			if ((f = fopen(_fname, "r")) == NULL) {
			    QString	msg = QString("Cannot open view file \"");
			    msg.append(_fname);
			    msg.append("\"\n");
			    msg.append(strerror(errno));
			    err(E_CRIT, FALSE, msg);
			    return;
			}
		    }
		}
	    }
	}
	// check for executable and popen() as needed
	//
	if (fgetc(f) == '#' && fgetc(f) == '!') {
	    char	cmd[MAXPATHLEN];
	    sprintf(cmd, "%s", _fname);
	    fclose(f);
	    if ((f = popen(cmd, "r")) == NULL) {
		QString	msg;
		msg.sprintf("Cannot execute \"%s\"\n%s", _fname, strerror(errno));
		err(E_CRIT, FALSE, msg);
		return;
	    }
	    is_popen = 1;
	}
	else {
	    rewind(f);
	}
    }

    _line = 1;
    _errors = 0;
    fprintf(stderr, "Load View: %s\n", _fname);

    while ((w = getwd(f)) != NULL) {
	if (state == S_BEGIN) {
	    // expect #pmchart
	    if (strcasecmp(w, "#pmchart") == 0)
		mode = M_PMCHART;
	    else if (strcasecmp(w, "#kmchart") == 0)
		mode = M_KMCHART;
	    else {
		xpect("#pmchart\" or \"#kmchart", w);
		goto abandon;
	    }
	    eol(f);
	    state = S_VERSION;
	    continue;
	}

	if (w[0] == '\n')
	    // skip empty lines and comments
	    continue;

	if (w[0] == '#') {
	    // and comments
	    skip2eol(f);
	    continue;
	}

	if (state == S_VERSION) {
	    // expect version X.X host [dynamic|static]
	    if (strcasecmp(w, "version") != 0) {
		xpect("version", w);
		goto abandon;
	    }
	    w = getwd(f);
	    if (w == NULL || w[0] == '\n') {
		xpect("<version number>", w);
		goto abandon;
	    }
	    version = 0;
	    if (mode == M_PMCHART) {
		if (strcmp(w, "2.1") == 0)
		    version = P2_1;
		else if (strcmp(w, "2.0") == 0)
		    version = P2_0;
		else if (strcmp(w, "1.1") == 0)
		    version = P1_1;
		else if (strcmp(w, "1.2") == 0)
		    version = P1_2;
	    }
	    else if (mode == M_KMCHART) {
		if (strcmp(w, "1") == 0)
		    version = K1;
	    }
	    if (version == 0) {
		xpect("<version number>", w);
		goto abandon;
	    }
	    w = getwd(f);
	    if (w == NULL || w[0] == '\n') {
		if (mode == M_KMCHART) {
		    // host [literal|dynamic] is optional for kmchart
		    h_mode = H_DYNAMIC;
		    state = S_TOP;
		    continue;
		}
		else {
		    xpect("host", w);
		    goto abandon;
		}
	    }
	    if (strcasecmp(w, "host") != 0) {
		xpect("host", w);
		goto abandon;
	    }
	    w = getwd(f);
	    if (w != NULL && strcasecmp(w, "literal") == 0) {
		h_mode = H_LITERAL;
	    }
	    else if (w != NULL && strcasecmp(w, "dynamic") == 0) {
		h_mode = H_DYNAMIC;
	    }
	    else {
		xpect("literal\" or \"dynamic", w);
		goto abandon;
	    }
	    eol(f);
	    state = S_TOP;
	}

	else if (state == S_TOP) {
new_chart:
	    if (strcasecmp(w, "chart") == 0) {
		char		*title = NULL;
		chartStyle	style = None;	// pander to g++
		int		autoscale = 1;
		char		*endnum;
		double		ymin = 0;	// pander to g++
		double		ymax = 0;	// pander to g++
		int		legend = 1;

		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("title\" or \"style", w);
		    goto abort_chart;
		}
		if (strcasecmp(w, "title") == 0) {
		    // optional title "<title>"
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("<title>", w);
			goto abort_chart;
		    }
		    if ((title = strdup(w)) == NULL) nomem();
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("style", w);
			goto abort_chart;
		    }
		}
		if (strcasecmp(w, "style") == 0) {
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("<chart style>", w);
			goto abort_chart;
		    }
		    if (strcasecmp(w, "plot") == 0)
			style = Line;
		    else if (strcasecmp(w, "bar") == 0)
			style = Bar;
		    else if (strcasecmp(w, "stacking") == 0)
			style = Stack;
		    else if (strcasecmp(w, "area") == 0)
			style = Area;
		    else if (strcasecmp(w, "utilization") == 0)
			style = Util;
		    else {
			xpect("<chart style>", w);
			goto abort_chart;
		    }
		}

		// down to the optional bits
		//	- scale
		//	- legend
		if ((w = getwd(f)) == NULL || w[0] == '\n')
		    goto done_chart;
		if (strcasecmp(w, "scale") == 0) {
		    // scale [from] ymin [to] ymax
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("from or <ymin>", w);
			goto abort_chart;
		    }
		    if (strcasecmp(w, "from") == 0) {
			if ((w = getwd(f)) == NULL || w[0] == '\n') {
			    xpect("<ymin>", w);
			    goto abort_chart;
			}
		    }
		    ymin = (int)strtod(w, &endnum);
		    if (*endnum != '\0') {
			xpect("<ymin>", w);
			goto abort_chart;
		    }
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("to or <ymax>", w);
			goto abort_chart;
		    }
		    if (strcasecmp(w, "to") == 0) {
			if ((w = getwd(f)) == NULL || w[0] == '\n') {
			    xpect("<ymax>", w);
			    goto abort_chart;
			}
		    }
		    ymax = (int)strtod(w, &endnum);
		    if (*endnum != '\0') {
			xpect("<ymax>", w);
			goto abort_chart;
		    }
		    autoscale = 0;
		    if ((w = getwd(f)) == NULL || w[0] == '\n')
			goto done_chart;
		}
		if (strcasecmp(w, "legend") == 0) {
		    // optional legend on|off
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("on\" or \"off", w);
			goto abort_chart;
		    }
		    if (strcasecmp(w, "on") == 0)
			legend = 1;
		    else if (strcasecmp(w, "off") == 0)
			legend = 0;
		    else {
			xpect("on\" or \"off", w);
			goto abort_chart;
		    }
		    if ((w = getwd(f)) == NULL || w[0] == '\n')
			goto done_chart;
		}
done_chart:
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL2) {
		    fprintf(stderr, "loadView: new chart: style=%s", stylestr(style));
		    if (title != NULL)
			fprintf(stderr, " title=\"%s\"", title);
		    if (autoscale)
			fprintf(stderr, " autoscale=yes");
		    else
			fprintf(stderr, " ymin=%.1f ymax=%.1f", ymin, ymax);
		    if (legend)
			fprintf(stderr, " legend=yes");
		    fputc('\n', stderr);
		}
#endif
		if (Cflag == 0) {
		    cp = activeTab->addChart();
		    cp->setStyle(style);
		    if (title != NULL)
			cp->changeTitle(title, mode == M_KMCHART);
		    if (legend == 0)
			cp->setLegendVisible(FALSE);
		}
		state = S_CHART;
		if (title != NULL) free(title);
		continue;

abort_chart:
		// unrecoverable error in the chart clause of the view
		// specification, abandon this one
		if (title != NULL) free(title);
		goto abandon;

	    }
	    else if (strcasecmp(w, "global") == 0) {
		// TODO -- global is an alternative clause to Chart at
		// this point in the config file ... no support for global
		// options to set window height, window width, number of
		// visible points, ...
		errmsg.append(QString("global clause not supported yet"));
		skip2eol(f);
	    }
	    else {
		xpect("chart\" or \"global", w);
		goto abandon;
	    }
	}

	else if (state == S_CHART) {
	    int		optional;
	    char	*legend = NULL;
	    char	*color = NULL;
	    char	*host = NULL;
	    int		inst_match_type = IM_NONE;
	    int		numinst = -1;
	    int		nextinst = -1;
	    int		*instlist = NULL;
	    char	**namelist = NULL;
	    regex_t	preg;
	    int		done_regex = 0;
	    int		abort = 1;	// default @ skip

	    if (strcasecmp(w, "chart") == 0) {
		// new chart
		state = S_TOP;
		goto new_chart;
	    }
	    if (strcasecmp(w, "plot") == 0) {
		optional = 0;
	    }
	    else if (strcasecmp(w, "optional-plot") == 0) {
		optional = 1;
	    }
	    else {
		xpect("plot\" or \"optional-plot", w);
		goto skip;
	    }
	    if ((w = getwd(f)) == NULL || w[0] == '\n') {
		xpect("title\" or \"color", w);
		goto skip;
	    }
	    if (strcasecmp(w, "title") == 0 ||
	        (mode == M_KMCHART && strcasecmp(w, "legend") == 0)) {
		// optional title "<title>" or
		// (for kmchart) legend "<title>"
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("<legend title>", w);
		    goto skip;
		}
		if ((legend = strdup(w)) == NULL) nomem();
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("color", w);
		    goto skip;
		}
	    }
	    // color <color> is mandatory for pmchart, optional for
	    // kmchart (where the default is color #-cycle)
	    if (strcasecmp(w, "color") == 0 || strcasecmp(w, "colour") == 0) {
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("<color>", w);
		    goto skip;
		}
		if ((color = strdup(w)) == NULL) nomem();
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("host", w);
		    goto skip;
		}
	    }
	    else if (mode == M_PMCHART) {
		xpect("color", w);
		goto skip;
	    }
	    // host <host> is mandatory for pmchart, optional for
	    // kmchart (where the default is host *)
	    if (strcasecmp(w, "host") == 0) {
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("<host>", w);
		    goto skip;
		}
		if (strcmp(w, "*") == 0)
		    host = NULL;	// just like the kmchart default
		else {
		    if ((host = strdup(w)) == NULL) nomem();
		}
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("metric", w);
		    goto skip;
		}
	    }
	    else if (mode == M_PMCHART) {
		xpect("host", w);
		goto skip;
	    }
	    // metric is mandatory
	    if (strcasecmp(w, "metric") == 0) {
		if ((w = getwd(f)) == NULL || w[0] == '\n') {
		    xpect("<metric>", w);
		    goto skip;
		}
		if ((pms.metric = strdup(w)) == NULL) nomem();
	    }
	    else {
		xpect("metric", w);
		goto skip;
	    }
	    pms.ninst = 0;
	    pms.inst[0] = NULL;
	    if ((w = getwd(f)) != NULL && w[0] != '\n') {
		// optional parts
		//	instance ...
		//	matching ...
		//	not-matching ...
		if (strcasecmp(w, "instance") == 0) {
		    inst_match_type = IM_INST;
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("<instance>", w);
			goto skip;
		    }
		    pms.ninst = 1;
		    if ((pms.inst[0] = strdup(w)) == NULL) nomem();
		}
		else if (strcasecmp(w, "matching") == 0) {
		    inst_match_type = IM_MATCH;
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("<pattern>", w);
			goto skip;
		    }
		    pms.ninst = 1;
		    pms.inst[0] = strdup(w);
		}
		else if (strcasecmp(w, "not-matching") == 0) {
		    inst_match_type = IM_NOT_MATCH;
		    if ((w = getwd(f)) == NULL || w[0] == '\n') {
			xpect("<pattern>", w);
			goto skip;
		    }
		    pms.ninst = 1;
		    pms.inst[0] = strdup(w);
		}
		else {
		    xpect("instance\" or \"matching\" or \"not-matching", w);
		    goto skip;
		}
		if (mode == M_PMCHART) {
		    // pmchart has this lame "instance extends to end
		    // of line" syntax ... sigh
		    while ((w = getwd(f)) != NULL && w[0] != '\n') {
			pms.inst[0] = (char *)realloc(pms.inst[0], strlen(pms.inst[0]) + strlen(w) + 2);
			if (pms.inst[0] == NULL) nomem();
			// if more than one space in the input, touch luck!
			strcat(pms.inst[0], " ");
			strcat(pms.inst[0], w);
		    }
		    if (pms.inst[0] != NULL) {
			pms.ninst = 1;
		    }
		}
		else {
		    // expect end of line after instance/pattern
		    // (kmchart uses quotes to make instance a single
		    // lexical element in the line)
		    eol(f);
		}
	    }

	    abort = 0;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "loadView: new %s", optional ? "optional-plot" : "plot");
		if (legend != NULL) fprintf(stderr, " legend=\"%s\"", legend);
		if (color != NULL) fprintf(stderr, " color=%s", color);
		if (host != NULL) fprintf(stderr, " host=%s", host);
		fprintf(stderr, " metric=%s", pms.metric);
		if (pms.ninst == 1) {
		    fprintf(stderr, " inst=%s", pms.inst[0]);
		    if (inst_match_type == IM_NONE)
			fprintf(stderr, " match=none (botch?)");
		    else if (inst_match_type == IM_INST)
			fprintf(stderr, " match=instance");
		    else if (inst_match_type == IM_MATCH)
			fprintf(stderr, " match=matching");
		    else if (inst_match_type == IM_NOT_MATCH)
			fprintf(stderr, " match=not-matching");
		}
		fputc('\n', stderr);
	    }
#endif
	    if (Cflag == 0) {
		pms.isarch = activeTab->isArchiveMode();
		if (host != NULL) {
		    // TODO -- literal host ... anything more to be done?
		    pms.source = host;
		    // TODO -- need to get host into Sources if not already
		    // there
		}
		else {
		    // no explicit host, use current default source
		    pms.source = (char *) activeSources->source();
		}
		// expand instances when not specified for metrics
		// with instance domains and all instances required,
		// or matching or not-matching instances required
		//
		if (inst_match_type != IM_INST) {
		    pmID	pmid;
		    pmDesc	desc;

		    // if pmLookupName() or pmLookupDesc() fail, we'll
		    // notice in addPlot() and report the error below,
		    // so no need to do anything special here
		    //
		    if (pmLookupName(1, &pms.metric, &pmid) < 0)
			goto try_plot;
		    if (pmLookupDesc(pmid, &desc) < 0)
			goto try_plot;
		    if (desc.indom == PM_INDOM_NULL) {
			if (inst_match_type == IM_MATCH ||
			    inst_match_type == IM_NOT_MATCH) {
			    // a bit embarrassing
			    QString	msg = QString();
			    msg.sprintf("\nMetric \"%s\" for\n%s %s: no instance domain, cannot handle matching specification",
				pms.metric, pms.isarch ? "archive" : "host",
				pms.source);
			    errmsg.append(msg);
			    goto skip;
			}
			goto try_plot;
		    }

		    if (pms.isarch)
			numinst = pmGetInDomArchive(desc.indom, &instlist, &namelist);
		    else
			numinst = pmGetInDom(desc.indom, &instlist, &namelist);
		    if (numinst < 1) {
			QString	msg = QString();
			msg.sprintf("\nMetric \"%s\" for\n%s %s: empty instance domain",
			    pms.metric, pms.isarch ? "archive" : "host",
			    pms.source);
			errmsg.append(msg);
			goto skip;
		    }
		    if (inst_match_type != IM_NONE) {
			sts = regcomp(&preg, pms.inst[0], REG_EXTENDED|REG_NOSUB);
			if (sts != 0) {
			    QString	msg = QString();
			    char	errbuf[1024];
			    regerror(sts, &preg, errbuf, sizeof(errbuf));
			    msg.sprintf("\nBad regular expression \"%s\"\n%s",
				pms.inst[0], errbuf);
			    errmsg.append(msg);
			    goto skip;
			}
			done_regex = 1;
		    }
		    pms.ninst = 1;
		    if (pms.inst[0] != NULL) {
			free(pms.inst[0]);
			pms.inst[0] = NULL;
		    }
		}

try_plot:
		if (numinst > 0) {
		    pms.inst[0] = NULL;
		    for (nextinst++ ; nextinst < numinst; nextinst++) {
			if (inst_match_type == IM_MATCH ||
			    inst_match_type == IM_NOT_MATCH) {
			    sts = regexec(&preg, namelist[nextinst], 0, NULL, 0);
			    if (sts != 0 && sts != REG_NOMATCH) {
				QString	msg = QString();
				char	errbuf[1024];
				regerror(sts, &preg, errbuf, sizeof(errbuf));
				msg.sprintf("\nRegular expression \"%s\" execution botch\n%s",
				    pms.inst[0], errbuf);
				errmsg.append(msg);
				goto skip;
			    }
			}
			switch (inst_match_type) {
			    case IM_MATCH:
				if (sts == 0)
				    pms.inst[0] = namelist[nextinst];
				break;
			    case IM_NOT_MATCH:
				if (sts == REG_NOMATCH)
				    pms.inst[0] = namelist[nextinst];
				break;
			    case IM_NONE:
				pms.inst[0] = namelist[nextinst];
				break;
			}
			if (pms.inst[0] != NULL)
			    break;
		    }
		    if (nextinst == numinst)
			goto skip;
		}
		if (legend != NULL && pms.inst[0] != NULL &&
		    (w = strstr(legend, "%i")) != NULL) {
		    // replace %i in legend
		    char	*tmp;
		    char	c;
		    tmp = (char *)malloc(strlen(legend) + strlen(pms.inst[0]) - 1);
		    if (tmp == NULL) nomem();
		    c = *w;	// copy up to (but not including) the %
		    *w = '\0';
		    strcpy(tmp, legend);
		    *w = c;
		    strcat(tmp, pms.inst[0]);
		    w +=2;
		    strcat(tmp, w);
		    m = cp->addPlot(&pms, tmp);
		    free(tmp);
		}
		else
		    m = cp->addPlot(&pms, legend);
		if (m < 0) {
		    if (!optional) {
			QString	msg;
			if (pms.inst[0] != NULL)
			    msg.sprintf("\nFailed to plot metric \"%s[%s]\" for\n%s %s:\n%s",
				pms.metric, pms.inst[0],
				pms.isarch ? "archive" : "host",
				pms.source, pmErrStr(m));
			else
			    msg.sprintf("\nFailed to plot metric \"%s\" for\n%s %s:\n%s",
				pms.metric, pms.isarch ? "archive" : "host",
				pms.source, pmErrStr(m));
			errmsg.append(msg);
		    }
		}
		else {
		    if (color != NULL && strcmp(color, "#-cycle") != 0) {
			c = new QColor();
			if (strncmp(color, "rgbi:", 5) == 0)
			    c = rgbi2qcolor(color);
			else
			    c->setNamedColor(QString(color));
			cp->setColor(m, *c);
		    }
		}
		if (numinst > 0)
		    // more instances to be procesed for this metric
		    goto try_plot;

	    }

skip:
	    if (legend != NULL) free(legend);
	    if (color != NULL) free(color);
	    if (host != NULL) free(host);
	    if (instlist != NULL) free(instlist);
	    if (namelist != NULL) free(namelist);
	    if (pms.metric != NULL) free(pms.metric);
	    if (pms.inst[0] != NULL) free(pms.inst[0]);
	    if (done_regex) regfree(&preg);

	    if (abort)
		goto abandon;

	    continue;
	}
	
	else {
	    QString	msg = QString();
	    msg.sprintf("Botch, state=%d", state);
	    err(E_CRIT, TRUE, msg);
	    goto abandon;
	}

	continue;

abandon:
	// giving up on the whole view specification
	break;

    }

    if (!errmsg.isEmpty()) {
	err(E_CRIT, TRUE, errmsg);
    }

    if (f != stdin) {
	if (is_popen)
	    pclose(f);
	else
	    fclose(f);
    }

    if (_errors)
	return;

    if (Cflag == 0 && cp != NULL)
	activeTab->setupWorldView();
}

void SaveViewDialog::saveView(const char *path, bool dynamic)
{
    FILE	*f;
    int		c;
    int		m;
    Chart	*cp;
    char	*p;
    char	*q;
    char	*qend;
    bool	autoscale;
    double	ymin;
    double	ymax;

    // TODO - dialog to confirm over writing an existing file
    // TODO - host dynamic vs literal - needs more code here.
    (void)dynamic;

    if ((f = fopen(path, "w")) == NULL) {
	QString	msg;
	msg.sprintf("Cannot open \"%s\" for writing\n%s", path, strerror(errno));
	err(E_CRIT, FALSE, msg);
	return;
    }
    fprintf(f, "#kmchart\nversion %d\n\n", K1);
    for (c = 0; c < activeTab->numChart(); c++) {
	cp = activeTab->chart(c);
	fprintf(f, "chart");
	p = cp->title();
	if (p != NULL)
	    fprintf(f, " title \"%s\"", p);
	switch (cp->style()) {
	    case None:
		p = "none - botched in Save!";
	    	break;
	    case Line:
		p = "plot";
		break;
	    case Bar:
		p = "bar";
		break;
	    case Stack:
		p ="stacking";
		break;
	    case Area:
		p = "area";
		break;
	    case Util:
		p = "utilization";
		break;
	}
	fprintf(f, " style %s", p);
	if (cp->style() != Util) {
	    cp->scale(&autoscale, &ymin, &ymax);
	    if (!autoscale)
		fprintf(f, " scale %f %f", ymin, ymax);
	}
	if (!cp->legendVisible())
	    fprintf(f, " legend off");
	fputc('\n', f);
	for (m = 0; m < cp->numPlot(); m++) {
	    fprintf(f, "\tplot");
	    p = cp->legend_spec(m);
	    if (p != NULL)
		fprintf(f, " legend \"%s\"", p);
	    fprintf(f, " color %s", cp->color(m).name().ascii());
	    p = cp->name(m)->ptr();
	    if ((q = strstr(p, "[")) != NULL) {
		// metric with an instance
		if ((qend = strstr(q, "]")) == NULL) {
		    QString	msg;
		    msg.sprintf("Botch @ metric name: \"%s\"", p);
		    err(E_CRIT, FALSE, msg);
		}
		else {
		    *q++ = '\0';
		    *qend = '\0';
		    fprintf(f, " metric %s instance \"%s\"", p, q);
		}
	    }
	    else
		// singular metric
		fprintf(f, " metric %s", p);
	    fputc('\n', f);
	}
    }

    fclose(f);
}
