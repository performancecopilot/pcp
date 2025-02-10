/*
** ATOP - System & Process Monitor
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** This source-file contains various functions to a.o. format the
** time-of-day, the cpu-time consumption and the memory-occupation. 
**
** Copyright (C) 2000-2010 Gerlof Langeveld
** Copyright (C) 2015-2021 Red Hat.
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
*/

#include <pcp/pmapi.h>
#include <pcp/pmafm.h>
#include <pcp/libpcp.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

#include "atop.h"
#include "photoproc.h"
#include "photosyst.h"
#include "hostmetrics.h"
#include "gpucom.h"

extern const char *hostmetrics[];
extern const char *ifpropmetrics[];
extern const char *systmetrics[];
extern const char *procmetrics[];

/*
** Add the PCP long option and environment variable handling into
** the mix, along with regular atop short option handling, ready
** for subsequent (combined) pmgetopt_r processing.
*/
void
setup_options(pmOptions *opts, char **argv, char *short_options)
{
	pmLongOptions	*opt;
	static pmLongOptions long_options[] =
	{
		{ "hotproc", 0, 0, 0, "use the pmdaproc(1) hotproc metrics" },
		PMOPT_ALIGN,
		PMOPT_ARCHIVE,
		PMOPT_DEBUG,
		PMOPT_HOST,
		PMOPT_START,
		PMOPT_SAMPLES,
		PMOPT_FINISH,
		PMOPT_INTERVAL,
		PMOPT_TIMEZONE,
		PMOPT_HOSTZONE,
		PMOPT_VERSION,
		PMAPI_OPTIONS_END
	};

	pmSetProgname(argv[0]);

	memset(opts, 0, sizeof *opts);
	opts->flags = PM_OPTFLAG_BOUNDARIES;
	opts->long_options = long_options;
	opts->short_options = short_options;

	/* only perform long option handling for PCP args */
	for (opt = &long_options[0]; opt->long_opt; opt++)
		opt->short_opt = 0;

	__pmStartOptions(opts);
}

void
close_options(pmOptions *opts)
{
	__pmEndOptions(opts);

	if (opts->errors)
		pmflush();
}

/*
** Function convtime() converts a value (number of seconds since
** 1-1-1970) to an ascii-string in the format hh:mm:ss, stored in
** chartim (9 bytes long).
*/
char *
convtime(double timed, char *chartim, size_t buflen)
{
	time_t		utime = (time_t) timed;
	struct tm 	tt;

	pmLocaltime(&utime, &tt);
	pmsprintf(chartim, buflen, "%02d:%02d:%02d",
		tt.tm_hour, tt.tm_min, tt.tm_sec);
	return chartim;
}

/*
** Function convdate() converts a value (number of seconds since
** 1-1-1970) to an ascii-string in the format yyyy/mm/dd, stored in
** chardat (11 bytes long).
*/
char *
convdate(double timed, char *chardat, size_t buflen)
{
	time_t		utime = (time_t) timed;
	struct tm 	tt;

	pmLocaltime(&utime, &tt);
	pmsprintf(chardat, buflen, "%04d/%02d/%02d",
		tt.tm_year+1900, tt.tm_mon+1, tt.tm_mday);
	return chardat;
}


