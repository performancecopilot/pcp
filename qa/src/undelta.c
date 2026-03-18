/*
 * workout for __pmLogUndeltaInDom() and in particular
 * https://github.com/performancecopilot/pcp/pull/2525/changes
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2026 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,	/* -D */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[-D debug]",
};

__pmLogInDom	*lidp1;		/* full indom */
__pmLogInDom	*lidp2;		/* delta indom */
char		**name1;	/* copy of lidp1->namelist[] after build() */
char		**name2;	/* copy of lidp2->namelist[] after build() */
int		num1;		/* number of name1[] entries */
int		num2;		/* number of name2[] entries */
pmInDom		indom;
char		buf[1024];

static void
allocstr(int a)
{
    int	onetrip = 0;

    if (a & PMLID_SELF) {
	if (onetrip)
	    putchar('|');
	else
	    onetrip = 1;
	printf("SELF");
    }
    if (a & PMLID_INSTLIST) {
	if (onetrip)
	    putchar('|');
	else
	    onetrip = 1;
	printf("INSTLIST");
    }
    if (a & PMLID_NAMELIST) {
	if (onetrip)
	    putchar('|');
	else
	    onetrip = 1;
	printf("NAMELIST");
    }
    if (a & PMLID_NAMES) {
	if (onetrip)
	    putchar('|');
	else
	    onetrip = 1;
	printf("NAMES");
    }
}

static void
dump(__pmLogInDom *lidp, char *what)
{
    int		i;
    int		k;

    printf("== dump %s __pmLogInDom @ %p\n", what, lidp);
    __pmPrintTimestamp(stdout, &lidp->stamp);
    printf(" alloc: ");
    allocstr(lidp->alloc);
    printf(" isdelta: %d numinst: %d\n", lidp->isdelta, lidp->numinst);
    for (i = 0; i < lidp->numinst; i++) {
	printf("[%d] %d", i, lidp->instlist[i]);
	if (lidp->namelist[i] == NULL)
	    printf(" NULL");
	else {
	    printf(" \"%s\" %p", lidp->namelist[i], lidp->namelist[i]);
	    if (lidp != lidp1) {
		for (k = 0; k < num1; k++) {
		    if (lidp->namelist[i] == name1[k])
			printf(" == lidp1->namelist[%d]", k);
		}
	    }
	    if (lidp != lidp2) {
		for (k = 0; k < num2; k++) {
		    if (lidp->namelist[i] == name2[k])
			printf(" == lidp2->namelist[%d]", k);
		}
	    }
	}
	putchar('\n');
    }
}

/*
 * construct the two __pmLogInDom's
 */
static void
build(int alloc)
{
    char		*p = buf;
    int			i;
    int			k;

    lidp1 = (__pmLogInDom *)malloc(sizeof(__pmLogInDom));
    lidp1->alloc = PMLID_SELF|alloc|PMLID_NAMELIST|PMLID_INSTLIST;
    lidp1->next = lidp1->prior = NULL;
    lidp1->indom = indom;
    lidp1->stamp.sec = 12 * 3600 + 34 * 60 + 56;		/* 12:34:56 UTC */
    lidp1->stamp.nsec = 100;
    lidp1->isdelta = 0;
    num2 = lidp1->numinst = 5;
    lidp1->namelist = (char **)malloc(lidp1->numinst * sizeof(char *));
    if (name1 != NULL)
	free(name1);
    name1 = (char **)malloc(lidp1->numinst * sizeof(char *));
    lidp1->instlist = (int *)malloc(lidp1->numinst * sizeof(int));
    for (i = 0; i < lidp1->numinst; i++) {
	lidp1->instlist[i] = i;
	pmsprintf(p, sizeof(buf) - (p-buf), "external %02d", i);
	if (alloc == PMLID_NAMES)
	    name1[i] = lidp1->namelist[i] = strdup(p);
	else {
	    name1[i] = lidp1->namelist[i] = p;
	    p += strlen(p) + 1;
	}
    }
    lidp1->buf = NULL;

    lidp2 = (__pmLogInDom *)malloc(sizeof(__pmLogInDom));
    lidp2->alloc = PMLID_SELF|alloc|PMLID_NAMELIST|PMLID_INSTLIST;
    lidp2->indom = indom;
    lidp2->stamp.sec = 12 * 3600 + 34 * 60 + 56;		/* 12:34:56 UTC */
    lidp2->stamp.nsec = 200;
    lidp2->isdelta = 1;
    num2 = lidp2->numinst = lidp1->numinst - 1;
    lidp2->namelist = (char **)malloc(lidp2->numinst * sizeof(char *));
    if (name2 != NULL)
	free(name2);
    name2 = (char **)malloc(lidp2->numinst * sizeof(char *));
    lidp2->instlist = (int *)malloc(lidp2->numinst * sizeof(int));
    /* delete some instances */
    k = 0;
    for (i = 0; i < lidp2->numinst; i++) {
	if ((i & 1) == 0) {
	    lidp2->instlist[k] = i;
	    name2[k] = lidp2->namelist[k] = NULL;
	    k++;
	}
    }
    /* add some instances */
    for (i = 0; i < lidp2->numinst; i++) {
	if (i & 1) {
	    /* add instance */
	    lidp2->instlist[k] = i+10;
	    pmsprintf(p, sizeof(buf) - (p-buf), "external %02d", i+10);
	    if (alloc == PMLID_NAMES)
		name2[k] = lidp2->namelist[k] = strdup(p);
	    else {
		name2[k] = lidp2->namelist[k] = p;
		p += strlen(p) + 1;
	    }
	    k++;
	}
    }
    lidp2->buf = NULL;

    /* link entries ... "next" is reverse chronological order */
    lidp1->next = NULL;
    lidp1->prior = lidp2;
    lidp2->next = lidp1;
    lidp2->prior = NULL;

    dump(lidp1, "lidp1 after build()");
    dump(lidp2, "lidp2 after build()");

}

int
main(int argc, char **argv)
{
    int			c;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    indom = pmInDom_build(42, 13);

    printf("=== malloc namelist[]\n");
    build(PMLID_NAMES);
    __pmLogUndeltaInDom(indom, lidp2);
    dump(lidp2, "lidp2 after __pmLogUndeltaInDom");
    __pmFreeLogInDom(lidp1);
    __pmFreeLogInDom(lidp2);

    putchar('\n');
    printf("=== namelist[] not malloc'd\n");
    build(0);
    __pmLogUndeltaInDom(indom, lidp2);
    dump(lidp2, "lidp2 after __pmLogUndeltaInDom");
    __pmFreeLogInDom(lidp1);
    __pmFreeLogInDom(lidp2);

    free(name1);
    free(name2);

    return 0;
}
