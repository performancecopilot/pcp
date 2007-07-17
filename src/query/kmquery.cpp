/*
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
 * Contact information: Nathan Scott, nathans At debian DoT org
 */

#include "kmquery.h"
#include <qpixmap.h>
#include <qlayout.h>
#include <qimage.h>
#include <qlabel.h>

#define max(a,b) ((a)>(b)?(a):(b))
#define DEFAULT_EDIT_WIDTH	240	/* in units of pixels */
#define DEFAULT_EDIT_HEIGHT	160	/* only for usesliderflag */

enum icontypes {
    INFO_ICON,
    ERROR_ICON,
    QUESTION_ICON,
    WARNING_ICON,
    ARCHIVE_ICON,
    HOST_ICON,
} iconic;
static char *title = "Query";
static int timeout;
static int *statusi;
static char **buttons;
static int buttoncount;
static char *defaultbutton;
static char **messages;
static int messagecount;

static void nomem(void)
{
    fputs("Insufficient memory\n", stderr);
    exit(1);
}

int KmQuery::setTimeout(char *string)
{
    char *endnum;
    timeout = (int)strtol(string, &endnum, 10);
    if (*endnum != '\0' || timeout <= 0)
	return -1;
    return 0;
}

void KmQuery::setTitle(char *heading)
{
    title = heading;
}

int KmQuery::messageCount(void)
{
    return messagecount;
}

int KmQuery::buttonCount(void)
{
    return buttoncount;
}

int KmQuery::setIcontype(char *string)
{
    if (strcmp(string, "info") == 0)
	iconic = INFO_ICON;
    else if (strcmp(string, "error") == 0 ||
	strcmp(string, "action") == 0 ||
	strcmp(string, "critical") == 0)
	iconic = ERROR_ICON;
    else if (strcmp(string, "question") == 0)
	iconic = QUESTION_ICON;
    else if (strcmp(string, "warning") == 0)
	iconic = WARNING_ICON;
    else if (strcmp(string, "archive") == 0)
	iconic = ARCHIVE_ICON;
    else if (strcmp(string, "host") == 0)
	iconic = HOST_ICON;
    else
	return -1;
    return 0;
}

void KmQuery::addMessage(char *string)
{
    messages = (char **)realloc(messages, (messagecount+1) * sizeof(char *));
    if (!messages)
	nomem();
    messages[messagecount++] = string;
}

void KmQuery::addButton(char *string, bool iamdefault, int status)
{
    buttons = (char **)realloc(buttons, (buttoncount+1) * sizeof(char *));
    statusi = (  int *)realloc(statusi, (buttoncount+1) * sizeof(int));
    if (!buttons)
	nomem();
    if (iamdefault)
	defaultbutton = string;
    statusi[buttoncount] = status;
    buttons[buttoncount++] = string;
}

void KmQuery::addButtons(char *pairs) // comma-separated label:exitcode string
{
    char *n;
    static int next = 100;

    QStringList list = QStringList::split(",", tr(pairs));
    for (QStringList::Iterator it = list.begin(); it != list.end(); ++it) {
	QString name = (*it).section(":", 0, 0);
	QString code = (*it).section(":", 1, 1);
	if (!name.isEmpty()) {
	    int sts = code.isEmpty() ? ++next : code.toInt();
	    if ((n = strdup(name.ascii())) == NULL)
		nomem();
	    addButton(n, FALSE, sts);
	}
    }
}

void KmQuery::setDefaultButton(char *string)
{
    for (int i = 0; i < buttoncount; i++)
	if (strcmp(buttons[i], string) == 0)
	    defaultbutton = buttons[i];
}

void KmQuery::buttonClicked()
{
    done(sts);
}

void KmQuery::timerEvent(QTimerEvent *e)
{
    (void)e;
    done(1);
}

// Currently we set default edit size to hardcoded values, until
// better ways are found to interact with any geometry requests.
// Note: the +4 pixels for height ensure te auto-scroll does not
// kick in, seems to be required.

