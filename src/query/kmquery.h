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
#ifndef KMQUERY_H
#define KMQUERY_H

#include <qvariant.h>
#include <qdialog.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qpushbutton.h>

class KmQuery : public QDialog
{
    Q_OBJECT
public:
    KmQuery(bool centerflag, bool mouseflag, bool inputflag, bool printflag,
	    bool noframeflag, bool nosliderflag, bool usesliderflag,
	    bool exclusiveflag);
    void setStatus(int status) { sts = status; }

    static void setTitle(char *string);
    static int setTimeout(char *string);
    static int setIcontype(char *string);

    static int messageCount(void);
    static void addMessage(char *string);

    static int buttonCount(void);
    static void addButton(char *string, bool iamdefault, int exitstatus);
    static void addButtons(char *stringlist);
    static void setDefaultButton(char *string);

public slots:
    virtual void buttonClicked();

protected:
    void timerEvent(QTimerEvent *);

private:
    int sts;
};

class QueryButton : public QPushButton
{
    Q_OBJECT
public:
    QueryButton(bool out, QWidget *p=0, const char *n=0) : QPushButton(p, n),
	s(0), k(0), l(0), t(0)
	{ if (out) connect(this, SIGNAL(clicked()), this, SLOT(print()));
	  else connect(this, SIGNAL(clicked()), this, SLOT(noprint())); }
    void setQuery(KmQuery *dialog) { k = dialog; }
    void setStatus(int status) { s = status; }
    void setEditor(QLineEdit *editor) { l = editor; }
    void setEditor(QTextEdit *editor) { t = editor; }

public slots:
    void print(void) { noprint(); puts(l ? l->text().ascii() : (t ?
					t->text().ascii() : text().ascii())); }
    void noprint(void) { k->setStatus(s); }
private:
    int s;
    KmQuery *k;
    QLineEdit *l;
    QTextEdit *t;
};

#endif // KMQUERY_H
