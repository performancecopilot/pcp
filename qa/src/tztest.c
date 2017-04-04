#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

char * __pmTimezone(void);

int main()
{
    time_t sept = (30*365.25+244)*24*3600; /* 1 Septemnber 2000 */
    time_t march = (30*365.25+60)*24*3600; /* 1 March 2000 */

    /* Two sets of timezone strings - one works for the first half
     * of the year, another for the second. Make sure that in the case
     * where we cannot reduce the timezone string and have to re-write it
     * same timezone is choosen */
    char *zones[][8] = {
    {
	"ABC-10",
	"ABC-10:01:02XYZ-11:03:04,M12.5.0/3:04:05,M7.1.0/2:06:07",
	"ABC-10:00:00XYZ-11:00:00,M12.5.0/3:00:00,M7.5.0/2:00:00",
	"ABC-10XYZ-11,M12.5.0/3:00:00,M7.1.0/2:00:00",
	"ABC-10XYZ-11,M12.5.0/3:01:02,M7.1.0/2:03:04",
	"ABC-10",
	":Someplace/Somewhere",
	NULL
    },
    {
	"ABC-10",
	"XYZ-11:03:04ABC-10:01:02,M12.5.0/3:04:05,M7.1.0/2:06:07",
	"ABC-10:00:00XYZ-11:00:00,M12.5.0/3:00:00,M7.5.0/2:00:00",
	"ABC-10XYZ-11,M12.5.0/3:00:00,M7.1.0/2:00:00",
	"XYZ-11ABC-10,M12.5.0/3:01:02,M7.1.0/2:03:04",
	"ABC-10",
	":Someplace/Somewhere",
	NULL
    }};
    char * tz = getenv("TZ");

    if (tz == NULL) {
	puts("Timezone is not set, abort the test");
    } else {
	int i;
	time_t now = time(NULL);
	struct tm * today = localtime(&now);
	int which = today->tm_mon / 6;
	char tb[256];
	char * newtz = __pmTimezone();

	printf("%s -> %s\n", tz, newtz);

	for (i=0; zones[which][i] != NULL; i++) {
	    char * tz;
	    struct tm  *tmp;
	    char tstr[64];
	    int dst;

	    sprintf(tb, "TZ=%s", zones[which][i]);
	    putenv(tb);
	    tzset();
	    tz = getenv("TZ");
            newtz = __pmTimezone();

            printf("%s -> %s\n", tz, newtz);
	    tmp = localtime(&march);
	    dst = tmp->tm_isdst;

            strftime(tstr, 64, "%d %B %Y %H:%M %Z", tmp);
	    printf("In March daylight saving is %s, and the time is %s\n",
		(dst ? "on" : "off"), tstr);
	    tmp = localtime(&sept);
	    dst = tmp->tm_isdst;
            strftime(tstr, 64, "%d %B %Y %H:%M %Z", tmp);
	    printf("In September daylight saving is %s, and the time is %s\n\n",
		(dst ? "on" : "off"), tstr);
	}
    }
    
    return 0;
}