/*
** Convert a string in format [YYYYMMDD]hh:]mm[:][ss] into an epoch time value or
** when only the value hh[:]mm was given, take this time from midnight.
**
** Arguments:           String with date-time in format [YYYYMMDD]hh[:]mm[:][ss]
**                      or hh[:]mm[:][ss].
**
**                      Pointer to time_t containing 0 or current epoch time.
**
** Return-value:	0 - Wrong input-format
**			1 - Success
*/
int
getbranchtime(char *itim, struct timeval *newtime)
{
	register int	ilen = strlen(itim);
	int		hours, minutes, seconds = 0;
	time_t		epoch;
	struct tm	tm;

	memset(&tm, 0, sizeof tm);

	/*
	** verify length of input string
	*/
	if (ilen != 4 && ilen != 5 &&   // hhmm or hh:mm
	    ilen != 6 && ilen != 8 &&   // hhmmss or hh:mm:ss
	    ilen != 12 && ilen != 13 && // YYYYMMDDhhmm or YYYYMMDDhh:mm
	    ilen != 14 && ilen != 16)   // YYYYMMDDhhmmss or YYYYMMDDhh:mm:ss
	        return 0;               // wrong date-time format

	/*
	** check string syntax for absolute time specified as
	** YYYYMMDDhh:mm:ss or YYYYMMDDhhmmss
	*/
	if ( sscanf(itim, "%4d%2d%2d%2d:%2d:%2d", &tm.tm_year, &tm.tm_mon,
	                        &tm.tm_mday,  &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6 ||
	     sscanf(itim, "%4d%2d%2d%2d%2d%2d",  &tm.tm_year, &tm.tm_mon,
	                        &tm.tm_mday,  &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6   )
	{
	        tm.tm_year -= 1900;
	        tm.tm_mon  -= 1;

	        if (tm.tm_year < 100 || tm.tm_mon  < 0  || tm.tm_mon > 11 ||
	            tm.tm_mday < 1   || tm.tm_mday > 31 ||
	            tm.tm_hour < 0   || tm.tm_hour > 23 ||
	            tm.tm_min  < 0   || tm.tm_min  > 59 ||
	            tm.tm_sec  < 0   || tm.tm_sec  > 59   )
	        {
	                return 0;       // wrong date-time format
	        }

	        tm.tm_isdst = -1;

	        if ((epoch = mktime(&tm)) == -1)
	                return 0;       // wrong date-time format

	        // correct date-time format
		newtime->tv_sec = epoch;
		newtime->tv_usec = 0;
	        return 1;
	}

	/*
	** check string syntax for absolute time specified as
	** YYYYMMDDhh:mm or YYYYMMDDhhmm
	*/
	if ( sscanf(itim, "%4d%2d%2d%2d:%2d", &tm.tm_year, &tm.tm_mon,
				&tm.tm_mday,  &tm.tm_hour, &tm.tm_min) == 5 ||
	     sscanf(itim, "%4d%2d%2d%2d%2d",  &tm.tm_year, &tm.tm_mon,
				&tm.tm_mday,  &tm.tm_hour, &tm.tm_min) == 5   )
	{
		tm.tm_year -= 1900;
		tm.tm_mon  -= 1;

		if (tm.tm_year < 100 || tm.tm_mon  < 0  || tm.tm_mon > 11 ||
                    tm.tm_mday < 1   || tm.tm_mday > 31 || 
		    tm.tm_hour < 0   || tm.tm_hour > 23 ||
		    tm.tm_min  < 0   || tm.tm_min  > 59   )
		{
			return 0;	// wrong date-time format
		}

		tm.tm_isdst = -1;

		if ((epoch = __pmMktime(&tm)) == -1)
			return 0;	// wrong date-time format

		// correct date-time format
		newtime->tv_sec = epoch;
		newtime->tv_usec = 0;
		return 1;
	}

	/*
	** check string syntax for relative time specified as
	** hh:mm:ss or hhmmss
	*/
	if ( sscanf(itim, "%2d:%2d:%2d", &hours, &minutes, &seconds) == 3 ||
	     sscanf(itim, "%2d%2d%2d",  &hours, &minutes, &seconds) == 3 )
	{
	        if ( hours < 0 || hours > 23 ||
	             minutes < 0 || minutes > 59 ||
	             seconds < 0 || seconds > 59 )
	                return 0;       // wrong date-time format

	        /*
	        ** when the new time is already filled with an epoch time,
	        ** the relative time will be on the same day as indicated by
	        ** that epoch time
	        ** when the new time is the time within a day or 0, the new
	        ** time will be stored again as the time within a day.
	        */
	        if (newtime->tv_sec <= SECONDSINDAY)   // time within the day?
	        {
	                newtime->tv_sec = (hours * 3600) + (minutes * 60) + seconds;

	                if (newtime->tv_sec >= SECONDSINDAY)
	                        newtime->tv_sec = SECONDSINDAY-1;

	                return 1;
	        }
	        else
	        {
			newtime->tv_sec = normalize_epoch(newtime->tv_sec,
	                                (hours*3600) + (minutes*60) + seconds);
	                return 1;
	        }
	}

	/*
	** check string syntax for relative time specified as
	** hh:mm or hhmm or hh:mm:ss or hhmmss
	*/

	if ( sscanf(itim, "%2d:%2d", &hours, &minutes) == 2     ||
	     sscanf(itim, "%2d%2d",  &hours, &minutes) == 2     ||
	     sscanf(itim, "%2d%2d%2d",  &hours, &minutes, &seconds) == 3  ||
	     sscanf(itim, "%2d:%2d:%2d",  &hours, &minutes, &seconds) == 3 )
	{
		if ( hours < 0 || hours > 23 || minutes < 0 || minutes > 59 )
			return 0;	// wrong date-time format
		if ( seconds < 0 || seconds > 59 )
			return 0;	// wrong date-time format

		newtime->tv_usec = 0;

		/*
		** when the new time is already filled with an epoch time,
		** the relative time will be on the same day as indicated by
		** that epoch time
		** when the new time is the time within a day or 0, the new
		** time will be stored again as the time within a day.
		*/
		if (newtime->tv_sec <= SECONDSINDAY)	// time within the day?
		{
			newtime->tv_sec = (hours * 3600) + (minutes * 60) + seconds;

			if (newtime->tv_sec >= SECONDSINDAY)
				newtime->tv_sec = SECONDSINDAY-1;
			return 1;
		}
		else
		{
			newtime->tv_sec = normalize_epoch(newtime->tv_sec,
					(hours*3600) + (minutes*60) + seconds);
			return 1;
		}
	}

	return 0;       // wrong date-time format
}

/*
** Normalize an epoch time with the number of seconds within a day
** Return-value:	Normalized epoch 
*/
time_t
normalize_epoch(time_t epoch, long secondsinday)
{
	struct tm	tm;

	pmLocaltime(&epoch, &tm);	// convert epoch to tm

	tm.tm_hour   = 0;
	tm.tm_min    = 0;
	tm.tm_sec    = secondsinday;
	tm.tm_isdst  = -1;

	return __pmMktime(&tm);		// convert tm to epoch
}

/*
** Function val2valstr() converts a positive value to an ascii-string of a 
** fixed number of positions; if the value does not fit, it will be formatted
** to exponent-notation (as short as possible, so not via the standard printf-
** formatters %f or %e). The offered buffer should have a length of width+1.
** The value might even be printed as an average for the interval-time.
*/
char *
val2valstr(count_t value, char *strvalue, size_t buflen, int width, int avg, int nsecs)
{
	count_t	maxval, remain = 0;
	int	exp     = 0;
	char	*suffix = "";

	if (avg && nsecs)
	{
		value  = (value + (nsecs/2)) / nsecs;     /* rounded value */
		width  = width - 2;	/* subtract two positions for '/s' */
		suffix = "/s";
	}

	if (value < 0)		// no negative value expected
	{
		pmsprintf(strvalue, buflen, "%*s%s", width, "?", suffix);
		return strvalue;
	}

	maxval = pow(10.0, width) - 1;

	if (value < maxval)
	{
		pmsprintf(strvalue, buflen, "%*lld%s", width, value, suffix);
	}
	else
	{
		if (width < 3)
		{
			/*
			** cannot avoid ignoring width
			*/
			pmsprintf(strvalue, buflen, "%lld%s", value, suffix);
		}
		else
		{
			/*
			** calculate new maximum value for the string,
			** calculating space for 'e' (exponent) + one digit
			*/
			width -= 2;
			maxval = pow(10.0, width) - 1;

			while (value > maxval)
			{
				exp++;
				remain = value % 10;
				value /= 10;
			}

			if (remain >= 5 && value < maxval)
				value++;

			pmsprintf(strvalue, buflen, "%*llde%d%s",
					width, value, exp, suffix);
		}
	}

	return strvalue;
}

#define DAYSECS 	(24*60*60)
#define HOURSECS	(60*60)
#define MINSECS 	(60)

/*
** Function val2elapstr() converts a value (number of seconds)
** to an ascii-string of up to max 13 positions in NNNdNNhNNmNNs
** stored in strvalue (at least 14 positions).
** returnvalue: number of bytes stored
*/
int
val2elapstr(int value, char *strvalue, size_t buflen)
{
        char	*p=strvalue;
	int	bytes, offset=0;

        if (value >= DAYSECS) 
        {
		bytes = pmsprintf(p, buflen-offset, "%dd", value/DAYSECS);
		p += bytes;
		offset += bytes;
        }

        if (value >= HOURSECS) 
        {
		bytes = pmsprintf(p, buflen-offset, "%dh", (value%DAYSECS)/HOURSECS);
		p += bytes;
		offset += bytes;
        }

        if (value >= MINSECS) 
        {
                bytes = pmsprintf(p, buflen-offset, "%dm", (value%HOURSECS)/MINSECS);
		p += bytes;
		offset += bytes;
        }

	bytes = pmsprintf(p, buflen-offset, "%ds", (value%MINSECS));
	offset += bytes;

        return offset;
}


/*
** Function val2cpustr() converts a value (number of milliseconds)
** to an ascii-string of 6 positions in milliseconds or minute-seconds or
** hours-minutes, stored in strvalue (at least 7 positions).
*/
#define	MAXMSEC		(count_t)100000
#define	MAXSEC		(count_t)6000
#define	MAXMIN		(count_t)6000

char *
val2cpustr(count_t value, char *strvalue, size_t buflen)
{
	if (value < MAXMSEC)
	{
		pmsprintf(strvalue, buflen, "%2lld.%02llds", value/1000, value%1000/10);
	}
	else
	{
	        /*
       	 	** millisecs irrelevant; round to seconds
       	 	*/
        	value = (value + 500) / 1000;

        	if (value < MAXSEC) 
        	{
               	 	pmsprintf(strvalue, buflen, "%2lldm%02llds", value/60, value%60);
		}
		else
		{
			/*
			** seconds irrelevant; round to minutes
			*/
			value = (value + 30) / 60;

			if (value < MAXMIN) 
			{
				pmsprintf(strvalue, buflen, "%2lldh%02lldm",
							value/60, value%60);
			}
			else
			{
				/*
				** minutes irrelevant; round to hours
				*/
				value = (value + 30) / 60;

				pmsprintf(strvalue, buflen, "%2lldd%02lldh",
						value/24, value%24);
			}
		}
	}

	return strvalue;
}

/*
** Function val2Hzstr() converts a value (in MHz) 
** to an ascii-string.
** The result-string is placed in the area pointed to strvalue,
** which should be able to contain 7 positions plus null byte.
*/
char *
val2Hzstr(count_t value, char *strvalue, size_t buflen)
{
	char *fformat;

        if (value < 1000)
        {
                pmsprintf(strvalue, buflen, "%4lldMHz", value);
        }
        else
        {
                double fval=value/1000.0;      // fval is double in GHz
                char prefix='G';

                if (fval >= 1000.0)            // prepare for the future
                {
                        prefix='T';        
                        fval /= 1000.0;
                }

                if (fval < 10.0)
		{
			 fformat = "%4.2f%cHz";
		}
		else
		{
			if (fval < 100.0)
				fformat = "%4.1f%cHz";
                	else
				fformat = "%4.0f%cHz";
		}

                pmsprintf(strvalue, buflen, fformat, fval, prefix);
        }

	return strvalue;
}


/*
** Function val2memstr() converts a value (number of bytes)
** to an ascii-string in a specific format (indicated by pformat).
** The result-string is placed in the area pointed to strvalue,
** which should be able to contain at least 7 positions.
*/
#define	ONEKBYTE	1024
#define	ONEMBYTE	1048576
#define	ONEGBYTE	1073741824L
#define	ONETBYTE	1099511627776LL
#define	ONEPBYTE	1125899906842624LL

#define	MAXBYTE		999
#define	MAXKBYTE	ONEKBYTE*999L
#define	MAXKBYTE9	ONEKBYTE*9L
#define	MAXMBYTE	ONEMBYTE*999L
#define	MAXMBYTE9	ONEMBYTE*9L
#define	MAXGBYTE	ONEGBYTE*999LL
#define	MAXGBYTE9	ONEGBYTE*9LL
#define	MAXTBYTE	ONETBYTE*999LL
#define	MAXTBYTE9	ONETBYTE*9LL
#define	MAXPBYTE9	ONEPBYTE*9LL


char *
val2memstr(count_t value, char *strvalue, size_t buflen, int pformat, int avgval, int nsecs)
{
	char 	aformat;	/* advised format		*/
	count_t	verifyval;
	char	*suffix = "";
	int	basewidth = 6;


	/*
	** notice that the value can be negative, in which case the
	** modulo-value should be evaluated and an extra position should
	** be reserved for the sign
	*/
	if (value < 0)
		verifyval = -value * 10;
	else
		verifyval =  value;

	/*
	** verify if printed value is required per second (average) or total
	*/
	if (avgval && nsecs)
	{
		value         = llround((double)((double)value/(double)nsecs));
		verifyval     = llround((double)((double)verifyval/(double)nsecs));
		basewidth    -= 2;
		suffix        = "/s";

		if (verifyval <= MAXBYTE)	/* bytes ? */
			aformat = BFORMAT;
		else
			if (verifyval <= MAXKBYTE9)	/* kbytes 1-9 ? */
				aformat = KBFORMAT;
			else
				if (verifyval <= MAXKBYTE)	/* kbytes ? */
					aformat = KBFORMAT_INT;
				else
					if (verifyval <= MAXMBYTE9)	/* mbytes 1-9 ? */
						aformat = MBFORMAT;
					else
						if (verifyval <= MAXMBYTE)	/* mbytes 10-999 ? */
							aformat = MBFORMAT_INT;
						else
							if (verifyval <= MAXGBYTE9)	/* gbytes 1-9 ? */
								aformat = GBFORMAT;
							else
								if (verifyval <= MAXGBYTE)	/* gbytes 10-999 ? */
									aformat = GBFORMAT_INT;
								else
									if (verifyval <= MAXTBYTE9)/* tbytes 1-9 ? */
										aformat = TBFORMAT;/* tbytes! */
									else
										if (verifyval <= MAXTBYTE)/* tbytes 10-999? */
											aformat = TBFORMAT_INT;/* tbytes! */
										else
											if (verifyval <= MAXPBYTE9)/* pbytes 1-9 ? */
												aformat = PBFORMAT;/* pbytes! */
											else
												aformat = PBFORMAT_INT;/* pbytes! */

	} else 
	/*
	** printed value per interval (normal mode) 
	*/
	{
		/*
		** determine which format will be used on bases of the value itself
		*/
		if (verifyval <= MAXBYTE)	/* bytes ? */
			aformat = BFORMAT;
		else
			if (verifyval <= MAXKBYTE)	/* kbytes ? */
				aformat = KBFORMAT;
			else
				if (verifyval <= MAXMBYTE)	/* mbytes ? */
					aformat = MBFORMAT;
				else
					if (verifyval <= MAXGBYTE)	/* gbytes ? */
						aformat = GBFORMAT;
					else
						if (verifyval <= MAXTBYTE)/* tbytes? */
							aformat = TBFORMAT;/* tbytes! */
						else
							aformat = PBFORMAT;/* pbytes! */


	}


	/*
	** check if this is also the preferred format
	*/
	if (aformat <= pformat)
		aformat = pformat;

	switch (aformat)
	{
	   case	BFORMAT:
		pmsprintf(strvalue, buflen, "%*lldB%s",
				basewidth-1, value, suffix);
		break;

	   case	KBFORMAT:
		pmsprintf(strvalue, buflen, "%*.1lfK%s",
			basewidth-1, (double)((double)value/ONEKBYTE), suffix); 
		break;

	   case	KBFORMAT_INT:
		pmsprintf(strvalue, buflen, "%*lldK%s",
				basewidth-1, llround((double)((double)value/ONEKBYTE)), suffix);
		break;

	   case	MBFORMAT:
		pmsprintf(strvalue, buflen, "%*.1lfM%s",
			basewidth-1, (double)((double)value/ONEMBYTE), suffix); 
		break;

	   case	MBFORMAT_INT:
		pmsprintf(strvalue, buflen, "%*lldM%s",
			basewidth-1, llround((double)((double)value/ONEMBYTE)), suffix); 
		break;

	   case	GBFORMAT:
		pmsprintf(strvalue, buflen, "%*.1lfG%s",
			basewidth-1, (double)((double)value/ONEGBYTE), suffix);
		break;

	   case	GBFORMAT_INT:
		pmsprintf(strvalue, buflen, "%*lldG%s",
			basewidth-1, llround((double)((double)value/ONEGBYTE)), suffix);
		break;

	   case	TBFORMAT:
		pmsprintf(strvalue, buflen, "%*.1lfT%s",
			basewidth-1, (double)((double)value/ONETBYTE), suffix);
		break;

	   case	TBFORMAT_INT:
		pmsprintf(strvalue, buflen, "%*lldT%s",
			basewidth-1, llround((double)((double)value/ONETBYTE)), suffix);
		break;

	   case	PBFORMAT:
		pmsprintf(strvalue, buflen, "%*.1lfP%s",
			basewidth-1, (double)((double)value/ONEPBYTE), suffix);
		break;

	   case	PBFORMAT_INT:
		pmsprintf(strvalue, buflen, "%*lldP%s",
			basewidth-1, llround((double)((double)value/ONEPBYTE)), suffix);
		break;

	   default:
		pmsprintf(strvalue, buflen, "!TILT!");
	}

	return strvalue;
}


/*
** Function numeric() checks if the ascii-string contains 
** a numeric (positive) value.
** Returns 1 (true) if so, or 0 (false).
*/
int
numeric(char *ns)
{
	register char *s = ns;

	while (*s)
		if (*s < '0' || *s > '9')
			return(0);		/* false */
		else
			s++;
	return(1);				/* true  */
}

/*
** generic pointer verification after malloc
*/
void
ptrverify(const void *ptr, const char *errormsg, ...)
{
	if (!ptr)
	{
        	va_list args;

		acctswoff();
		netatop_signoff();

		if (vis.show_end)
			(vis.show_end)();

		va_start(args, errormsg);
		vfprintf(stderr, errormsg, args);
        	va_end(args);

		exit(13);
	}
}

/*
 * +** cleanup, give error message and exit
+*/
void
mcleanstop(int exitcode, const char *errormsg, ...)
{
	va_list args;

	acctswoff();
	netatop_signoff();
	(vis.show_end)();

	va_start(args, errormsg);
	vfprintf(stderr, errormsg, args);
	va_end(args);

	exit(exitcode);
}

/*
** cleanup, give error message and exit
*/
void
cleanstop(int exitcode)
{
	acctswoff();
	netatop_signoff();
	(vis.show_end)();

	exit(exitcode);
}

/*
** determine if we are running with root privileges
** returns: boolean
*/
int
rootprivs(void)
{
#ifdef HAVE_GETRESUID
 	uid_t ruid, euid, suid;

	getresuid(&ruid, &euid, &suid);
#else
	uid_t suid = getuid();
#endif

	return !suid;
}

/*
** establish an async timer alarm with microsecond-level precision
*/
void
setalarm(struct timeval *delta)
{
	struct itimerval val;

	val.it_value = *delta;
	val.it_interval.tv_sec = val.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &val, NULL);
}

void
setalarm2(int sec, int usec)
{
	struct timeval delta;

	delta.tv_sec = sec;
	delta.tv_usec = usec;
	setalarm(&delta);
}

void
setup_step_mode(int forward)
{
	const int SECONDS_IN_24_DAYS = 2073600;

	if (!rawreadflag)
		fetchmode = PM_MODE_LIVE;
	else if (forward)
		fetchmode = PM_MODE_FORW;
	else
		fetchmode = PM_MODE_INTERP;

	if (interval.tv_sec > SECONDS_IN_24_DAYS)
	{
		fetchstep = interval.tv_sec;
		fetchmode |= PM_XTB_SET(PM_TIME_SEC);
	}
	else
	{
		fetchstep = interval.tv_sec * 1e3 + interval.tv_usec / 1e3;
		fetchmode |= PM_XTB_SET(PM_TIME_MSEC);
	}
}

/*
** Set the origin position and interval for PMAPI context fetching.
** Enable full-thread proc reporting for all live modes.
*/
static int
setup_origin(pmOptions *opts)
{
	int		sts = 0;

	start = opts->start;
	finish = opts->finish;
	curtime = origin = opts->origin;

	if (opts->interval.tv_sec || opts->interval.tv_usec)
		interval = opts->interval;

	/* initial archive mode, position and delta */
	if (opts->context == PM_CONTEXT_ARCHIVE)
	{
		curtime = start;
		setup_step_mode(1);
		if ((sts = pmSetMode(fetchmode, &curtime, fetchstep)) < 0)
		{
			pmprintf(
		"%s: pmSetMode failure: %s\n", pmGetProgname(), pmErrStr(sts));
			opts->flags |= PM_OPTFLAG_RUNTIME_ERR;
			opts->errors++;
		}
	}
	else
	/* set proc.control.perclient.threads to unsigned integer value 1 */
	{
		pmResult	*result = calloc(1, sizeof(pmResult));
		pmValueSet	*vset = calloc(1, sizeof(pmValueSet));

		ptrverify(vset, "Malloc vset failed for %s enabling\n", "thread");
		ptrverify(result, "Malloc result failed for %s enabling\n", "thread");

		vset->vlist[0].inst = PM_IN_NULL;
		vset->vlist[0].value.lval = 1;
		vset->valfmt = PM_VAL_INSITU;
		vset->numval = 1;
		vset->pmid = pmID_build(3, 10, 2);

		result->vset[0] = vset;
		result->numpmid = 1;

		sts = pmStore(result);
		if (sts < 0 && pmDebugOptions.appl0)
			fprintf(stderr, "%s: pmStore failed: %s\n",
					pmGetProgname(), pmErrStr(sts));
		sts = 0;	/* continue without detailed thread stats */
		pmFreeResult(result);
	}

	return sts;
}

/*
** Function abstime() converts a user-specified time string
** into @-prefixed form that is used by libpcp interfaces.
*/
char *
abstime(char *str)
{
	size_t		length = strlen(str) + 2;
	char		*arg;

	/* length includes @-prefix and a null terminator */
	if ((arg = malloc(length)) == NULL)
		pmNoMem("abstime", length, PM_FATAL_ERR);
	pmsprintf(arg, length, "@%s", str);
	return arg;
}

/*
** PMAPI context creation and initial command line option handling.
*/
static int
setup_context(pmOptions *opts)
{
	char		*source;
	int		sts, ctx;

	if (opts->context == PM_CONTEXT_ARCHIVE)
		source = opts->archives[0];
	else if (opts->context == PM_CONTEXT_HOST)
	{
		source = opts->hosts[0];
		if (strcmp(source, "local:") == 0 ||
		    strcmp(source, "localhost") == 0)
			localhost = 1;
	}
	else if (opts->context == PM_CONTEXT_LOCAL)
	{
		source = NULL;
		localhost = 1;
	}
	else
	{
		opts->context = PM_CONTEXT_HOST;
		source = "local:";
		localhost = 1;
	}

	if ((sts = ctx = pmNewContext(opts->context, source)) < 0)
	{
		if (opts->context == PM_CONTEXT_HOST)
			pmprintf(
		"%s: Cannot connect to pmcd on host \"%s\": %s\n",
				pmGetProgname(), source, pmErrStr(sts));
		else if (opts->context == PM_CONTEXT_LOCAL)
			pmprintf(
		"%s: Cannot make standalone connection on localhost: %s\n",
				pmGetProgname(), pmErrStr(sts));
		else
			pmprintf(
		"%s: Cannot open archive \"%s\": %s\n",
				pmGetProgname(), source, pmErrStr(sts));
	}
	else if ((sts = pmGetContextOptions(ctx, opts)) == 0)
		sts = setup_origin(opts);

	if (sts < 0)
	{
		pmflush();
		cleanstop(1);
	}

	return ctx;
}

void
setup_globals(pmOptions *opts)
{
	pmID		pmids[HOST_NMETRICS];
	pmDesc		descs[HOST_NMETRICS];
	pmResult	*result = NULL;
	char		*p;

	setup_context(opts);
	setup_metrics(hostmetrics, &pmids[0], &descs[0], HOST_NMETRICS);
	fetch_metrics("host", HOST_NMETRICS, pmids, &result);

	if (result == NULL || HOST_NMETRICS != result->numpmid)
	{
		fprintf(stderr,
			"%s: pmFetch failed to fetch initial metric value(s)\n",
			pmGetProgname());
		cleanstop(1);
	}

	setup_photosyst();
	setup_photoproc();
	setup_gpuphotoproc();

	if ((hertz = extract_integer(result, descs, HOST_HERTZ)) <= 0)
		hertz = sysconf(_SC_CLK_TCK);
	if ((pidmax = extract_integer(result, descs, HOST_PID_MAX)) <= 0)
		pidmax = getmaxpid();
	pidwidth = getpidwidth();
	if ((pagesize = extract_integer(result, descs, HOST_PAGESIZE)) <= 0)
		pagesize = getpagesize();
	if ((system_boottime = extract_count_t(result, descs, HOST_BTIME)) <= 0)
		system_boottime = result->timestamp.tv_sec - interval.tv_sec;

	extract_string(result, descs, HOST_RELEASE, sysname.release, sizeof sysname.release);
	sscanf(sysname.release, "%d.%d.%d", &os_rel, &os_vers, &os_sub);
	extract_string(result, descs, HOST_VERSION, sysname.version, sizeof sysname.version);
	extract_string(result, descs, HOST_MACHINE, sysname.machine, sizeof sysname.machine);
	extract_string(result, descs, HOST_NODENAME, sysname.nodename, sizeof sysname.nodename);
	if ((p = strchr(sysname.nodename, '.')))
		*p = '\0';	/* without domain-name */
	nodenamelen = strlen(sysname.nodename);

	/* default hardware inventory - used as fallbacks only if other metrics missing */
	if ((hinv_nrcpus = extract_integer(result, descs, NRCPUS)) <= 0)
		hinv_nrcpus = 1;
	if ((hinv_nrgpus = extract_integer(result, descs, NRGPUS)) <= 0)
		hinv_nrgpus = 1;
	if ((hinv_nrdisk = extract_integer(result, descs, NRDISK)) <= 0)
		hinv_nrdisk = 1;
	if ((hinv_nrnuma = extract_integer(result, descs, NRNUMA)) <= 0)
		hinv_nrnuma = 1;
	if ((hinv_nrintf = extract_integer(result, descs, NRINTF)) <= 0)
		hinv_nrintf = 1;

	pmFreeResult(result);

	curtime = origin;
	setup_step_mode(0);
}

int
get_instance_index(pmResult *result, int value, int inst)
{
	pmValueSet *values = result->vset[value];
	int i;

	for (i = 0; i < values->numval; i++)
		if (values->vlist[i].inst == inst)
			return i;
	return 0;	/* not found, pick the start */
}

/*
** extract values from a pmResult structure using given offset(s)
** "value" is always a macro identifier from a metric map file.
*/
int
extract_integer_index(pmResult *result, pmDesc *descs, int value, int offset)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];

	if (values->numval <= 0 || values->numval <= offset)
		return 0;
	pmExtractValue(values->valfmt, &values->vlist[offset],
			descs[value].type, &atom, PM_TYPE_32);
	return atom.l;
}

