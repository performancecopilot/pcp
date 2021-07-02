#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

int main()
{
    time_t	sep;
    time_t	mar;
    struct tm	init;

    /* Test timezone strings.
     * Make sure that in the case where we cannot reduce the timezone
     * string and have to re-write it the same timezone offset
     * is choosen
     */
    char *zones[] = {
	"ABC-10",
	"ABC-10:01:02XYZ-11:03:04,M12.5.0/3:04:05,M7.1.0/2:06:07",
	"ABC-10:00:00XYZ-11:00:00,M12.5.0/3:00:00,M7.5.0/2:00:00",
	"ABC-10XYZ-11,M12.5.0/3:00:00,M7.1.0/2:00:00",
	"ABC-10XYZ-11,M12.5.0/3:01:02,M7.1.0/2:03:04",
	"ABC-10",
	":Someplace/Somewhere",
	NULL
    };
    char * tz = getenv("TZ");

    putenv("TZ=UTC");
    /* mar == 12:00:00 1 Mar 2000 UTC */
    memset(&init, 0, sizeof(init));
    init.tm_hour = 12;
    init.tm_mday = 1;
    init.tm_mon = 2;
    init.tm_year = 2000 - 1900;
    mar = mktime(&init);

    /* sep == 12:00:00 1 Sep 2000 UTC */
    memset(&init, 0, sizeof(init));
    init.tm_hour = 12;
    init.tm_mday = 1;
    init.tm_mon = 8;
    init.tm_year = 2000 - 1900;
    sep = mktime(&init);

    if (tz == NULL) {
	printf("Debug: Mar time_t %ld -> %s", (long)mar, ctime(&mar));
	printf("Debug: Sep time_t %ld -> %s", (long)sep, ctime(&sep));
	puts("Timezone is not set, abort the test");
    } else {
	int		i;
	char		tb[256];
	char		*newtz;

	/* restore $TZ from environment at the start of main() */
	pmsprintf(tb, sizeof(tb), "TZ=%s", tz);
	putenv(tb);

	newtz = __pmTimezone();

	printf("%s -> %s\n", tz, newtz);

	for (i=0; zones[i] != NULL; i++) {
	    char * tz;
	    struct tm  *tmp;
	    char tstr[64];
	    int dst;

	    pmsprintf(tb, sizeof(tb), "TZ=%s", zones[i]);
	    putenv(tb);
	    tzset();
	    tz = getenv("TZ");
            newtz = __pmTimezone();

            printf("%s -> %s\n", tz, newtz);
	    tmp = localtime(&mar);
	    dst = tmp->tm_isdst;

            strftime(tstr, 64, "%d %B %Y %H:%M %Z", tmp);
	    printf("In March daylight saving is %s, and the time is %s\n",
		(dst ? "on" : "off"), tstr);
	    tmp = localtime(&sep);
	    dst = tmp->tm_isdst;
            strftime(tstr, 64, "%d %B %Y %H:%M %Z", tmp);
	    printf("In September daylight saving is %s, and the time is %s\n\n",
		(dst ? "on" : "off"), tstr);
	}
    }
    
    return 0;
}
