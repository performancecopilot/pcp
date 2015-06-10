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
** Copyright (C) 2015 Red Hat.
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
#include <pcp/impl.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

#include "atop.h"
#include "hostmetrics.h"

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
	snprintf(chartim, buflen, "%02d:%02d:%02d",
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
	snprintf(chardat, buflen, "%04d/%02d/%02d",
		tt.tm_year+1900, tt.tm_mon+1, tt.tm_mday);
	return chardat;
}


/*
** Convert a hh:mm string into a number of seconds since 00:00
**
** Return-value:	0 - Wrong input-format
**			1 - Success
*/
int
hhmm2secs(char *itim, unsigned int *otim)
{
	register int	i;
	int		hours, minutes;

	/*
	** check string syntax
	*/
	for (i=0; *(itim+i); i++)
		if ( !isdigit(*(itim+i)) && *(itim+i) != ':' )
			return(0);

	sscanf(itim, "%d:%d", &hours, &minutes);

	if ( hours < 0 || hours > 23 || minutes < 0 || minutes > 59 )
		return(0);

	*otim = (hours * 3600) + (minutes * 60);

	if (*otim >= SECSDAY)
		*otim = SECSDAY-1;

	return(1);
}


/*
** Function val2valstr() converts a value to an ascii-string of a fixed
** number of positions; if the value does not fit, it will be formatted to
** exponent-notation (as short as possible, so not via the standard printf-
** formatters %f or %e). The offered string should have a length of width+1.
** The value might even be printed as an average for the interval-time.
*/
char *
val2valstr(count_t value, char *strvalue, size_t buflen, int width, int avg, int nsecs)
{
	count_t	maxval, remain = 0;
	int	exp     = 0;
	char	*suffix = "";

	if (avg)
	{
		value  = (value + (nsecs/2)) / nsecs;     /* rounded value */
		width  = width - 2;	/* subtract two positions for '/s' */
		suffix = "/s";
	}

	maxval = pow(10.0, width) - 1;

	if (value < maxval)
	{
		snprintf(strvalue, buflen, "%*lld%s", width, value, suffix);
	}
	else
	{
		if (width < 3)
		{
			/*
			** cannot avoid ignoring width
			*/
			snprintf(strvalue, buflen, "%lld%s", value, suffix);
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

			if (remain >= 5)
				value++;

			snprintf(strvalue, buflen, "%*llde%d%s",
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
        char	*p=strvalue, doshow=0;
	int	bytes, offset=0;

        if (value > DAYSECS) 
        {
                bytes = snprintf(p, buflen-offset, "%dd", value/DAYSECS);
		p += bytes;
		offset += bytes;
                value %= DAYSECS;
		doshow = 1;
        }

        if (value > HOURSECS || doshow) 
        {
                bytes = snprintf(p, buflen-offset, "%dh", value/HOURSECS);
		p += bytes;
		offset += bytes;
                value %= HOURSECS;
		doshow = 1;
        }

        if (value > MINSECS || doshow) 
        {
                bytes = snprintf(p, buflen-offset, "%dm", value/MINSECS);
		p += bytes;
		offset += bytes;
                value %= MINSECS;
		doshow = 1;
        }

        if (value || doshow) 
        {
                bytes = snprintf(p, buflen-offset, "%ds", value);
		p += bytes;
		offset += bytes;
		doshow = 1;
        }

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
		snprintf(strvalue, buflen, "%2lld.%02llds", value/1000, value%1000/10);
	}
	else
	{
	        /*
       	 	** millisecs irrelevant; round to seconds
       	 	*/
        	value = (value + 500) / 1000;

        	if (value < MAXSEC) 
        	{
               	 	snprintf(strvalue, buflen, "%2lldm%02llds", value/60, value%60);
		}
		else
		{
			/*
			** seconds irrelevant; round to minutes
			*/
			value = (value + 30) / 60;

			if (value < MAXMIN) 
			{
				snprintf(strvalue, buflen, "%2lldh%02lldm",
							value/60, value%60);
			}
			else
			{
				/*
				** minutes irrelevant; round to hours
				*/
				value = (value + 30) / 60;

				snprintf(strvalue, buflen, "%2lldd%02lldh",
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
** which should be able to contain at least 8 positions.
*/
char *
val2Hzstr(count_t value, char *strvalue, size_t buflen)
{
        if (value < 1000)
        {
                snprintf(strvalue, buflen, "%4lldMHz", value);
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
                snprintf(strvalue, buflen, "%4.2f%cHz", fval, prefix);
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

#define	MAXBYTE		1024
#define	MAXKBYTE	ONEKBYTE*99999L
#define	MAXMBYTE	ONEMBYTE*999L
#define	MAXGBYTE	ONEGBYTE*999LL

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
	if (avgval)
	{
		value     /= nsecs;
		verifyval *= 100;
		basewidth -= 2;
		suffix     = "/s";
	}
	
	/*
	** determine which format will be used on bases of the value itself
	*/
	if (verifyval <= MAXBYTE)	/* bytes ? */
		aformat = ANYFORMAT;
	else
		if (verifyval <= MAXKBYTE)	/* kbytes ? */
			aformat = KBFORMAT;
		else
			if (verifyval <= MAXMBYTE)	/* mbytes ? */
				aformat = MBFORMAT;
			else
				if (verifyval <= MAXGBYTE)	/* mbytes ? */
					aformat = GBFORMAT;
				else
					aformat = TBFORMAT;

	/*
	** check if this is also the preferred format
	*/
	if (aformat <= pformat)
		aformat = pformat;

	switch (aformat)
	{
	   case	ANYFORMAT:
		snprintf(strvalue, buflen, "%*lld%s",
				basewidth, value, suffix);
		break;

	   case	KBFORMAT:
		snprintf(strvalue, buflen, "%*lldK%s",
				basewidth-1, value/ONEKBYTE, suffix);
		break;

	   case	MBFORMAT:
		snprintf(strvalue, buflen, "%*.1lfM%s",
			basewidth-1, (double)((double)value/ONEMBYTE), suffix); 
		break;

	   case	GBFORMAT:
		snprintf(strvalue, buflen, "%*.1lfG%s",
			basewidth-1, (double)((double)value/ONEGBYTE), suffix);
		break;

	   case	TBFORMAT:
		snprintf(strvalue, buflen, "%*.1lfT%s",
			basewidth-1, (double)((double)value/ONETBYTE), suffix);
		break;

	   default:
		snprintf(strvalue, buflen, "!TILT!");
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
	va_list args;

        va_start(args, errormsg);

	if (!ptr)
	{
		acctswoff();
		netatop_signoff();

		if (vis.show_end)
			(vis.show_end)();

        	va_list args;
		fprintf(stderr, errormsg, args);
        	va_end  (args);

		exit(13);
	}
}

/*
** signal catcher for cleanup before exit
*/
void
cleanstop(exitcode)
{
	acctswoff();
	netatop_signoff();
	(vis.show_end)();
	exit(exitcode);
}

/*
** establish an async timer alarm with microsecond-level precision
*/
void
setalarm(struct timeval *interval)
{
	struct itimerval val;

	val.it_value = *interval;
	val.it_interval.tv_sec = val.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &val, NULL);
}

void
setalarm2(int sec, int usec)
{
	struct timeval interval;

	interval.tv_sec = sec;
	interval.tv_usec = usec;
	setalarm(&interval);
}

static void
setup_step_mode(void)
{
	const int SECONDS_IN_24_DAYS = 2073600;

	if (rawreadflag)
		fetchmode = PM_MODE_INTERP;
	else
		fetchmode = PM_MODE_LIVE;

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
 * Set the origin position and interval for PMAPI context fetching
 */
static int
setup_origin(pmOptions *opts)
{
	int		sts = 0;

	curtime = origin = opts->origin;

	/* initial archive mode, position and delta */
	if (opts->context == PM_CONTEXT_ARCHIVE)
	{
		if (opts->interval.tv_sec || opts->interval.tv_usec)
			interval = opts->interval;

		setup_step_mode();

		if ((sts = pmSetMode(fetchmode, &curtime, fetchstep)) < 0)
		{
			pmprintf(
		"%s: pmSetMode failure: %s\n", pmProgname, pmErrStr(sts));
			opts->flags |= PM_OPTFLAG_RUNTIME_ERR;
			opts->errors++;
		}
	}

	return sts;
}

/*
 * PMAPI context creation and initial command line option handling.
 */
static int
setup_context(pmOptions *opts)
{
	char		*source;
	int		sts, ctx;

	if (opts->context == PM_CONTEXT_ARCHIVE)
		source = opts->archives[0];
	else if (opts->context == PM_CONTEXT_HOST)
		source = opts->hosts[0];
	else if (opts->context == PM_CONTEXT_LOCAL)
		source = NULL;
	else
	{
		opts->context = PM_CONTEXT_HOST;
		source = "local:";
	}

	if ((sts = ctx = pmNewContext(opts->context, source)) < 0)
	{
		if (opts->context == PM_CONTEXT_HOST)
			pmprintf(
		"%s: Cannot connect to pmcd on host \"%s\": %s\n",
				pmProgname, source, pmErrStr(sts));
		else if (opts->context == PM_CONTEXT_LOCAL)
			pmprintf(
		"%s: Cannot make standalone connection on localhost: %s\n",
				pmProgname, pmErrStr(sts));
		else
			pmprintf(
		"%s: Cannot open archive \"%s\": %s\n",
				pmProgname, source, pmErrStr(sts));
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
	pmResult	*result;

	setup_context(opts);
	setup_metrics(hostmetrics, &pmids[0], &descs[0], HOST_NMETRICS);
	fetch_metrics("host", HOST_NMETRICS, pmids, &result);

	if (HOST_NMETRICS != result->numpmid)
	{
		fprintf(stderr,
			"%s: pmFetch failed to fetch initial metric value(s)\n",
			pmProgname);
		cleanstop(1);
	}

	hertz = extract_integer(result, descs, HOST_HERTZ);
	pagesize = extract_integer(result, descs, HOST_PAGESIZE);
	extract_string(result, descs, HOST_RELEASE, sysname.release, sizeof(sysname.release));
	extract_string(result, descs, HOST_VERSION, sysname.version, sizeof(sysname.version));
	extract_string(result, descs, HOST_MACHINE, sysname.machine, sizeof(sysname.machine));
	extract_string(result, descs, HOST_NODENAME, sysname.nodename, sizeof(sysname.nodename));
	nodenamelen = strlen(sysname.nodename);

	pmFreeResult(result);
}

/*
** extract values from a pmResult structure using given offset(s)
** "value" is always a macro identifier from a metric map file.
*/
int
extract_integer_index(pmResult *result, pmDesc *descs, int value, int i)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];

	if (values->numval <= 0 || values->numval <= i)
		return -1;
	pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_32);
	return atom.l;
}

int
extract_integer(pmResult *result, pmDesc *descs, int value)
{
	return extract_integer_index(result, descs, value, 0);
}

int
extract_integer_inst(pmResult *result, pmDesc *descs, int value, int inst)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];
	int i;

	for (i = 0; i < values->numval; i++)
	{
		if (values->vlist[i].inst != inst)
			continue;
		pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_32);
		break;
	}
	if (values->numval <= 0 || i == values->numval)
		return -1;
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
		return -1;

	pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_64);
	return atom.ll;
}

count_t
extract_count_t_inst(pmResult *result, pmDesc *descs, int value, int inst)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];
	int i;

	for (i = 0; i < values->numval; i++)
	{
		if (values->vlist[i].inst != inst)
			continue;
		pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_64);
		break;
	}
	if (values->numval <= 0 || i == values->numval)
		return -1;
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
extract_string_inst(pmResult *result, pmDesc *descs, int value, char *buffer, int buflen, int inst)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];
	int i;

	for (i = 0; i < values->numval; i++)
	{
		if (values->vlist[i].inst != inst)
			continue;
		pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_STRING);
		break;
	}
	if (values->numval <= 0 || values->numval == i)
		return NULL;
	strncpy(buffer, atom.cp, buflen);
	free(atom.cp);
	if (buflen > 1)	/* might be a single character - e.g. process state */
	    buffer[buflen-1] = '\0';
	return buffer;
}

