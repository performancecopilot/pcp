#include <stdio.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,	/* -D */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:",
    .long_options = longopts,
    .short_usage = "[...] spec",
};


static __pmHashWalkState
print_attribute(const __pmHashNode *tp, void *cp)
{
    char buffer[256];

    if (!__pmAttrStr_r(tp->key, tp->data, buffer, sizeof(buffer))) {
	fprintf(stderr, "Found unrecognised attribute (%d: \"%s\")\n",
		tp->key, tp->data ? (char *)tp->data : "");
    }
    buffer[sizeof(buffer)-1] = '\0';
    printf("%s\n", buffer);
    return PM_HASH_WALK_NEXT;
}

int
main(int argc, char **argv)
{
    char		*msg;
    char		buffer[512];
    __pmHashCtl		attrs;
    __pmHostSpec	*hosts;
    int			count, sts, i, j;
    int			c;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.errors || opts.optind != argc-1) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    __pmHashInit(&attrs);
    printf("pmParseHostAttrsSpec(\"%s\", ...)\n", argv[opts.optind]);
    sts = __pmParseHostAttrsSpec(argv[opts.optind], &hosts, &count, &attrs, &msg);
    if (sts < 0) {
	if (sts == PM_ERR_GENERIC)
	    printf("pmParseHostAttrsSpec error:\n%s\n", msg);
	else
	    printf("Error: %s\n", pmErrStr(sts));
	exit(1);
    }
    for (i = 0; i < count; i++) {
	printf("host[%d]: \"%s\"", i, hosts[i].name);
	if (hosts[i].nports == 1)
	    printf(" port:");
	else if (hosts[i].nports > 1)
	    printf(" ports:");
	for (j = 0; j < hosts[i].nports; j++)
	    printf(" %d", hosts[i].ports[j]);
	putchar('\n');
    }
    __pmHashWalkCB(print_attribute, NULL, &attrs);

    sts = __pmUnparseHostAttrsSpec(hosts, count, &attrs, buffer, sizeof(buffer));
    if (sts < 0) {
	printf("pmUnparseHostAttrsSpec: %s\n", pmErrStr(sts));
	exit(1);
    }
    printf("pmUnparseHostAttrsSpec(\"%s\") -> \"%s\"\n", argv[opts.optind], buffer);

    __pmFreeHostAttrsSpec(hosts, count, &attrs);
    __pmHashClear(&attrs);
    exit(0);
}
