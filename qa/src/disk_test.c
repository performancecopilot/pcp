/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

static int listOne[2];
#define LEN(l) (sizeof(l)/sizeof(l[0]))

static char *namelist[] = {
    "disk.dev.read"
};

#define NCONTEXTS 2

int
main(int argc, char **argv)
{
    int		e, i;
    int		h[NCONTEXTS];
    pmID	metrics[2];
    pmResult	*resp;
    pmInDom	diskindom;
    pmDesc	desc;
    int		*inst;
    char	**name;
    int		numinst;
    int		c;
    int		sts;
    int		errflag = 0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc) {
	fprintf(stderr, "Usage: %s [-D debugspec]\n", pmProgname);
	exit(1);
    }

    for (i=0; i < NCONTEXTS; i++) {
	if ((h[i] = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	    printf("pmNewContext: %s\n", pmErrStr(h[i]));
	    exit(1);
	}

	if ((e = pmLookupName(1, namelist, metrics)) < 0) {
	    printf("pmLookupName: %s\n", pmErrStr(e));
	    exit(1);
	}

	if ((e = pmLookupDesc(metrics[0], &desc)) < 0) {
	    printf("pmLookupDesc: %s\n", pmErrStr(e));
	    exit(1);
	}
    }

    diskindom = desc.indom;
    if ((numinst = pmGetInDom(diskindom, &inst, &name)) < 0) {
	printf("pmGetInDom: %s\n", pmErrStr(numinst));
	exit(1);
    }
    printf("Disks:\n");
    for (i = 0; i < numinst; i++)
	printf("\t[%d]: %d %s\n", i, inst[i], name[i]);

    for (i=0; i < NCONTEXTS; i++) {
	pmUseContext(h[i]);

	listOne[0] = inst[0];
	listOne[1] = inst[numinst-1];

	pmAddProfile(diskindom, 0, (int *)0);
	printf("all drives should be included here\n");
	if ((e = pmFetch(1, metrics, &resp)) < 0) {
	    printf("pmFetch[2]: %s\n", pmErrStr(e));
	}
	else
	    __pmDumpResult(stdout, resp);

	pmDelProfile(diskindom, 0, (int *)0);
	putchar('\n');
	printf("no drives should be included here\n");
	if ((e = pmFetch(1, metrics, &resp)) < 0) {
	    printf("pmFetch[3]: %s\n", pmErrStr(e));
	}
	else
	    __pmDumpResult(stdout, resp);

	pmDelProfile(diskindom, 0, (int *)0);
	pmAddProfile(diskindom, LEN(listOne), listOne);
	putchar('\n');
	printf("only the first and last drive should be included here\n");
	if ((e = pmFetch(1, metrics, &resp)) < 0) {
	    printf("pmFetch[0]: %s\n", pmErrStr(e));
	}
	else
	    __pmDumpResult(stdout, resp);

	pmDelProfile(diskindom, 0, (int *)0);
	pmAddProfile(diskindom, numinst, inst);
	pmDelProfile(diskindom, 1, listOne);
	putchar('\n');
	printf("all except drive zero should be included here\n");
	if ((e = pmFetch(1, metrics, &resp)) < 0) {
	    printf("pmFetch[1]: %s\n", pmErrStr(e));
	}
	else
	    __pmDumpResult(stdout, resp);

	pmDelProfile(diskindom, 0, (int *)0);
	pmAddProfile(diskindom, 1, inst);
	putchar('\n');
	printf("drive zero ONLY should be included here\n");
	if ((e = pmFetch(1, metrics, &resp)) < 0) {
	    printf("pmFetch[1]: %s\n", pmErrStr(e));
	}
	else
	    __pmDumpResult(stdout, resp);
    }

    exit(0);
}