float
extract_float_inst(pmResult *result, pmDesc *descs, int value, int inst)
{
	pmAtomValue atom = { 0 };
	pmValueSet *values = result->vset[value];
	int i;

	for (i = 0; i < values->numval; i++)
	{
		if (values->vlist[i].inst != inst)
			continue;
		pmExtractValue(values->valfmt, &values->vlist[i],
			descs[value].type, &atom, PM_TYPE_FLOAT);
		break;
	}
	if (values->numval <= 0 || i == values->numval)
		return -1;
	return atom.f;
}


void
setup_metrics(char **metrics, pmID *pmidlist, pmDesc *desclist, int nmetrics)
{
	int	i, sts;

	if ((sts = pmLookupName(nmetrics, metrics, pmidlist)) < 0)
	{
		fprintf(stderr, "%s: pmLookupName: %s\n",
			pmProgname, pmErrStr(sts));
		cleanstop(1);
	}
	if (nmetrics != sts)
	{
		for (i=0; i < nmetrics; i++)
		{
			if (pmidlist[i] != PM_ID_NULL)
				continue;
			if (pmDebug & DBG_TRACE_APPL0)
				fprintf(stderr,
					"%s: pmLookupName failed for %s\n",
					pmProgname, metrics[i]);
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
			if (pmDebug & DBG_TRACE_APPL0)
				fprintf(stderr,
					"%s: pmLookupDesc failed for %s: %s\n",
					pmProgname, metrics[i], pmErrStr(sts));
			pmidlist[i] = desclist[i].pmid = PM_ID_NULL;
		}
	}
}

