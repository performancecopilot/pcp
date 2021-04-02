/*
 * cheap and nasty routine to load per-context derived metrics using
 * pmAddDerivedMetric() with an input file a la pmLoadDerivedConfig()
 * (but this routine loads globla derived metrics).
 * Copyright (c) 2020 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <ctype.h>
#include <errno.h>

#define STATE_BEGIN	0
#define STATE_NAME	1
#define STATE_ASSIGN	2
#define STATE_PRE_EXPR	3
#define STATE_EXPR	4
#define STATE_COMMENT	5

/*
 * Load per-context derived metrics from a config file ...
 * assumes current PMAPI context is valid.
 * Parser is brutal and expects:
 * name<whitespace>=<whitespace>expr<newline>
 * but tolerates blank lines and comment lines beginning with #
 *
 * Returns no. of derived metrics loaded, else -1 for error.
 */
int
add_ctx_dm(char *filename)
{
    FILE	*f;
    char	name[100];		/* no checks, assume big enough */
    char	expr[1000];		/* no checks, assume big enough */
    int		c;
    char	*np = NULL;
    char	*ep = NULL;
    int		state = STATE_BEGIN;
    int		lineno = 1;
    int		nload = 0;
    int		sts;
    char	*p;

    if ((f = fopen(filename, "r")) == NULL) {
	fprintf(stderr, "add_ctx_dm: fopen(%s) failed: %s\n", filename, strerror(errno));
	return -1;
    }

    while ((c = fgetc(f)) != EOF) {
	if (c == '\n') {
	    if (state == STATE_EXPR) {
		*ep = '\0';
		sts = pmAddDerivedMetric(name, expr, &p);
		if (sts < 0) {
		    fprintf(stderr, "%s:%d pmAddDerived(\"%s\", \"%s\") failed\n%s\n", filename, lineno, name, expr, p);
		    free(p);
		}
		else
		    nload++;
		state = STATE_BEGIN;
		lineno++;
		continue;
	    }
	    else if (state == STATE_COMMENT) {
		state = STATE_BEGIN;
		continue;
	    }
	    else if (state != STATE_BEGIN) {
		printf("%s:%d botch newline: state=%d", filename, lineno, state);
		if (state == STATE_NAME) {
		    *np = '\0';
		    printf(" name=\"%s\"", name);
		}
		else if (state == STATE_ASSIGN) {
		    printf(" name=\"%s\"", name);
		}
		putchar('\n');
		fclose(f);
		return -1;
	    }
	    continue;
	}
	if (c == '#' && state == STATE_BEGIN) {
	    state = STATE_COMMENT;
	    continue;
	}
	if (state == STATE_COMMENT) {
	    continue;
	}
	if (state == STATE_BEGIN) {
	    np = name;
	    state = STATE_NAME;
	}
	if (isspace(c)) {
	    if (state == STATE_NAME) {
		*np = '\0';
		state = STATE_ASSIGN;
	    }
	    if (state != STATE_EXPR)
		continue;
	}
	else if (c == '=') {
	    if (state == STATE_NAME) {
		*np = '\0';
		state = STATE_PRE_EXPR;
		ep = expr;
		continue;
	    }
	    else if (state == STATE_ASSIGN) {
		state = STATE_PRE_EXPR;
		ep = expr;
		continue;
	    }
	}
	if (state == STATE_PRE_EXPR) state = STATE_EXPR;
	if (state == STATE_NAME) *np++ = c;
	if (state == STATE_EXPR) *ep++ = c;
    }

    fclose(f);
    return nload;
}
