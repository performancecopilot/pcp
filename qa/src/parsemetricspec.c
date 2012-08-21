#include <stdio.h>
#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    int			isarch;
    char		*msg;
    pmMetricSpec	*rslt;
    int			sts;
    int			i;

    if (argc != 4) {
	fprintf(stderr, "Usage: parsemetricspec spec isarch host\n");
	exit(1);
    }

    if (strcmp(argv[1], "NULL") == 0) argv[1] = NULL;
    isarch = atol(argv[2]);
    if (strcmp(argv[3], "NULL") == 0) argv[3] = NULL;

    printf("pmParseMetricSpec(\"%s\", %d, \"%s\", ...)\n",
	argv[1], isarch, argv[3]);
    
    sts = pmParseMetricSpec(argv[1], isarch, argv[3], &rslt, &msg);

    if (sts < 0) {
	if (sts == PM_ERR_GENERIC)
	    printf("pmParseMetricSpec Error:\n%s\n", msg);
	else
	    printf("error:    %s\n", pmErrStr(sts));
    }
    else {
	printf("isarch:   %d\n", rslt->isarch);
	printf("source:   \"%s\"\n", rslt->source);
	printf("metric:   \"%s\"\n", rslt->metric);
	for (i = 0; i < rslt->ninst; i++) {
	    printf("inst[%d]:  \"%s\"\n", i, rslt->inst[i]);
	}
    }

    exit(0);
}