int
fetch_metrics(const char *purpose, int nmetrics, pmID *pmids, pmResult **result)
{
	int	sts;

	pmSetMode(fetchmode, &curtime, fetchstep);
	if ((sts = pmFetch(nmetrics, pmids, result)) < 0)
	{
		if (sts != PM_ERR_EOL)
			fprintf(stderr, "%s: %s query: %s\n",
				pmProgname, purpose, pmErrStr(sts));
		cleanstop(1);
	}
	if (pmDebug & DBG_TRACE_APPL1)
	{
		pmResult	*rp = *result;
		struct tm	tmp;
		time_t		sec;

		sec = (time_t)rp->timestamp.tv_sec;
		pmLocaltime(&sec, &tmp);

		fprintf(stderr, "%s: got %d %s metrics @%02d:%02d:%02d.%03d\n",
				pmProgname, rp->numpmid, purpose,
				tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
				(int)(rp->timestamp.tv_usec / 1000));
	}
	return sts;
}

int
get_instances(const char *purpose, int value, pmDesc *descs, int **ids, char ***insts)
{
	int	sts, i;

	if (descs[value].pmid == PM_ID_NULL)
	{
		/* we have no descriptor for this metric, thus no instances */
		*insts = NULL;
		*ids = NULL;
		return 0;
	}

	sts = !rawreadflag ? pmGetInDom(descs[value].indom, ids, insts) :
			pmGetInDomArchive(descs[value].indom, ids, insts);
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
			pmProgname, purpose, pmErrStr(sts));
		cleanstop(1);
	}
	if (pmDebug & DBG_TRACE_APPL1)
	{
		fprintf(stderr, "%s: got %d %s instances:\n",
			pmProgname, sts, purpose);
		for (i=0; i < sts; i++)
			fprintf(stderr, "    [%d]  %s\n", (*ids)[i], (*insts)[i]);
	}
	return sts;
}

