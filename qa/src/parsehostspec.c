#include <stdio.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    char		*msg, buffer[512];
    pmHostSpec		*hosts;
    int			count, sts, i, j;

    if (argc != 2) {
	fprintf(stderr, "Usage: parsehostspec spec\n");
	exit(1);
    }

    printf("pmParseHostSpec(\"%s\", ...)\n", argv[1]);
    sts = __pmParseHostSpec(argv[1], &hosts, &count, &msg);
    if (sts < 0) {
	if (sts == PM_ERR_GENERIC)
	    printf("pmParseHostSpec error:\n%s\n", msg);
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

    sts = __pmUnparseHostSpec(hosts, count, buffer, sizeof(buffer));
    if (sts < 0) {
	printf("pmUnparseHostSpec: %s\n", pmErrStr(sts));
	exit(1);
    }
    printf("pmUnparseHostSpec(\"%s\") -> \"%s\"\n", argv[1], buffer);

    __pmFreeHostSpec(hosts, count);
    exit(0);
}
