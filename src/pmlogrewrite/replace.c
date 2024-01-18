/*
 * sed-like replacement function for a set of regular expressions.
 *
 * Copyright (c) 2023 Ken McDonell.  All Rights Reserved.
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

#include "pmapi.h"
#include "libpcp.h"
#include "logger.h"

#define push(c) {\
if (r >= reslen) {\
char *tmp_result;\
reslen = reslen == 0 ? 1024 : 2*reslen;\
tmp_result = (char *)realloc(result, reslen);\
if (tmp_result == NULL) {\
    fprintf(stderr, "replace: result realloc(%d) failed!\n", reslen);\
    abandon();\
    /*NOTREACHED*/\
}\
result = tmp_result;\
}\
result[r++] = c;\
}

#define MAXSUB	9	/* \1 thru \9 */
/*
 *
 * input string is buf[]
 *
 * vcp[].pat is the uncompiled regular expression (only used here in
 * diagnostics)
 * vcp[].regex is a previously compiled regular expression
 * vcp[].replace is the sed-like replacement text and metacharacters
 * (& and \1 ... \9 in particular)
 *
 * each of the nvc possible match-and-replacement operations is applied
 * in turn
 */
char *
re_replace(char *buf, value_change_t *vcp, int nvc)
{
    char	*target;	/* string to be matched */
    int		k;		/* vcp[] index for multiple replacements */
    int		i;		/* misc index */
    int		sts;
    /*
     * MAXSUB+1 is enough for the whole match [0] and then substrings
     * \1 [1] thru \9 [9]
     */
    regmatch_t	pmatch[MAXSUB+1];

    target = buf;
    for (k = 0; k < nvc; k++) {
	sts = regexec(&vcp[k].regex, target, MAXSUB+1, pmatch, 0);
	if (sts == REG_NOMATCH) {
	    continue;
	}
	if (pmDebugOptions.appl5) {
	    fprintf(stderr, "regex[%d] target \"%s\" -> match (%d)", k, target, sts);
	}
	if (sts == 0) {
	    char	*result = NULL;
	    int		reslen = 0;	/* allocated size of result */
	    int		r = 0;		/* next posn to append to result[] */
	    int		seenslash;
	    char	*p;
	    if (pmDebugOptions.appl5) {
		for (i = 0; i <= MAXSUB; i++) {
		    if (pmatch[i].rm_so == -1)
			break;
		    if (i == 0)
			fprintf(stderr, " & [%lld,%lld]", (long long)pmatch[i].rm_so, (long long)pmatch[i].rm_eo);
		    else
			fprintf(stderr, " \\%d [%lld,%lld]", i, (long long)pmatch[i].rm_so, (long long)pmatch[i].rm_eo);
		}
		fputc('\n', stderr);
	    }
	    /* prefix before matched re */
	    for (i = 0; i < pmatch[0].rm_so; )
		push(target[i++]);
	    /* re replacement */
	    seenslash = 0;
	    for (p = vcp[k].replace; *p; p++) {
		if (seenslash) {
		    seenslash = 0;
		    if (*p >= '1' && *p <= '9') {
			int	sub = *p - '0';
			if (pmatch[sub].rm_so >= 0) {
			    for (i = pmatch[sub].rm_so; i < pmatch[sub].rm_eo; )
				push(target[i++]);
			}
			else {
			    fprintf(stderr, "Botch: no \\%d substring from regexp match\n", sub);
			    fprintf(stderr, "    metric value: %s\n", target);
			    fprintf(stderr, "    regex: %s\n", vcp[k].pat);
			    fprintf(stderr, "    replacement: %s\n", vcp[k].replace);
			    abandon();
			    /*NOTREACHED*/
			}
		    }
		    else
			push(*p);
		}
		else if (*p == '\\') {
		    seenslash = 1;
		}
		else if (*p == '&') {
		    for (i = pmatch[0].rm_so; i < pmatch[0].rm_eo; )
			push(target[i++]);
		}
		else {
		    push(*p);
		}
	    }
	    /* suffix after matched re */
	    for (i = pmatch[0].rm_eo; target[i] != '\0'; )
		push(target[i++]);
	    push('\0');
	    if (target != buf)
		free(target);
	    target = result;
	}
	else {
	    if (pmDebugOptions.appl5) {
		fputc('\n', stderr);
	    }
	}
    }

    if (target == buf)
	return NULL;
    else
	return target;
}