/*
** Write a pmlogger configuration file for recording.
*/
static void
rawconfig(FILE *fp, double interval)
{
	char		**p;
	unsigned int	delta;
	extern char	*hostmetrics[];
	extern char	*ifpropmetrics[];
	extern char	*systmetrics[];
	extern char	*procmetrics[];

	fprintf(fp, "log mandatory on once {\n");
	for (p = hostmetrics; (*p)[0] != '.'; p++)
		fprintf(fp, "    %s\n", *p);
	for (p = ifpropmetrics; (*p)[0] != '.'; p++)
		fprintf(fp, "    %s\n", *p);
	fprintf(fp, "}\n\n");

	delta = (unsigned int)(interval * 1000.0);	/* msecs */
	fprintf(fp, "log mandatory on every %u milliseconds {\n", delta);
	for (p = systmetrics; (*p)[0] != '.'; p++)
		fprintf(fp, "    %s\n", *p);
	for (p = procmetrics; (*p)[0] != '.'; p++)
		fprintf(fp, "    %s\n", *p);
	fputs("}\n", fp);
}

void
rawwrite(const char *name, const char *host,
	struct timeval *delta, unsigned int nsamples, char midnightflag)
{
	pmRecordHost	*record;
	struct timeval	elapsed;
	double		duration;
	double		interval;
	char		args[MAXPATHLEN];
	int		sts;

