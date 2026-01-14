/*
 * Generate an ASCII PMNS for all the metrics at or below metricname,
 * context defaults to pmcd on local:
 *
 * Most useful for dbpmda when exercising PMDA's with dynamic metrics.
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2025 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMOPT_ARCHIVE,	/* -a */
    PMOPT_DEBUG,	/* -D */
    PMOPT_HOST,		/* -h */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "a:D:h:?",
    .long_options = longopts,
    .short_usage = "[options] metricname",
};

int		ctx = -1;
typedef struct list {
    struct list	*next;
    char	*name;
} list_t;
list_t	*head = NULL;
list_t	*tail = NULL;

/*
 * quick and dirty static string concatenator
 */
char *
mkname(const char *prefix, const char *name)
{
    static char	buf[1024];	/* long enough for the longest PMNS name */

    pmstrncpy(buf, sizeof(buf), prefix);
    pmstrncat(buf, sizeof(buf), ".");
    pmstrncat(buf, sizeof(buf), name);

    return buf;
}

void
dochn(char *name, int root)
{
    int		nchn;
    int		sts;
    char	**offspring;
    int		*status;
    int		i;
    pmID	pmid;
    char	*fullname;
    char	*p;

    if (pmDebugOptions.appl0)
	fprintf(stderr, "dochn(\"%s\")\n", name);

    if ((nchn = pmGetChildrenStatus(name, &offspring, &status)) < 1) {
	fprintf(stderr, "Error: pmGetChildrenStatus(%s, ...): %s\n",
		name, pmErrStr(nchn));
	exit(1);
    }

    /*
     * one trip to emit root [plus any prefix parts] for name
     */
    if (root) {
	printf("root {\n");
	for (p = name; *p; p++) {
	    if (p == name || *p == '.') {
		/*
		 * name: sample.foo.bar
		 *             ^   ^
		 *             p   c2
		 */
		char	*c2;
		if (p > name)
		    *p = '\0';
		c2 = strchr(&p[1], '.');
		if (p > name)
		    *p = '.';
		if (c2 != NULL)
		    *c2 = '\0';
		if (p > name)
		    printf("    %s\n", &p[1]);
		else
		    printf("    %s\n", name);
		printf("}\n");
		printf("%s {\n", name);
		if (c2 != NULL)
		    *c2 = '.';
	    }
	}
    }
    else
	printf("%s {\n", name);

    for (i = 0; i < nchn; i++) {
	fullname = mkname(name, offspring[i]);
	if (status[i] == PMNS_LEAF_STATUS) {
	    char	pbuf[20];
	    if ((sts = pmLookupName(1, (const char **)&fullname, &pmid)) < 0) {
		fprintf(stderr, "Error: pmLookupName(..., %s, ...): %s\n",
			offspring[i], pmErrStr(sts));
		exit(1);
	    }
	    pbuf[0] = '\0';
	    pmIDStr_r(pmid, pbuf, sizeof(pbuf));
	    for (p = pbuf; *p; p++) {
		if (*p == '.') *p = ':';
	    }
	    printf("    %s %s\n", offspring[i], pbuf);
	}
	else {
	    printf("    %s\n", offspring[i]);
	    /*
	     * breadth-first search, so add this one to the tail of the
	     * queue
	     */
	    list_t	*new = malloc(sizeof(*new));
	    if (new == NULL) {
		fprintf(stderr, "Error: malloc failed\n");
		exit(1);
	    }
	    new->next = NULL;
	    if ((new->name = strdup(fullname)) == NULL) {
		fprintf(stderr, "Error: strdup failed\n");
		exit(1);
	    }
	    if (head == NULL)
		head = tail = new;
	    else {
		tail->next = new;
		tail = new;
	    }
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "new " PRINTF_P_PFX "%p name %s head " PRINTF_P_PFX "%p tail " PRINTF_P_PFX "%p\n", new, new->name, head, tail);
	}
    }

    printf("}\n");
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.narchives == 1) {
	if ((ctx = pmNewContext(PM_CONTEXT_ARCHIVE, opts.archives[0])) < 0) {
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), opts.archives[0],  pmErrStr(ctx));
	    exit(1);
	}
	if ((sts = pmGetContextOptions(ctx, &opts)) < 0) {
	    pmflush();
	    fprintf(stderr, "%s: pmGetContextOptions(%d, ...) failed: %s\n",
			pmGetProgname(), pmWhichContext(), pmErrStr(sts));
		exit(1);
	}
    }
    else if (opts.narchives > 0) {
	fprintf(stderr, "%s: at most one archive allowed\n", pmGetProgname());
	exit(1);
    }

    if (opts.nhosts == 1) {
	if ((ctx = pmNewContext(PM_CONTEXT_HOST, opts.hosts[0])) < 0) {
	    fprintf(stderr, "%s: Cannot connect to pmcd on host \"%s\": %s\n",
		    pmGetProgname(), opts.hosts[0],  pmErrStr(ctx));
	    exit(1);
	}
    }
    else if (opts.nhosts > 0) {
	fprintf(stderr, "%s: at most one host allowed\n", pmGetProgname());
	exit(1);
    }

    if (opts.errors || \
        (ctx == -1 && opts.optind != argc-1) ||
        (ctx >= 0 && opts.optind != argc)) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (ctx == -1) {
	if ((ctx = pmNewContext(PM_CONTEXT_HOST, "local:")) < 0) {
	    fprintf(stderr, "%s: Cannot connect to pmcd on host \"local:\": %s\n",
		    pmGetProgname(), pmErrStr(ctx));
	    exit(1);
	}
    }

    /* initial preamble and first name */
    dochn(argv[opts.optind], 1);

    /*
     * breadth-first traversal of all non-leaf nodes in the PMNS
     * below the first name
     */
    while (head != NULL) {
	list_t	*next;
	dochn(head->name, 0);
	next = head->next;
	free(head->name);
	free(head);
	head = next;
    }

    return 0;
}