int
extract_integer(pmResult *result, pmDesc *descs, int value)
{
	return extract_integer_index(result, descs, value, 0);
}

int
extract_integer_instmap_count(pmResult *result, pmDesc *descs, int value, int inst)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];
	int i, count = 0;

	if (values->numval <= 0)
		return 0;

	for (i = 0; i < values->numval; i++)
	{
		pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_32);
		if (atom.l == inst)
			count++;
	}
	return count;
}

int
extract_integer_inst(pmResult *result, pmDesc *descs, int value, int inst, int offset)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];
	int i;

	if (values->numval <= 0)
		return 0;
	if (offset < values->numval && values->vlist[offset].inst == inst)
	{
		pmExtractValue(values->valfmt, &values->vlist[offset],
			descs[value].type, &atom, PM_TYPE_32);
		return atom.l;
	}

	for (i = 0; i < values->numval; i++)
	{
		if (values->vlist[i].inst != inst)
			continue;
		pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_32);
		break;
	}
	if (i == values->numval)
		return 0;
	return atom.l;
}

count_t
extract_count_t(pmResult *result, pmDesc *descs, int value)
{
	return extract_count_t_index(result, descs, value, 0);
}

count_t
extract_count_t_index(pmResult *result, pmDesc *descs, int value, int i)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];

	if (values->numval <= 0 || values->numval <= i)
		return 0;

	pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_64);
	return atom.ll;
}

