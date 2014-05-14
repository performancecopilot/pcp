/*
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
#include "pmquery.h"

#define max(a,b) ((a)>(b)?(a):(b))
#define DEFAULT_EDIT_WIDTH	420	/* in units of pixels */
#define DEFAULT_EDIT_HEIGHT	160	/* only for usesliderflag */

enum icontypes {
    INFO_ICON,
    ERROR_ICON,
    QUESTION_ICON,
    WARNING_ICON,
    ARCHIVE_ICON,
    HOST_ICON,
} iconic;

static const char *title = "Query";
static int timeout;
static int *statusi;
static const char **buttons;
static int buttoncount;
static const char *defaultbutton;
static char **messages;
static int messagecount;

static void nomem()
{
    fputs("Insufficient memory\n", stderr);
    exit(1);
}

int PmQuery::setTimeout(char *string)
{
    char *endnum;
    timeout = (int)strtol(string, &endnum, 10);
    if (*endnum != '\0' || timeout <= 0)
	return -1;
    return 0;
}

void PmQuery::setTitle(char *heading)
{
    title = heading;
}

int PmQuery::messageCount()
{
    return messagecount;
}

int PmQuery::buttonCount()
{
    return buttoncount;
}

int PmQuery::setIcontype(char *string)
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

void PmQuery::addMessage(char *string)
{
    messages = (char **)realloc(messages, (messagecount+1) * sizeof(char *));
    if (!messages)
	nomem();
    messages[messagecount++] = string;
}

void PmQuery::addButton(const char *string, bool iamdefault, int status)
{
    buttons = (const char **)realloc(buttons, (buttoncount+1) * sizeof(char *));
    statusi = (int *)realloc(statusi, (buttoncount+1) * sizeof(int));
    if (!buttons)
	nomem();
    if (iamdefault)
	defaultbutton = string;
    statusi[buttoncount] = status;
    buttons[buttoncount++] = string;
}

void PmQuery::addButtons(char *string) // comma-separated label:exitcode string
{
    char *n;
    QString pairs(string);
    static int next = 100;

    QStringList list = pairs.split(",");
    for (QStringList::Iterator it = list.begin(); it != list.end(); ++it) {
	QString name = (*it).section(":", 0, 0);
	QString code = (*it).section(":", 1, 1);
	if (!name.isEmpty()) {
	    int sts = code.isEmpty() ? ++next : code.toInt();
	    if ((n = strdup(name.toAscii().data())) == NULL)
		nomem();
	    addButton(n, FALSE, sts);
	}
    }
}

void PmQuery::setDefaultButton(char *string)
{
    for (int i = 0; i < buttoncount; i++)
	if (strcmp(buttons[i], string) == 0)
	    defaultbutton = buttons[i];
}

void PmQuery::buttonClicked()
{
    done(my.status);
}

void PmQuery::timerEvent(QTimerEvent *)
{
    done(1);
}

// Currently we set default edit size to hardcoded values, until
// better ways are found to interact with any geometry requests.
// Note: the +4 pixels for height ensure the auto-scroll does not
// kick in, seems to be required.

