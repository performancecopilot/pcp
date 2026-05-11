/*
 * Adapted from http://oss.sgi.com/bugzilla/show_bug.cgi?id=1068
 *
 * Copyright (c) 2014 Red Hat and Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <stdio.h>

#define MYSIZE_BAD 2
#define MYSIZE_OK 1024

int
main()
{
    char	*buffer;
    pmUnits	foo = { .dimSpace=-1, .dimTime=-1, .dimCount=1,
		        .scaleSpace=PM_SPACE_TBYTE, .scaleTime=PM_TIME_USEC,
		        .scaleCount=7
		      };
    pmUnits	extrafoo = { .dimSpace=-1, .dimTime=-1, .dimCount=1,
		        .scaleSpace=PM_SPACE_TBYTE, .scaleTime=PM_TIME_USEC,
		        .scaleCount=7, .extraUnit = PM_UNITS_TEMPERATURE,
			.extraScale = PM_TEMPERATURE_K,
		      };
    char	*result;

    buffer = (char *)malloc(MYSIZE_OK);
    buffer[0] = '\0';
    result = pmUnitsStr_r(&foo, buffer, MYSIZE_OK);
    buffer[MYSIZE_OK-1] = '\0';
    if (result != NULL)
	printf ("good one: %s\n", result);
    else
	printf ("good one: return NULL\n");
    printf ("buffer: %s\n", buffer);
    buffer[0] = '\0';
    result = pmUnitsStr_r(&extrafoo, buffer, MYSIZE_OK);
    buffer[MYSIZE_OK-1] = '\0';
    if (result != NULL)
	printf ("good extra one: %s\n", result);
    else
	printf ("good extra one: return NULL\n");
    printf ("buffer: %s\n", buffer);
    free(buffer);

    buffer = (char *)malloc(MYSIZE_BAD);
    buffer[0] = '\0';
    result = pmUnitsStr_r(&foo, buffer, MYSIZE_BAD);
    buffer[MYSIZE_BAD-1] = '\0';
    if (result != NULL)
	printf ("bad one: %s\n", result);
    else
	printf ("bad one: return NULL\n");
    printf ("buffer: %s\n", buffer);
    buffer[0] = '\0';
    result = pmUnitsStr_r(&extrafoo, buffer, MYSIZE_BAD);
    buffer[MYSIZE_BAD-1] = '\0';
    if (result != NULL)
	printf ("bad extra one: %s\n", result);
    else
	printf ("bad extra one: return NULL\n");
    printf ("buffer: %s\n", buffer);
    free(buffer);

    return 0;
}
