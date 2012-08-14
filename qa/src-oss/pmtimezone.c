/*
 * Copyright (c) 2009 Ken McDonell.  All Rights Reserved.
 * 
 * Based on from tz.c in libpcp/src ... use $PCPQA_TZ not $TZ and
 * no squashing and default is EST-10 for everyone ... 8^)>
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

#if !defined(HAVE_UNDERBAR_ENVIRON)
#define _environ environ
#endif

extern char **_environ;

/*
 * __pmTimezone: work out local timezone
 */
char *
__pmTimezone(void)
{
    char *tz = getenv("PCPQA_TZ");

    if (tz == NULL) {
	fprintf(stderr, "__pmTimezone: BOZO $PCPQA_TZ not set, using EST-10\n");
	tz = strdup("EST-10");
    }
    if (strlen(tz) > PM_TZ_MAXLEN) {
	fprintf(stderr, "__pmTimezone: BOZO $PCPQA_TZ (%s) must be shorter than %d chars!\n", tz, PM_TZ_MAXLEN);
	tz[PM_TZ_MAXLEN-1] = '\0';
    }

    fprintf(stderr, "Burglar alert tz=%s\n", tz);

    return tz;
}
