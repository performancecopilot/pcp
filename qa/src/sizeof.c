/*
 * print sizeof() some common types
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>

int
main(int argc, char **argv)
{
    while (argc > 1) {
	if (strcmp(argv[1], "char") == 0)
	    printf("char=%zd\n", sizeof(char));
	else if (strcmp(argv[1], "short") == 0)
	    printf("short=%zd\n", sizeof(short));
	else if (strcmp(argv[1], "int") == 0)
	    printf("int=%zd\n", sizeof(int));
	else if (strcmp(argv[1], "long") == 0)
	    printf("long=%zd\n", sizeof(long));
	else if (strcmp(argv[1], "long long") == 0)
	    printf("long long=%zd\n", sizeof(long long));
	else if (strcmp(argv[1], "float") == 0)
	    printf("float=%zd\n", sizeof(float));
	else if (strcmp(argv[1], "double") == 0)
	    printf("double=%zd\n", sizeof(long));
	else if (strcmp(argv[1], "ptr") == 0)
	    printf("ptr=%zd\n", sizeof(void *));
	else {
	    printf("Botch: don't grok type \"%s\"\n", argv[1]);
	    return 1;
	}
	argc--;
	argv++;
    }

    return 0;
}