PmQuery::PmQuery(bool inputflag, bool printflag, bool noframeflag,
		 bool nosliderflag, bool usesliderflag, bool exclusiveflag)
    : QDialog()
{
    QHBoxLayout *hboxLayout;
    QVBoxLayout *vboxLayout;
    QSpacerItem *spacerItem;
    QSpacerItem *spacerItem1;
    QVBoxLayout *vboxLayout1;
    QHBoxLayout *hboxLayout1;
    QSpacerItem *spacerItem2;

    QString filename;
    if (iconic == HOST_ICON)
	filename = tr(":dialog-host.png");
    else if (iconic == ERROR_ICON)
	filename = tr(":dialog-error.png");
    else if (iconic == WARNING_ICON)
	filename = tr(":dialog-warning.png");
    else if (iconic == ARCHIVE_ICON)
	filename = tr(":dialog-archive.png");
    else if (iconic == QUESTION_ICON)
	filename = tr(":dialog-question.png");
    else // (iconic == INFO_ICON)
	filename = tr(":dialog-information.png");

    QIcon	icon(filename);
    QPixmap	pixmap(filename);
    setWindowIcon(icon);
    setWindowTitle(tr(title));

    QGridLayout *gridLayout = new QGridLayout(this);
    gridLayout->setSpacing(6);
    gridLayout->setMargin(9);
    hboxLayout = new QHBoxLayout();
    hboxLayout->setSpacing(6);
    hboxLayout->setMargin(0);
    vboxLayout = new QVBoxLayout();
    vboxLayout->setSpacing(6);
    vboxLayout->setMargin(0);
    spacerItem = new QSpacerItem(20, 2, QSizePolicy::Minimum,
					QSizePolicy::Expanding);

    vboxLayout->addItem(spacerItem);

    QLabel *imageLabel = new QLabel(this);
    imageLabel->setPixmap(pixmap);

    vboxLayout->addWidget(imageLabel);

    spacerItem1 = new QSpacerItem(20, 20, QSizePolicy::Minimum,
					  QSizePolicy::Expanding);

    vboxLayout->addItem(spacerItem1);
    hboxLayout->addLayout(vboxLayout);
    vboxLayout1 = new QVBoxLayout();
    vboxLayout1->setSpacing(6);
    vboxLayout1->setMargin(0);

    int height;
    int width = DEFAULT_EDIT_WIDTH; 

    QLineEdit* lineEdit = NULL;
    QTextEdit* textEdit = NULL;
    if (inputflag && messagecount <= 1) {
	lineEdit = new QLineEdit(this);
	if (messagecount == 1)
	    lineEdit->setText(tr(messages[0]));
	height = lineEdit->font().pointSize() + 4;
	if (height < 0)
	    height = lineEdit->font().pixelSize() + 4;
	if (height < 0)
	    height = lineEdit->heightForWidth(width) + 4;
	lineEdit->setSizePolicy(QSizePolicy::MinimumExpanding,
				QSizePolicy::Fixed);
	lineEdit->setMinimumSize(QSize(width, height));
	lineEdit->setGeometry(QRect(0, 0, width, height));
	vboxLayout1->addWidget(lineEdit);
    }
    else {
	textEdit = new QTextEdit(this);
	textEdit->setLineWrapMode(QTextEdit::FixedColumnWidth);
	textEdit->setLineWrapColumnOrWidth(80);
	textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	if (nosliderflag)
	    textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	else if (usesliderflag)
	    textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	else
	    textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	for (int m = 0; m < messagecount; m++)
	    textEdit->append(tr(messages[m]));
	if (inputflag)
	    textEdit->setReadOnly(FALSE);
	else {
	    textEdit->setLineWidth(1);
	    textEdit->setFrameStyle(noframeflag ? QFrame::NoFrame :
				    QFrame::Box | QFrame::Sunken);
	    textEdit->setReadOnly(TRUE);
	}
	if (usesliderflag)
	    height = DEFAULT_EDIT_HEIGHT;
	else {
	    height = textEdit->font().pointSize() + 4;
	    if (height < 0)
		height = textEdit->font().pixelSize() + 4;
	    if (height < 0)
	        height = textEdit->heightForWidth(width) + 4;
	    height *= messagecount;
	}
	textEdit->setMinimumSize(QSize(width, height));
	textEdit->setSizePolicy(QSizePolicy::MinimumExpanding,
				QSizePolicy::MinimumExpanding);
	vboxLayout1->addWidget(textEdit);
    }

    hboxLayout1 = new QHBoxLayout();
    hboxLayout1->setSpacing(6);
    hboxLayout1->setMargin(0);
    spacerItem2 = new QSpacerItem(40, 20, QSizePolicy::Expanding,
					  QSizePolicy::Minimum);
    hboxLayout1->addItem(spacerItem2);

    for (int i = 0; i < buttoncount; i++) {
	QueryButton *button = new QueryButton(printflag, this);
	button->setMinimumSize(QSize(72, 32));
	button->setDefault(buttons[i] == defaultbutton);
	button->setQuery(this);
	button->setText(tr(buttons[i]));
	button->setStatus(statusi[i]);
	if (inputflag && buttons[i] == defaultbutton) {
	    if (textEdit) 
		button->setEditor(textEdit);
	    else if (lineEdit) {
		button->setEditor(lineEdit);
		if (buttons[i] == defaultbutton)
		    connect(lineEdit, SIGNAL(returnPressed()),
			    button, SLOT(click()));
	    }
	}
	connect(button, SIGNAL(clicked()), this, SLOT(buttonClicked()));
	hboxLayout1->addWidget(button);
    }

    vboxLayout1->addLayout(hboxLayout1);
    hboxLayout->addLayout(vboxLayout1);
    gridLayout->addLayout(hboxLayout, 0, 0, 1, 1);
    gridLayout->setSizeConstraint(QLayout::SetFixedSize);

    if (inputflag && messagecount <= 1)
	resize(QSize(320, 83));
    else
	resize(QSize(320, 132));

    if (timeout)
	startTimer(timeout * 1000);

    if (exclusiveflag)
	setWindowModality(Qt::WindowModal);
}
