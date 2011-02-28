/*
 * Copyright 1998, Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdarg.h>
#include "pmapi.h"
#include "impl.h"
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif

#define MINCOLS	80
#define MINROWS	24

static int	neols = -1;
static int	needresize;
static int	needscroll;
static int	skiprest;
static int	nrows;
static int	ncols;
static char	shortmsg[] = "more? (h=help) ";
static char	longmsg[] = \
	"[q or n to stop, y or <space> to go on, <enter> to step] more? ";
#ifdef HAVE_TERMIO_H
static struct termio	otty;
#endif

void setio(int reset)	{ neols = 0; skiprest = reset; }
void setscroll(void)	{ needscroll = 1; }
int  resized(void)	{ return needresize; }

/* looks after window resizing for the printing routine */
void
onwinch(int dummy)
{
#ifdef SIGWINCH
    __pmSetSignalHandler(SIGWINCH, onwinch);
#endif
    needresize = 1;
}

/* in interactive mode scrolling, if no more wanted skiprest is set */
static void
promptformore(void)
{
    int			i;
    int			ch;
    int			sts = 1;
    char		c;
    char		*prompt;
#ifdef HAVE_TERMIO_H
    static int		first = 1;
    struct termio	ntty;
#endif

#ifdef HAVE_TERMIO_H
    if (first) {
	if (ioctl(0, TCGETA, &otty) < 0) {
	    fprintf(stderr, "%s: TCGETA ioctl failed: %s\n", pmProgname,
		    osstrerror());
	    exit(1);
	}
	first = 0;
    }

    /* put terminal into raw mode so we can read input immediately */
    memcpy(&ntty, &otty, sizeof(struct termio));
    ntty.c_cc[VMIN] = 1;
    ntty.c_cc[VTIME] = 1;
    ntty.c_lflag &= ~(ICANON | ECHO);
    if (ioctl(0, TCSETAW, &ntty) < 0) {
	fprintf(stderr, "%s: TCSETAW ioctl failed: %s\n", pmProgname,
		osstrerror());
	exit(1);
    }
#endif

    prompt = shortmsg;
    while (sts == 1) {
	putchar('\r');
	for (i = 0; i < ncols-1; i++)
	    putchar(' ');
	putchar('\r');
	printf("%s", prompt);
	fflush(stdout);

	if (read(0, &c, 1) != 1) {
	    sts = 1;
	    goto reset_tty;
	}
	ch = (int)c;

	switch(ch) {
	case 'n':	/* stop */
	case 'q':
	    setio(1);
	    sts = 0;
	    break;
	case 'y':	/* page down */
	case ' ':
	    neols = sts = 0;
	    break;
	case '\n':	/* step down */
	    neols = nrows;
	    sts = 0;
	    break;
	default:
	    prompt = longmsg;
	}
    }

reset_tty:
#ifdef HAVE_TERMIO_H
    if (ioctl(0, TCSETAW, &otty) < 0) {
	fprintf(stderr, "%s: reset TCSETAW ioctl failed: %s\n", pmProgname,
		osstrerror());
	exit(1);
    }
#endif

    putchar('\r');
    for (i = 0; i < ncols-1; i++)
	putchar(' ');
    putchar('\r');
    fflush(stdout);
}

/*
 * generic printing routine which can pause at end of a screenful.
 * if this returns 1, the user has requested an end to this info,
 * so the caller must always observe the pprintf return value.
 */
void
pprintf(char *format, ...)
{
    char		*p;
    va_list		args;
#ifdef TIOCGWINSZ
    struct winsize	geom;
#endif
    static int		first = 1;

    if (first == 1) {	/* first time thru */
	first = 0;
#ifdef TIOCGWINSZ
	ioctl(0, TIOCGWINSZ, &geom);
	nrows = (geom.ws_row < MINROWS? MINROWS : geom.ws_row);
	ncols = (geom.ws_col < MINCOLS? MINCOLS : geom.ws_col);
#else
	nrows = MINROWS;
	ncols = MINCOLS;
#endif
    }

    if (skiprest)
	return;

    va_start(args, format);
    if (needscroll) {
	/*
	 * use the fact that i know we never print more than MINROWS at once
	 * to figure out how many lines we've done before doing the vfprintf
	 */
	if (neols >= nrows-1) {
	    promptformore();
	    if (skiprest) {
		va_end(args);
		return;
	    }
	}
	for (p = format; *p != '\0'; p++)
	    if (*p == '\n') neols++;
	vfprintf(stdout, format, args);
    }
    else
	vfprintf(stdout, format, args);
    va_end(args);
    if (needresize) {
#ifdef HAVE_TIOCGWINSZ
	ioctl(0, TIOCGWINSZ, &geom);
	nrows = (geom.ws_row < MINROWS? MINROWS : geom.ws_row);
	ncols = (geom.ws_col < MINCOLS? MINCOLS : geom.ws_col);
#ifdef PMIECONF_DEBUG
	printf("debug - reset size: cols=%d rows=%d\n", ncols, nrows);
#endif
#endif
	needresize = 0;
    }
}

/* general error printing routine */
void
error(char *format, ...)
{
    va_list	args;
    FILE	*f;

    if (skiprest)
	return;
    va_start(args, format);
    if (needscroll) {
	f = stdout;
	fprintf(f, "  Error - ");
    }
    else {
	f = stderr;
	fprintf(f, "%s: error - ", pmProgname);
    }
    vfprintf(f, format, args);
    fprintf(f, "\n");
    neols++;
    va_end(args);
}
