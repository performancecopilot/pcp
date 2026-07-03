/*
 * Verify pmNewZone accepts/rejects timezone strings correctly.
 */

#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    int		sts;

    pmSetProgname(argv[0]);

    if (argc != 2) {
	fprintf(stderr, "Usage: %s timezone\n", pmGetProgname());
	return 1;
    }

    if (argv[1][0] == '\0') {
	printf("pmNewZone(\"\") - skipped (empty string)\n");
	return 0;
    }

    sts = pmNewZone(argv[1]);
    if (sts >= 0)
	printf("pmNewZone(\"%s\") -> %d (accepted)\n", argv[1], sts);
    else
	printf("pmNewZone(\"%s\") -> %s (rejected)\n", argv[1], pmErrStr(sts));

    return 0;
}