int
present_metric_value(pmResult *result, int value)
{
	return (result->vset[value]->numval <= 0);
}

unsigned long long
extract_ucount_t_inst(pmResult *result, pmDesc *descs, int value, int inst, int offset)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];
	int i;

	if (values->numval <= 0)
		return (unsigned long long)-1;
	if (offset < values->numval && values->vlist[offset].inst == inst)
	{
		pmExtractValue(values->valfmt, &values->vlist[offset],
			descs[value].type, &atom, PM_TYPE_U64);
		return atom.ull;
	}

	for (i = 0; i < values->numval; i++)
	{
		if (values->vlist[i].inst != inst)
			continue;
		pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_U64);
		break;
	}
	if (i == values->numval)
		return (unsigned long long)-1;
	return atom.ull;
}

count_t
extract_count_t_inst(pmResult *result, pmDesc *descs, int value, int inst, int offset)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];
	int i;

	if (values->numval <= 0)
		return 0;
	if (offset < values->numval && values->vlist[offset].inst == inst)
	{
		pmExtractValue(values->valfmt, &values->vlist[offset],
			descs[value].type, &atom, PM_TYPE_U64);
		return atom.ll;
	}

	for (i = 0; i < values->numval; i++)
	{
		if (values->vlist[i].inst != inst)
			continue;
		pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_64);
		break;
	}
	if (i == values->numval)
		return 0;
	return atom.ll;
}