KmQuery::KmQuery(bool cflag, bool mouseflag, bool inputflag, bool printflag,
		 bool noframeflag, bool nosliderflag, bool usesliderflag,
		 bool exclusiveflag)
    : QDialog(NULL, NULL, FALSE, 0)
{
    int height;
    int width = DEFAULT_EDIT_WIDTH; 
    QPixmap pixmap;

    // TODO: currently unimplemented options ...
    (void)cflag; // center of display
    (void)mouseflag;
    (void)exclusiveflag;

    if (iconic == HOST_ICON)
	pixmap = QPixmap::fromMimeSource("dialog-host.png");
    else if (iconic == ERROR_ICON)
	pixmap = QPixmap::fromMimeSource("dialog-error.png");
    else if (iconic == WARNING_ICON)
	pixmap = QPixmap::fromMimeSource("dialog-warning.png");
    else if (iconic == ARCHIVE_ICON)
	pixmap = QPixmap::fromMimeSource("dialog-archive.png");
    else if (iconic == QUESTION_ICON)
	pixmap = QPixmap::fromMimeSource("dialog-question.png");
    else // (iconic == INFO_ICON)
	pixmap = QPixmap::fromMimeSource("dialog-information.png");

    setIcon(pixmap);
    setName("KmQuery");
    setCaption(tr(title));
    QGridLayout* KmQueryLayout;
    KmQueryLayout = new QGridLayout(this, 1, 1, 11, 6, "KmQueryLayout"); 

    QLabel *pixmapLabel = new QLabel(this, "pixmapLabel");
    pixmapLabel->setSizePolicy(QSizePolicy((QSizePolicy::SizeType)0,
				(QSizePolicy::SizeType)0, 0, 0,
				pixmapLabel->sizePolicy().hasHeightForWidth()));
    pixmapLabel->setScaledContents(FALSE);
    pixmapLabel->setPixmap(pixmap);

    KmQueryLayout->addWidget(pixmapLabel, 0, 0);

    QVBoxLayout* layout2;
    layout2 = new QVBoxLayout(0, 0, 6, "layout2"); 

    QLineEdit* lineEdit = NULL;
    QTextEdit* textEdit = NULL;
    if (inputflag && messagecount <= 1) {
	lineEdit = new QLineEdit(this, "lineEdit");
	lineEdit->setSizePolicy(QSizePolicy((QSizePolicy::SizeType)3,
				(QSizePolicy::SizeType)3, 0, 0,
				lineEdit->sizePolicy().hasHeightForWidth()));
	if (messagecount == 1)
	    lineEdit->setText(tr(messages[0]));
	height = usesliderflag ? DEFAULT_EDIT_HEIGHT :
				 lineEdit->heightForWidth(width) + 4;
	lineEdit->setMinimumSize(QSize(width, height));
	layout2->addWidget(lineEdit);
    }
    else {
	textEdit = new QTextEdit(this, "textEdit");
	textEdit->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
				QSizePolicy::MinimumExpanding, 0, 0,
				textEdit->sizePolicy().hasHeightForWidth()));
	textEdit->setWordWrap(QTextEdit::FixedColumnWidth);
	textEdit->setWrapColumnOrWidth(80);
	textEdit->setHScrollBarMode(QScrollView::AlwaysOff);
	if (nosliderflag)
	    textEdit->setVScrollBarMode(QScrollView::AlwaysOff);
	else if (usesliderflag)
	    textEdit->setVScrollBarMode(QScrollView::AlwaysOn);
	else
	    textEdit->setVScrollBarMode(QScrollView::Auto);
	for (int m = 0; m < messagecount; m++)
	    textEdit->append(tr(messages[m]));
	if (inputflag) {
	    textEdit->setReadOnly(FALSE);
	}
	else {
	    textEdit->setLineWidth(1);
	    textEdit->setFrameStyle(noframeflag ? QFrame::NoFrame :
				    QFrame::Box | QFrame::Sunken);
	    textEdit->setPaletteBackgroundColor(paletteBackgroundColor());
	    textEdit->setReadOnly(TRUE);
	}
	height = usesliderflag ? DEFAULT_EDIT_HEIGHT :
				 textEdit->heightForWidth(width) + 4;
	textEdit->setMinimumSize(QSize(width, height));
	layout2->addWidget(textEdit);
    }

    QHBoxLayout* layout1 = new QHBoxLayout(0, 0, 6, "layout1"); 
    QSpacerItem* spacer1 = new QSpacerItem(134, 32,
				QSizePolicy::Expanding, QSizePolicy::Minimum);
    layout1->addItem(spacer1);

    for (int i = 0; i < buttoncount; i++) {
	QueryButton* button = new QueryButton(printflag, this);
	button->setMinimumSize(QSize(72, 32));
	button->setDefault(buttons[i] == defaultbutton);
	button->setQuery(this);
	button->setText(tr(buttons[i]));
	button->setStatus(statusi[i]);
	if (inputflag && buttons[i] == defaultbutton) {
	    if (lineEdit)
		button->setEditor(lineEdit);
	    else if (textEdit)
		button->setEditor(textEdit);
	}
	layout1->addWidget(button);
	connect(button, SIGNAL(clicked()), this, SLOT(buttonClicked()));
    }

    layout2->addLayout(layout1);

    KmQueryLayout->addLayout(layout2, 0, 1);
    resize(QSize(318, 88).expandedTo(minimumSizeHint()));
    clearWState(WState_Polished);

    if (timeout)
	startTimer(timeout * 1000);
}
