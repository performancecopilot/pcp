/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Display offset date
 */

#include "pmapi.h"
#include "impl.h"

#define usage "Usage: pmdate { +valueS | -valueS } ... format\n\
\n\
where the scale \"S\" is one of: S (seconds), M (minutes), H (hours), \n\
d (days), m (months) or y (years)\n"

int
main(int argc, char *argv[])
{
    time_t	now;
    time_t	check;
    int		need;
    char	*buf;
    char	*p;
    char	*pend;
    struct tm	*tmp;
    int		sgn;
    int		val;
    int		mo_delta = 0;
    int		yr_delta = 0;

    __pmSetProgname(argv[0]);

    if (argc < 2) {
	fprintf(stderr, usage);
	exit(1);
    }

    if (strcmp(argv[1], "-?") == 0 || strcmp(argv[1], "--help") == 0) { 
	fprintf(stderr, usage);
	exit(1);
    }

    time(&now);

    while (argc > 2) {
	p = argv[1];
	if (*p == '+')
	    sgn = 1;
	else if (*p == '-')
	    sgn = -1;
	else {
	    fprintf(stderr, "%s: incorrect leading sign for offset (%s), must be \"+\" or \"-\"\n",
		       pmProgname, argv[1]);
	    exit(1);
	}
	p++;

	val = (int)strtol(p, &pend, 10);
	switch (*pend) {
	    case 'S':
		now += sgn * val;
		break;
	    case 'M':
		now += sgn * val * 60;
		break;
	    case 'H':
		now += sgn * val * 60 * 60;
		break;
	    case 'd':
		now += sgn * val * 24 * 60 * 60;
		break;
	    case 'm':
		mo_delta += sgn*val;
		break;
	    case 'y':
		yr_delta += sgn*val;
		break;
	    case '\0':
		fprintf(stderr, "%s: missing scale after offset (%s)\n", pmProgname, argv[1]);
		exit(1);
	    case '?':
		fprintf(stderr, usage);
		exit (1);
	    default:
		fprintf(stderr, "%s: unknown scale after offset (%s)\n", pmProgname, argv[1]);
		exit(1);
	}

	argv++;
	argc--;
    }

    tmp = localtime(&now);

    if (yr_delta) {
	/*
	 * tm_year is years since 1900 and yr_delta is relative (not
	 * absolute), so this is Y2K safe
	 */
	tmp->tm_year += yr_delta;
	/* TODO feb leap year */
    }
    if (mo_delta) {
	/*
	 * tm_year is years since 1900 and the tm_year-- and
	 * tm_year++ is adjusting for underflow and overflow in
	 * tm_mon as a result of relative month delta, so this
	 * is Y2K safe
	 */
	tmp->tm_mon += mo_delta;
	while (tmp->tm_mon < 0) {
	    tmp->tm_mon += 12;
	    tmp->tm_year--;
	}
	while (tmp->tm_mon > 12) {
	    tmp->tm_mon -= 12;
	    tmp->tm_year++;
	}
    }

    if ((check = mktime(tmp)) == -1) {
	fprintf(stderr, "%s: impossible date conversion\n", pmProgname);
	exit(1);
    }

    /*
     * Note:    256 is _more_ than enough to accommodate the longest
     *		value for _every_ %? lexicon that strftime() understands
     */
    need = strlen(argv[1]) + 256;
    if ((buf = (char *)malloc(need)) == NULL) {
	fprintf(stderr, "%s: malloc failed\n", pmProgname);
	exit(1);
    }

    if (strftime(buf, need, argv[1], tmp) == 0) {
	fprintf(stderr, "%s: format too long\n", pmProgname);
	exit(1);
    }
    else {
	buf[need-1] = '\0';
	printf("%s\n", buf);
	exit(0);
    }

}
