/*
 * Copyright (c) 2016 Ken McDonell.  All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

/*
 * sort instances based on instance name from pminfo/__pmDumpResult
 * output like
 *	inst [0 or "sdc1"] value 24163451
 *	inst [1 or "dm-1"] value 24163428
 * 
 * Different ordering may arise because the pmda is using readdir()
 * and the directory contents are created from a QA tar ball, in which
 * case the order of the directory entries is non-deterministic.
 *
 * Note: Feasible extensions would include:
 * - allow an alternate pattern (pat) from the command line
 * - allow an alternate sort key prefix char (pfx) from the command line
 */

#define MAXLINELEN 200

static char pfx = '"';

/*
 * compare keys up following the pfx character ... no error
 * checking here, assume the QA engineer knows what they're doing
 */
int
compar(const void *a, const void *b)
{
    char *key_a = *((char **)a);
    char *key_b = *((char **)b);

    for (key_a++; key_a[0] && key_a[-1] != pfx; key_a++)
	;
    for (key_b++; key_b[0] && key_b[-1] != pfx; key_b++)
	;

    return strcmp(key_a, key_b);
}

int
main(int argc, char **argv)
{
    /* run as a filter, ignore command line args for the moment */
    int		sts;
    char	**sortlines = NULL;
    int		nlines = 0;		/* size of sortlines[] */
    int		cur = 0;		/* input lines in sortlines[] */
    int		n = 0;			/* input line count */
    char	*pat = "inst \\[";
    regex_t	re;

    sts = regcomp(&re, pat, REG_NOSUB);
    if (sts != 0) {
	char	errmsg[100];
	regerror(sts, &re, errmsg, sizeof(errmsg));
	fprintf(stderr, "Error: bad regex /%s/: %s\n", pat, errmsg);
	exit(1);
    }

    for ( ; ; ) {
	if (cur >= nlines) {
	    sortlines = (char **)realloc(sortlines, (nlines+1)*sizeof(char *));
	    if (sortlines == NULL) {
		fprintf(stderr, "realloc %d sortlines failed\n", nlines+1);
		return(1);
	    }
	    sortlines[nlines] = (char *)malloc(MAXLINELEN);
	    if (sortlines[nlines] == NULL) {
		fprintf(stderr, "malloc sortlines[%d] failed\n", nlines);
		return(1);
	    }
	    nlines++;
	}
	n++;
	if (fgets(sortlines[cur], MAXLINELEN, stdin) == NULL) {
	    break;
	}
	sts = strlen(sortlines[cur]);
	if (sortlines[cur][sts-1] != '\n') {
	    fprintf(stderr, "Error: input truncated at line %d. Increase MAXLINELEN.\n", n);
	    exit(1);
	}

	sts = regexec(&re, sortlines[cur], 0, NULL, 0);
	if (sts == 0) {
	    /* match */
	    cur++;
	}
	else {
	    /* no match */
	    if (cur > 0) {
		/* sort previous block and output */
		int	i;
		qsort(sortlines, cur, sizeof(sortlines[0]), compar);
		for (i = 0; i < cur; i++)
		    fputs(sortlines[i], stdout);
	    }
	    /* output current line */
	    fputs(sortlines[cur], stdout);
	    cur = 0;
	}
    }

    if (cur > 0) {
	int	i;
	qsort(sortlines, cur, sizeof(sortlines[0]), compar);
	for (i = 0; i < cur; i++)
	    fputs(sortlines[i], stdout);
    }

    return(0);

}