	interval = __pmtimevalToReal(delta);
	duration = interval * nsamples;

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

	if (pmDebug & DBG_TRACE_APPL1)
	{
		fprintf(stderr, "%s: start recording, %.2fsec duration [%s].\n",
			pmProgname, duration, name);
	}

	if (__pmMakePath(name, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) < 0)
	{
		fprintf(stderr, "%s: making folio path %s for recording: %s\n",
			pmProgname, name, osstrerror());
		cleanstop(1);
	}
	if (chdir(name) < 0)
	{
		fprintf(stderr, "%s: entering folio %s for recording: %s\n",
			pmProgname, name, strerror(oserror()));
		cleanstop(1);
	}

	/*
        ** Non-graphical application using libpcp_gui services - never want
	** to see popup dialogs from pmlogger(1) here, so force the issue.
	*/
	putenv("PCP_XCONFIRM_PROG=/bin/true");

	snprintf(args, sizeof(args), "%s.folio", basename((char *)name));
	args[sizeof(args)-1] = '\0';
	if (pmRecordSetup(args, pmProgname, 1) == NULL)
	{
		fprintf(stderr, "%s: cannot setup recording to %s: %s\n",
			pmProgname, name, osstrerror());
		cleanstop(1);
	}
	if ((sts = pmRecordAddHost(host, 1, &record)) < 0)
	{
		fprintf(stderr, "%s: adding host %s to recording: %s\n",
			pmProgname, host, pmErrStr(sts));
		cleanstop(1);
	}

	rawconfig(record->f_config, interval);

	/*
	** start pmlogger with a deadhand timer, ensuring it will stop
	*/
	snprintf(args, sizeof(args), "-T%.3fseconds", duration);
	args[sizeof(args)-1] = '\0';
	if ((sts = pmRecordControl(record, PM_REC_SETARG, args)) < 0)
	{
		fprintf(stderr, "%s: setting loggers arguments: %s\n",
			pmProgname, pmErrStr(sts));
		cleanstop(1);
	}
	if ((sts = pmRecordControl(NULL, PM_REC_ON, "")) < 0)
	{
		fprintf(stderr, "%s: failed to start recording: %s\n",
			pmProgname, pmErrStr(sts));
		cleanstop(1);
	}

	__pmtimevalFromReal(duration, &elapsed);
	__pmtimevalSleep(elapsed);

	if ((sts = pmRecordControl(NULL, PM_REC_OFF, "")) < 0)
	{
		fprintf(stderr, "%s: failed to stop recording: %s\n",
			pmProgname, pmErrStr(sts));
		cleanstop(1);
	}

	if (pmDebug & DBG_TRACE_APPL1)
	{
		fprintf(stderr, "%s: cleanly stopped recording.\n",
			pmProgname);
	}
}
