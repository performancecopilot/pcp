/*
 * for PV 939998 affecting pmgadgets instance cache
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static int
matchInstanceName(char *p, char *q)
{
    for (;; p++, q++) {
	if ((*p == ' ' || *p == '\0') &&
	    (*q == ' ' || *q == '\0'))
	    return 1;

	if (*p != *q || *p == ' ' || *p == '\0' ||
	    *q == ' ' || *q == '\0')
	    return 0;
    }
}

int
main(int argc, char *argv[])
{
    char *a = argv[1];
    char *b = argv[2];

    if (argc != 3) {
	fprintf(stderr, "Usage: %s string1 string2\n", argv[0]);
	exit(1);
    }

    printf("\"%s\" \"%s\" %s\n", a, b,
	    matchInstanceName(a, b) ? "true" : "false");
    exit(0);
}