char *
extract_string_index(pmResult *result, pmDesc *descs, int value, char *buffer, int buflen, int i)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];

	if (values->numval <= 0 || values->numval <= i)
		return NULL;

	pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_STRING);
	strncpy(buffer, atom.cp, buflen);
	free(atom.cp);
	if (buflen > 1)	/* might be a single character - e.g. process state */
	    buffer[buflen-1] = '\0';
	return buffer;
}

char *
extract_string(pmResult *result, pmDesc *descs, int value, char *buffer, int buflen)
{
	return extract_string_index(result, descs, value, buffer, buflen, 0);
}

char *
extract_string_inst(pmResult *result, pmDesc *descs, int value, char *buffer, int buflen, int inst, int offset)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];
	int i;

	if (values->numval <= 0)
		return NULL;
	if (offset < values->numval && values->vlist[offset].inst == inst)
	{
		pmExtractValue(values->valfmt, &values->vlist[offset],
			descs[value].type, &atom, PM_TYPE_STRING);
		goto copyout;
	}

	for (i = 0; i < values->numval; i++)
	{
		if (values->vlist[i].inst != inst)
			continue;
		pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_STRING);
		break;
	}
	if (values->numval == i)
		return NULL;
copyout:
	strncpy(buffer, atom.cp, buflen);
	free(atom.cp);
	if (buflen > 1)	/* might be a single character - e.g. process state */
	    buffer[buflen-1] = '\0';
	return buffer;
}

