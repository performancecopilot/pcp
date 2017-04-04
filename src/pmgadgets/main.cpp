/*
 * Copyright (c) 2013, Red Hat.
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

#include <QApplication>
#include "pmgadgets.h"
#include "global.h"

AppData	appData;		// Global options/resources

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    PmGadgets gadgets;
    return gadgets.exec();
}

#if 0
#include <QDesktopWidget>
#include <QCursor>
static char usage[] =
    "Usage: pmgadgets [options] [message...]\n\n"
    "Options:\n"
    "  -? | -help display this usage message\n"
    "  -header title   set window title\n"
    "  -noslider       do not display a text box slider\n"
    "  -noframe        do not display a frame around the text box\n";

    char *option;
    char *filename = NULL;
    char *defaultname = NULL;

    int errflag = 0;
    if (errflag) {
	fprintf(stderr, "%s", usage);
	exit(1);
    }

    if (nearmouseflag)
	grid.move(QCursor::pos());
    else if (centerflag) {
	int x = (a.desktop()->screenGeometry().width() / 2) - (q.width() / 2);
	int y = (a.desktop()->screenGeometry().height() / 2) - (q.height() / 2);
	grid.move(x > 0 ? x : 0, y > 0 ? y : 0);
    }
#endif
