#include <time.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

int __pmParseTime(
    const char	    *string,	/* string to be parsed */
    struct timeval  *logStart,	/* start of log or current time */
    struct timeval  *logEnd,	/* end of log or tv_sec == INT_MAX */
				/* assumes sizeof(t_time) == sizeof(int) */
    struct timeval  *rslt,	/* if parsed ok, result filled in */
    char	    **errMsg);	/* error message, please free */

void
set_tm (struct timeval *ntv, struct tm *ntm, struct tm *btm, int mon, int mday, int hour, int min)
{
  memcpy (ntm, btm, sizeof(struct tm));
  if (mon > 0)
    ntm->tm_mon = mon;
  if (mday > 0)
    ntm->tm_mday = mday;
  if (hour > 0)
    ntm->tm_hour = hour;
  if (min > 0)
    ntm->tm_min = min;

  if (ntv != NULL) {
    ntv->tv_sec = mktime (ntm);
    ntv->tv_usec = 0;
  }
}

void
dump_dt (char *str, struct tm *atm)
{
  int pfx;
  printf ("\"%s\"%n", str, &pfx);
  printf ("%*s", 31-pfx, " ");
  printf ("%d-%.2d-%.2d %.2d:%.2d:%.2d\n", 
	  atm->tm_year + 1900,
	  atm->tm_mon + 1,
	  atm->tm_mday,
	  atm->tm_hour,
	  atm->tm_min,
	  atm->tm_sec);
}

int
main ()
{
  
  struct timeval tvstart;	// .tv_sec .tv_usec
  struct timeval tvend;
  struct timeval tvrslt;
  struct tm tmstart;		// .tm_sec .tm_min .tm_hour .tm_mday .tm_mon .tm_year .tm_wday .tm_yday
  struct tm tmend;
  struct tm tmrslt;
  struct tm tmtmp;
  time_t ttstart;
  char buffer[256];
  char *errmsg = (char*)(&buffer);
  char *tmtmp_str;
 
  ttstart = 1392649730; // time(&ttstart) => time_t
  tvstart.tv_sec = ttstart;
  tvstart.tv_usec = 0;
  localtime_r (&ttstart, &tmstart); // time_t => tm
  set_tm (&tvend, &tmend, &tmstart, 0, 27, 11, 28);
  dump_dt ("start ", &tmstart);
  dump_dt ("end   ", &tmend);

  set_tm (NULL, &tmtmp, &tmstart, 0, 19, 11, 45);
  tmtmp_str = asctime(&tmtmp);
  char *tmtmp_c = strchr (tmtmp_str, '\n');
  *tmtmp_c = ' ';
  __pmParseTime (tmtmp_str, &tvstart, &tvend, &tvrslt, &errmsg);
  localtime_r (&tvrslt.tv_sec, &tmrslt); // time_t => tm
  dump_dt (tmtmp_str, &tmrslt);
  
  // See strftime for a description of the % formats
  char *strftime_fmt [] =  {
    "+1minute",
    "-1minute",
    "%F",
    "%D",
    "%D %r",
    "%D %R",
    "%D %T",
    "%d %b %Y %X",
    "yesterday",
    "next day",
    "1 day ago",
    "1 week ago",
    "@%F",
    "@%D",
    "@%D %r",
    "@%D %R",
    "@%D %T",
    "@%d %b %Y %X",
    "@yesterday",
    "@next day",
    "@1 day ago",
    "1 day",
    "5 minutes 5 seconds"
  };

  int sfx;
  for (sfx = 0; sfx < (sizeof(strftime_fmt) / sizeof(void*)); sfx++) {
    int len = strftime (buffer, sizeof (buffer), strftime_fmt[sfx], &tmtmp);
    if (len != 0)
      {
	__pmParseTime (buffer, &tvstart, &tvend, &tvrslt, &errmsg);
	localtime_r (&tvrslt.tv_sec, &tmrslt); // time_t => tm
	dump_dt (buffer, &tmrslt);
      }
    else
      printf ("strftime format \"%s\" not recognized\n", strftime_fmt[sfx]);
  }
  
  return 0;
}