float
extract_float_inst(pmResult *result, pmDesc *descs, int value, int inst, int offset)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];
	int i;

	if (values->numval <= 0)
		return -1;
	if (offset < values->numval && values->vlist[offset].inst == inst)
	{
		pmExtractValue(values->valfmt, &values->vlist[offset],
			descs[value].type, &atom, PM_TYPE_FLOAT);
		return atom.f;
	}

	for (i = 0; i < values->numval; i++)
	{
		if (values->vlist[i].inst != inst)
			continue;
		pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_FLOAT);
		break;
	}
	if (i == values->numval)
		return -1;
	return atom.f;
}


void
setup_metrics(const char **metrics, pmID *pmidlist, pmDesc *desclist, int nmetrics)
{
	int	i, sts;

	if ((sts = pmLookupName(nmetrics, metrics, pmidlist)) < 0)
	{
		fprintf(stderr, "%s: pmLookupName: %s\n",
			pmGetProgname(), pmErrStr(sts));
		cleanstop(1);
	}
	if (nmetrics != sts)
	{
		for (i=0; i < nmetrics; i++)
		{
			if (pmidlist[i] != PM_ID_NULL)
				continue;
			if (pmDebugOptions.appl0)
				fprintf(stderr,
					"%s: pmLookupName failed for %s\n",
					pmGetProgname(), metrics[i]);
		}
	}

	for (i=0; i < nmetrics; i++)
	{
		if (pmidlist[i] == PM_ID_NULL)
		{
			desclist[i].pmid = PM_ID_NULL;
			continue;
		}
		if ((sts = pmLookupDesc(pmidlist[i], &desclist[i])) < 0)
		{
			if (pmDebugOptions.appl0)
				fprintf(stderr,
					"%s: pmLookupDesc failed for %s: %s\n",
					pmGetProgname(), metrics[i], pmErrStr(sts));
			pmidlist[i] = desclist[i].pmid = PM_ID_NULL;
		}
	}
}

int
time_less_than(struct timeval *a, struct timeval *b)
{
    if (a->tv_sec < b->tv_sec)
	return 1;
    if (a->tv_sec == b->tv_sec && a->tv_usec < b->tv_usec)
	return 1;
    return 0;
}

int
time_greater_than(struct timeval *a, struct timeval *b)
{
    if (a->tv_sec > b->tv_sec)
	return 1;
    if (a->tv_sec == b->tv_sec && a->tv_usec > b->tv_usec)
	return 1;
    return 0;
}

int
fetch_metrics(const char *purpose, int nmetrics, pmID *pmids, pmResult **result)
{
	pmResult	*rp;
	int		sts;

	if (time_greater_than(&curtime, &finish)) {
	    sampflags |= (RRLAST | RRMARK);
	    return PM_ERR_EOL;
	}

	if ((sts = pmSetMode(fetchmode, &curtime, fetchstep)) < 0)
	{
		fprintf(stderr, "%s: %s setmode: %s\n",
			pmGetProgname(), purpose, pmErrStr(sts));
		cleanstop(1);
	}
	if ((sts = pmFetch(nmetrics, pmids, result)) < 0)
	{
		if (sts == PM_ERR_EOL)
		{
			sampflags |= (RRLAST | RRMARK);
			return sts;
		}
		fprintf(stderr, "%s: %s query: %s\n",
			pmGetProgname(), purpose, pmErrStr(sts));
		cleanstop(1);
	}
	rp = *result;
	if (rp->numpmid == 0)	/* mark record */
		sampflags |= RRMARK;
	if (pmDebugOptions.appl1)
	{
		struct tm	tmp;
		time_t		sec;

		sec = (time_t)rp->timestamp.tv_sec;
		pmLocaltime(&sec, &tmp);

		fprintf(stderr, "%s: got %d %s metrics @%02d:%02d:%02d.%03d\n",
				pmGetProgname(), rp->numpmid, purpose,
				tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
				(int)(rp->timestamp.tv_usec / 1000));
	}
	return sts;
}

int
instances(const char *purpose, int all, int id, pmDesc *descs, int **ids, char ***insts)
{
	int	sts, i;

	if (descs[id].pmid == PM_ID_NULL)
	{
		/* we have no descriptor for this metric, thus no instances */
		*insts = NULL;
		*ids = NULL;
		return 0;
	}

	sts = (rawreadflag && all) ?
			pmGetInDomArchive(descs[id].indom, ids, insts) :
			pmGetInDom(descs[id].indom, ids, insts);
	if (sts == PM_ERR_INDOM_LOG)
	{
		/* metrics but no indom - expected sometimes, "no values" */
		*insts = NULL;
		*ids = NULL;
		return 0;
	}
	if (sts < 0)
	{
		fprintf(stderr, "%s: %s instances: %s\n",
			pmGetProgname(), purpose, pmErrStr(sts));
		cleanstop(1);
	}
	if (pmDebugOptions.appl1)
	{
		fprintf(stderr, "%s: got %d %s instances:\n",
			pmGetProgname(), sts, purpose);
		for (i=0; i < sts; i++)
			fprintf(stderr, "    [%d]  %s\n", (*ids)[i], (*insts)[i]);
	}
	return sts;
}

/* retrieve instances observed across all of time */
int
get_instances(const char *purpose, int mid, pmDesc *descs, int **ids, char ***insts)
{
    return instances(purpose, 1, mid, descs, ids, insts);
}

/* retrieve instances observed in the last sample */
int
fetch_instances(const char *purpose, int mid, pmDesc *descs, int **ids, char ***insts)
{
    return instances(purpose, 0, mid, descs, ids, insts);
}

static char *
rawlocalhost(pmOptions *opts)
{
	int		ctxt;
	char		*host;

	if (opts->nhosts > 0)
		return opts->hosts[0];

	if ((ctxt = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0)
	{
		fprintf(stderr, "%s: cannot create local context: %s\n",
			pmGetProgname(), pmErrStr(ctxt));
		cleanstop(1);
	}
	host = (char *)pmGetContextHostName(ctxt);
	pmDestroyContext(ctxt);

	if (host[0] == '\0')
	{
		fprintf(stderr, "%s: cannot find local hostname\n", pmGetProgname());
		cleanstop(1);
	}
	return host;
}

static int
lookslikedatetome(const char *p)
{
	register int    i;

	for (i=0; i < 8; i++)
		if ( !isdigit(*(p+i)) )
			return 0;

	if (*p != '2')
		return 0;       /* adapt this in the year 3000 */

	if ( *(p+4) > '1')
		return 0;

	if ( *(p+6) > '3')
		return 0;

	return 1;       /* yes, looks like a date to me */
}

void
rawarchive_from_midnight(pmOptions *opts)
{
	static char	midnight[32];
	struct tm 	today;
	time_t		now = time(NULL);

	pmLocaltime(&now, &today);
	pmsprintf(midnight, sizeof(midnight), "@ %04d-%02d-%02d 00:00:00",
		today.tm_year+1900, today.tm_mon+1, today.tm_mday);
	/* setting origin_optarg also enables archive mode */
	opts->origin_optarg = opts->start_optarg = midnight;
}

void
rawarchive(pmOptions *opts, const char *name)
{
	struct tm	*tp;
	time_t		timenow;
	char		tmp[MAXPATHLEN];
	char		path[MAXPATHLEN];
	char		*logdir, *py, *host;
	int		sep = pmPathSeparator();
	int		sts, len = (name? strlen(name) : 0);

	if (len == 0) {
		rawarchive_from_midnight(opts);
		return;
	}

	/* see if a valid archive exists as specified */
	if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, name)) >= 0)
	{
		pmDestroyContext(sts);
		__pmAddOptArchive(opts, (char * )name);
		return;
	}

	/* see if a valid folio exists as specified */
	strncpy(tmp, name, sizeof tmp);
	tmp[sizeof tmp-1] = '\0';
	if (access(tmp, R_OK) == 0)
	{
		__pmAddOptArchiveFolio(opts, tmp);
		return;
	}
	pmsprintf(path, sizeof path, "%s/%s.folio", name, basename(tmp));
	if (access(path, R_OK) == 0)
	{
		__pmAddOptArchiveFolio(opts, path);
		return;
	}

	/* else go hunting in the system locations... */
	if ((logdir = pmGetOptionalConfig("PCP_LOG_DIR")) == NULL)
	{
		fprintf(stderr, "%s: cannot find PCP_LOG_DIR\n", pmGetProgname());
		cleanstop(1);
	}

	host = rawlocalhost(opts);

	/*
	** Use original rawread() algorithms for specifying dates
	*/

	if (len == 8 && lookslikedatetome(name))
	{
		pmsprintf(path, sizeof path, "%s%c%s%c%s%c%s",
			logdir, sep, "pmlogger", sep, host, sep, name);
		__pmAddOptArchive(opts, (char * )path);
	}
	/*
	** if one or more 'y' (yesterday) characters are used and that
	** string is not known as an existing file, the standard logfile
	** is shown from N days ago (N is determined by the number
	** of y's).
	*/
	else
	{
                /*
		** make a string existing of y's to compare with
		*/
		py = malloc(len+1);
		ptrverify(py, "Malloc failed for 'yes' sequence\n");

		memset(py, 'y', len);
		*(py+len) = '\0';

		if ( strcmp(name, py) == 0 )
		{
			timenow  = time(0);
			timenow -= len*3600*24;
			tp       = localtime(&timenow);

			pmsprintf(path, sizeof path, "%s%c%s%c%s%c%04u%02u%02u",
				logdir, sep, "pmlogger", sep, host, sep,
				tp->tm_year+1900, tp->tm_mon+1, tp->tm_mday);
			__pmAddOptArchive(opts, (char * )path);
		}
		else
		{
			fprintf(stderr, "%s: cannot find archive from \"%s\"\n",
				pmGetProgname(), name);
			cleanstop(1);
		}

		free(py);
	}
}

/*
** Write a pmlogger configuration file for recording.
*/
static void
rawconfig(FILE *fp, double delta)
{
	const char		**p;
	unsigned int		logdelta;

	fprintf(fp, "log mandatory on once {\n");
	for (p = hostmetrics; (*p)[0] != '.'; p++)
		fprintf(fp, "    %s\n", *p);
	for (p = ifpropmetrics; (*p)[0] != '.'; p++)
		fprintf(fp, "    %s\n", *p);
	fprintf(fp, "}\n\n");

	logdelta = (unsigned int)(delta * 1000.0);	/* msecs */
	fprintf(fp, "log mandatory on every %u milliseconds {\n", logdelta);
	for (p = systmetrics; (*p)[0] != '.'; p++)
		fprintf(fp, "    %s\n", *p);
	for (p = procmetrics; (*p)[0] != '.'; p++)
		fprintf(fp, "    %s\n", *p);
	fputs("}\n", fp);
}

void
rawwrite(pmOptions *opts, const char *name,
	struct timeval *delta, unsigned int nsamples, char midnightflag)
{
	pmRecordHost	*record;
	struct timeval	elapsed;
	double		duration, ddelta;
	char		args[MAXPATHLEN];
	char		*host;
	int		sts;

	host = (opts->nhosts > 0) ? opts->hosts[0] : "local:";
	ddelta = pmtimevalToReal(delta);
	duration = ddelta * nsamples;

	if (midnightflag)
	{
		time_t		now = time(NULL);
		struct tm	*tp;

		tp = localtime(&now);

		tp->tm_hour = 23;
		tp->tm_min  = 59;
		tp->tm_sec  = 59;

		duration = (double) (mktime(tp) - now);
	}

	/* need to avoid elapsed.tv_sec going negative */
	if (duration > INT_MAX)
	    duration = INT_MAX - 1;

	if (pmDebugOptions.appl1)
	{
		fprintf(stderr, "%s: start recording, %.2fsec duration [%s].\n",
			pmGetProgname(), duration, name);
	}

	if (__pmMakePath(name, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) < 0)
	{
		fprintf(stderr, "%s: making folio path %s for recording: %s\n",
			pmGetProgname(), name, osstrerror());
		cleanstop(1);
	}
	if (chdir(name) < 0)
	{
		fprintf(stderr, "%s: entering folio %s for recording: %s\n",
			pmGetProgname(), name, strerror(oserror()));
		cleanstop(1);
	}

	/*
        ** Non-graphical application using libpcp_gui services - never want
	** to see popup dialogs from pmlogger(1) here, so force the issue.
	*/
	putenv("PCP_XCONFIRM_PROG=/bin/true");

	pmsprintf(args, sizeof args, "%s.folio", basename((char *)name));
	if (pmRecordSetup(args, pmGetProgname(), 1) == NULL)
	{
		fprintf(stderr, "%s: cannot setup recording to %s: %s\n",
			pmGetProgname(), name, osstrerror());
		cleanstop(1);
	}
	if ((sts = pmRecordAddHost(host, 1, &record)) < 0)
	{
		fprintf(stderr, "%s: adding host %s to recording: %s\n",
			pmGetProgname(), host, pmErrStr(sts));
		cleanstop(1);
	}

	rawconfig(record->f_config, ddelta);

	/*
	** start pmlogger with a deadhand timer, ensuring it will stop
	*/
	if (opts->samples || midnightflag) {
	    pmsprintf(args, sizeof args, "-T%.3fseconds", duration);
	    if ((sts = pmRecordControl(record, PM_REC_SETARG, args)) < 0)
		{
		    fprintf(stderr, "%s: setting loggers arguments: %s\n",
			    pmGetProgname(), pmErrStr(sts));
		    cleanstop(1);
		}
	}
	if ((sts = pmRecordControl(NULL, PM_REC_ON, "")) < 0)
	{
		fprintf(stderr, "%s: failed to start recording: %s\n",
			pmGetProgname(), pmErrStr(sts));
		cleanstop(1);
	}

	pmtimevalFromReal(duration, &elapsed);
	__pmtimevalSleep(elapsed);

	if ((sts = pmRecordControl(NULL, PM_REC_OFF, "")) < 0)
	{
		fprintf(stderr, "%s: failed to stop recording: %s\n",
			pmGetProgname(), pmErrStr(sts));
		cleanstop(1);
	}

	if (pmDebugOptions.appl1)
	{
		fprintf(stderr, "%s: cleanly stopped recording.\n",
			pmGetProgname());
	}
}

/*
** print any custom PCP options for this command
*/
void
show_pcp_usage(pmOptions *opts)
{
	pmLongOptions	*lop;

	for (lop = opts->long_options; lop->long_opt; lop++) {
		if (!lop->message)
			continue;
		if (!lop->has_arg)
			printf("\t  --%s\t%s\n", lop->long_opt, lop->message);
		else
			printf("\t  --%s %s\t%s\n",
				lop->long_opt, lop->argname, lop->message);
	}
}

/*
 * Manage a cache of user and group names, such that
 * uid and gid based lookups can be performed into a
 * hash with one copy of each name.  Also, this hash
 * table is temporal in that an insert with a fresh,
 * different value replaces any earlier observation.
 */
static __pmHashCtl users;
static __pmHashCtl groups;

static void
add_name(__pmHashCtl *cache, int key, const char *name)
{
	__pmHashNode	*hp;

	if ((hp = __pmHashSearch(key, cache)) == NULL)
	{
		__pmHashAdd(key, strdup(name), cache);
	}
	else
	{
		if (strcmp(name, hp->data) == 0)
			return;
		free(hp->data);
		hp->data = strdup(name);
	}
}

static char *
get_name(__pmHashCtl *cache, int key)
{
	__pmHashNode	*hp;

	if ((hp = __pmHashSearch(key, cache)) == NULL)
		return NULL;
	return (char *)hp->data;
}

void
add_username(int uid, const char *username)
{
	add_name(&users, uid, username);
}

char *
get_username(int uid)
{
	return get_name(&users, uid);
}

void
add_groupname(int gid, const char *groupname)
{
	add_name(&groups, gid, groupname);
}

char *
get_groupname(int gid)
{
	return get_name(&groups, gid);
}
/*
** return maximum number of digits for PID/TID
*/
int
getpidwidth(void)
{
	char	linebuf[64];
	int	numdigits = 5;

	/*
	** determine maximum number of digits for PID/TID
	*/
	if (pmsprintf(linebuf, sizeof linebuf, "%d", pidmax))
		numdigits = strlen(linebuf);

	return numdigits;
}
