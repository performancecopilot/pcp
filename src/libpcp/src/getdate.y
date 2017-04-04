%{
/*
**  Originally written by Steven M. Bellovin <smb@research.att.com> while
**  at the University of North Carolina at Chapel Hill.  Later tweaked by
**  a couple of people on Usenet.  Completely overhauled by Rich $alz
**  <rsalz@bbn.com> and Jim Berets <jberets@bbn.com> in August, 1990;
**  Conversion to GNU bison pure-parser form and to use PCP time(zone)
**  interfaces, by Nathan Scott <nathans@redhat.com> in September 2015.
**
**  This grammar has 10 shift/reduce conflicts.
**
**  This code is in the public domain and has no copyright.
*/
#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"

#define EPOCH		1970
#define TM_YEAR_ORIGIN 1900
#define HOUR(x)		((time_t)(x) * 60)
#define SECSPERDAY	(24L * 60L * 60L)

/*
**  An entry in the lexical lookup table.
*/
typedef struct _TABLE {
    const char	*name;
    int		type;
    time_t	value;
} TABLE;

/*
**  Daylight-savings mode:  on, off, or not yet known.
*/
typedef enum _DSTMODE {
    DSTon, DSToff, DSTmaybe
} DSTMODE;

/*
**  Meridian:  am, pm, or 24-hour style.
*/
typedef enum _MERIDIAN {
    MERam, MERpm, MER24
} MERIDIAN;


/*
**  Parser structure (used to be globals).
*/
typedef struct _PARSER {
    char	*yyInput;
    DSTMODE	yyDSTmode;
    time_t	yyDayOrdinal;
    time_t	yyDayNumber;
    int		yyHaveDate;
    int		yyHaveDay;
    int		yyHaveRel;
    int		yyHaveTime;
    int		yyHaveZone;
    time_t	yyTimezone;
    time_t	yyDay;
    time_t	yyHour;
    time_t	yyMinutes;
    time_t	yyMonth;
    time_t	yySeconds;
    time_t	yyYear;
    MERIDIAN	yyMeridian;
    time_t	yyRelMonth;
    time_t	yyRelSeconds;
} PARSER;

union YYSTYPE;
static int yylex(union YYSTYPE *, PARSER *);
static int yyerror(PARSER *, const char *);

%}

%pure-parser
%parse-param { PARSER *lp }
%lex-param { PARSER *lp }

/* This grammar has 10 shift/reduce conflicts. */
%expect 10

%union {
    time_t		Number;
    enum _MERIDIAN	Meridian;
}

%token	tAGO tDAY tDAYZONE tID tMERIDIAN tMINUTE_UNIT tMONTH tMONTH_UNIT
%token	tSEC_UNIT tSNUMBER tUNUMBER tZONE tDST

%type	<Number>	tDAY tDAYZONE tMINUTE_UNIT tMONTH tMONTH_UNIT
%type	<Number>	tSEC_UNIT tSNUMBER tUNUMBER tZONE
%type	<Meridian>	tMERIDIAN o_merid

%%

spec	: /* NULL */
	| spec item
	;

item	: time {
	    lp->yyHaveTime++;
	}
	| zone {
	    lp->yyHaveZone++;
	}
	| date {
	    lp->yyHaveDate++;
	}
	| day {
	    lp->yyHaveDay++;
	}
	| rel {
	    lp->yyHaveRel++;
	}
	| number
	;

time	: tUNUMBER tMERIDIAN {
	    lp->yyHour = $1;
	    lp->yyMinutes = 0;
	    lp->yySeconds = 0;
	    lp->yyMeridian = $2;
	}
	| tUNUMBER ':' tUNUMBER o_merid {
	    lp->yyHour = $1;
	    lp->yyMinutes = $3;
	    lp->yySeconds = 0;
	    lp->yyMeridian = $4;
	}
	| tUNUMBER ':' tUNUMBER tSNUMBER {
	    lp->yyHour = $1;
	    lp->yyMinutes = $3;
	    lp->yyMeridian = MER24;
	    lp->yyDSTmode = DSToff;
	    lp->yyTimezone = - ($4 % 100 + ($4 / 100) * 60);
	}
	| tUNUMBER ':' tUNUMBER ':' tUNUMBER o_merid {
	    lp->yyHour = $1;
	    lp->yyMinutes = $3;
	    lp->yySeconds = $5;
	    lp->yyMeridian = $6;
	}
	| tUNUMBER ':' tUNUMBER ':' tUNUMBER tSNUMBER {
	    lp->yyHour = $1;
	    lp->yyMinutes = $3;
	    lp->yySeconds = $5;
	    lp->yyMeridian = MER24;
	    lp->yyDSTmode = DSToff;
	    lp->yyTimezone = - ($6 % 100 + ($6 / 100) * 60);
	}
	;

zone	: tZONE {
	    lp->yyTimezone = $1;
	    lp->yyDSTmode = DSToff;
	}
	| tDAYZONE {
	    lp->yyTimezone = $1;
	    lp->yyDSTmode = DSTon;
	}
	|
	  tZONE tDST {
	    lp->yyTimezone = $1;
	    lp->yyDSTmode = DSTon;
	}
	;

day	: tDAY {
	    lp->yyDayOrdinal = 0;
	    lp->yyDayNumber = $1;
	}
	| tDAY ',' {
	    lp->yyDayOrdinal = 0;
	    lp->yyDayNumber = $1;
	}
	| tUNUMBER tDAY {
	    lp->yyDayOrdinal = $1;
	    lp->yyDayNumber = $2;
	}
	;

date	: tUNUMBER '/' tUNUMBER {
	    lp->yyMonth = $1;
	    lp->yyDay = $3;
	}
	| tUNUMBER '/' tUNUMBER '/' tUNUMBER {
	    if ($1 >= 100) {
		lp->yyYear = $1;
		lp->yyMonth = $3;
		lp->yyDay = $5;
	    } else {
		lp->yyMonth = $1;
		lp->yyDay = $3;
		lp->yyYear = $5;
	    }
	}
	| tUNUMBER tSNUMBER tSNUMBER {
	    /* ISO 8601 format.  yyyy-mm-dd.  */
	    lp->yyYear = $1;
	    lp->yyMonth = -$2;
	    lp->yyDay = -$3;
	}
	| tUNUMBER tMONTH tSNUMBER {
	    /* e.g. 17-JUN-1992.  */
	    lp->yyDay = $1;
	    lp->yyMonth = $2;
	    lp->yyYear = -$3;
	}
	| tMONTH tUNUMBER {
	    lp->yyMonth = $1;
	    lp->yyDay = $2;
	}
	| tMONTH tUNUMBER ',' tUNUMBER {
	    lp->yyMonth = $1;
	    lp->yyDay = $2;
	    lp->yyYear = $4;
	}
	| tUNUMBER tMONTH {
	    lp->yyMonth = $2;
	    lp->yyDay = $1;
	}
	| tUNUMBER tMONTH tUNUMBER {
	    lp->yyMonth = $2;
	    lp->yyDay = $1;
	    lp->yyYear = $3;
	}
	;

rel	: relunit tAGO {
	    lp->yyRelSeconds = -lp->yyRelSeconds;
	    lp->yyRelMonth = -lp->yyRelMonth;
	}
	| relunit
	;

relunit	: tUNUMBER tMINUTE_UNIT {
	    lp->yyRelSeconds += $1 * $2 * 60L;
	}
	| tSNUMBER tMINUTE_UNIT {
	    lp->yyRelSeconds += $1 * $2 * 60L;
	}
	| tMINUTE_UNIT {
	    lp->yyRelSeconds += $1 * 60L;
	}
	| tSNUMBER tSEC_UNIT {
	    lp->yyRelSeconds += $1;
	}
	| tUNUMBER tSEC_UNIT {
	    lp->yyRelSeconds += $1;
	}
	| tSEC_UNIT {
	    lp->yyRelSeconds++;
	}
	| tSNUMBER tMONTH_UNIT {
	    lp->yyRelMonth += $1 * $2;
	}
	| tUNUMBER tMONTH_UNIT {
	    lp->yyRelMonth += $1 * $2;
	}
	| tMONTH_UNIT {
	    lp->yyRelMonth += $1;
	}
	;

number	: tUNUMBER {
	    if (lp->yyHaveTime && lp->yyHaveDate && !lp->yyHaveRel)
		lp->yyYear = $1;
	    else {
		if($1>10000) {
		    lp->yyHaveDate++;
		    lp->yyDay= ($1)%100;
		    lp->yyMonth= ($1/100)%100;
		    lp->yyYear = $1/10000;
		}
		else {
		    lp->yyHaveTime++;
		    if ($1 < 100) {
			lp->yyHour = $1;
			lp->yyMinutes = 0;
		    }
		    else {
		    	lp->yyHour = $1 / 100;
		    	lp->yyMinutes = $1 % 100;
		    }
		    lp->yySeconds = 0;
		    lp->yyMeridian = MER24;
	        }
	    }
	}
	;

o_merid	: /* NULL */ {
	    $$ = MER24;
	}
	| tMERIDIAN {
	    $$ = $1;
	}
	;

%%

/* Month and day table. */
static TABLE const MonthDayTable[] = {
    { "january",	tMONTH,  1 },
    { "february",	tMONTH,  2 },
    { "march",		tMONTH,  3 },
    { "april",		tMONTH,  4 },
    { "may",		tMONTH,  5 },
    { "june",		tMONTH,  6 },
    { "july",		tMONTH,  7 },
    { "august",		tMONTH,  8 },
    { "september",	tMONTH,  9 },
    { "sept",		tMONTH,  9 },
    { "october",	tMONTH, 10 },
    { "november",	tMONTH, 11 },
    { "december",	tMONTH, 12 },
    { "sunday",		tDAY, 0 },
    { "monday",		tDAY, 1 },
    { "tuesday",	tDAY, 2 },
    { "tues",		tDAY, 2 },
    { "wednesday",	tDAY, 3 },
    { "wednes",		tDAY, 3 },
    { "thursday",	tDAY, 4 },
    { "thur",		tDAY, 4 },
    { "thurs",		tDAY, 4 },
    { "friday",		tDAY, 5 },
    { "saturday",	tDAY, 6 },
    { NULL,		0, 0 }
};

/* Time units table. */
static TABLE const UnitsTable[] = {
    { "year",		tMONTH_UNIT,	12 },
    { "month",		tMONTH_UNIT,	1 },
    { "fortnight",	tMINUTE_UNIT,	14 * 24 * 60 },
    { "week",		tMINUTE_UNIT,	7 * 24 * 60 },
    { "day",		tMINUTE_UNIT,	1 * 24 * 60 },
    { "hour",		tMINUTE_UNIT,	60 },
    { "minute",		tMINUTE_UNIT,	1 },
    { "min",		tMINUTE_UNIT,	1 },
    { "second",		tSEC_UNIT,	1 },
    { "sec",		tSEC_UNIT,	1 },
    { NULL,		0,		0 }
};

/* Assorted relative-time words. */
static TABLE const OtherTable[] = {
    { "tomorrow",	tMINUTE_UNIT,	1 * 24 * 60 },
    { "yesterday",	tMINUTE_UNIT,	-1 * 24 * 60 },
    { "today",		tMINUTE_UNIT,	0 },
    { "now",		tMINUTE_UNIT,	0 },
    { "last",		tUNUMBER,	-1 },
    { "this",		tMINUTE_UNIT,	0 },
    { "next",		tUNUMBER,	2 },
    { "first",		tUNUMBER,	1 },
/*  { "second",		tUNUMBER,	2 }, */
    { "third",		tUNUMBER,	3 },
    { "fourth",		tUNUMBER,	4 },
    { "fifth",		tUNUMBER,	5 },
    { "sixth",		tUNUMBER,	6 },
    { "seventh",	tUNUMBER,	7 },
    { "eighth",		tUNUMBER,	8 },
    { "ninth",		tUNUMBER,	9 },
    { "tenth",		tUNUMBER,	10 },
    { "eleventh",	tUNUMBER,	11 },
    { "twelfth",	tUNUMBER,	12 },
    { "ago",		tAGO,		1 },
    { NULL,		0,		0 }
};

/* The timezone table. */
/* Some of these are commented out because a time_t can't store a float. */
static TABLE const TimezoneTable[] = {
    { "gmt",	tZONE,     HOUR( 0) },	/* Greenwich Mean */
    { "ut",	tZONE,     HOUR( 0) },	/* Universal (Coordinated) */
    { "utc",	tZONE,     HOUR( 0) },
    { "wet",	tZONE,     HOUR( 0) },	/* Western European */
    { "bst",	tDAYZONE,  HOUR( 0) },	/* British Summer */
    { "wat",	tZONE,     HOUR( 1) },	/* West Africa */
    { "at",	tZONE,     HOUR( 2) },	/* Azores */
#if	0
    /* For completeness.  BST is also British Summer, and GST is
     * also Guam Standard. */
    { "bst",	tZONE,     HOUR( 3) },	/* Brazil Standard */
    { "gst",	tZONE,     HOUR( 3) },	/* Greenland Standard */
#endif
#if 0
    { "nft",	tZONE,     HOUR(3.5) },	/* Newfoundland */
    { "nst",	tZONE,     HOUR(3.5) },	/* Newfoundland Standard */
    { "ndt",	tDAYZONE,  HOUR(3.5) },	/* Newfoundland Daylight */
#endif
    { "ast",	tZONE,     HOUR( 4) },	/* Atlantic Standard */
    { "adt",	tDAYZONE,  HOUR( 4) },	/* Atlantic Daylight */
    { "est",	tZONE,     HOUR( 5) },	/* Eastern Standard */
    { "edt",	tDAYZONE,  HOUR( 5) },	/* Eastern Daylight */
    { "cst",	tZONE,     HOUR( 6) },	/* Central Standard */
    { "cdt",	tDAYZONE,  HOUR( 6) },	/* Central Daylight */
    { "mst",	tZONE,     HOUR( 7) },	/* Mountain Standard */
    { "mdt",	tDAYZONE,  HOUR( 7) },	/* Mountain Daylight */
    { "pst",	tZONE,     HOUR( 8) },	/* Pacific Standard */
    { "pdt",	tDAYZONE,  HOUR( 8) },	/* Pacific Daylight */
    { "yst",	tZONE,     HOUR( 9) },	/* Yukon Standard */
    { "ydt",	tDAYZONE,  HOUR( 9) },	/* Yukon Daylight */
    { "hst",	tZONE,     HOUR(10) },	/* Hawaii Standard */
    { "hdt",	tDAYZONE,  HOUR(10) },	/* Hawaii Daylight */
    { "cat",	tZONE,     HOUR(10) },	/* Central Alaska */
    { "ahst",	tZONE,     HOUR(10) },	/* Alaska-Hawaii Standard */
    { "nt",	tZONE,     HOUR(11) },	/* Nome */
    { "idlw",	tZONE,     HOUR(12) },	/* International Date Line West */
    { "cet",	tZONE,     -HOUR(1) },	/* Central European */
    { "met",	tZONE,     -HOUR(1) },	/* Middle European */
    { "mewt",	tZONE,     -HOUR(1) },	/* Middle European Winter */
    { "mest",	tDAYZONE,  -HOUR(1) },	/* Middle European Summer */
    { "swt",	tZONE,     -HOUR(1) },	/* Swedish Winter */
    { "sst",	tDAYZONE,  -HOUR(1) },	/* Swedish Summer */
    { "fwt",	tZONE,     -HOUR(1) },	/* French Winter */
    { "fst",	tDAYZONE,  -HOUR(1) },	/* French Summer */
    { "eet",	tZONE,     -HOUR(2) },	/* Eastern Europe, USSR Zone 1 */
    { "bt",	tZONE,     -HOUR(3) },	/* Baghdad, USSR Zone 2 */
#if 0
    { "it",	tZONE,     -HOUR(3.5) },/* Iran */
#endif
    { "zp4",	tZONE,     -HOUR(4) },	/* USSR Zone 3 */
    { "zp5",	tZONE,     -HOUR(5) },	/* USSR Zone 4 */
#if 0
    { "ist",	tZONE,     -HOUR(5.5) },/* Indian Standard */
#endif
    { "zp6",	tZONE,     -HOUR(6) },	/* USSR Zone 5 */
#if	0
    /* For completeness.  NST is also Newfoundland Stanard, and SST is
     * also Swedish Summer. */
    { "nst",	tZONE,     -HOUR(6.5) },/* North Sumatra */
    { "sst",	tZONE,     -HOUR(7) },	/* South Sumatra, USSR Zone 6 */
#endif	/* 0 */
    { "wast",	tZONE,     -HOUR(7) },	/* West Australian Standard */
    { "wadt",	tDAYZONE,  -HOUR(7) },	/* West Australian Daylight */
#if 0
    { "jt",	tZONE,     -HOUR(7.5) },/* Java (3pm in Cronusland!) */
#endif
    { "cct",	tZONE,     -HOUR(8) },	/* China Coast, USSR Zone 7 */
    { "jst",	tZONE,     -HOUR(9) },	/* Japan Standard, USSR Zone 8 */
#if 0
    { "cast",	tZONE,     -HOUR(9.5) },/* Central Australian Standard */
    { "cadt",	tDAYZONE,  -HOUR(9.5) },/* Central Australian Daylight */
#endif
    { "east",	tZONE,     -HOUR(10) },	/* Eastern Australian Standard */
    { "eadt",	tDAYZONE,  -HOUR(10) },	/* Eastern Australian Daylight */
    { "gst",	tZONE,     -HOUR(10) },	/* Guam Standard, USSR Zone 9 */
    { "nzt",	tZONE,     -HOUR(12) },	/* New Zealand */
    { "nzst",	tZONE,     -HOUR(12) },	/* New Zealand Standard */
    { "nzdt",	tDAYZONE,  -HOUR(12) },	/* New Zealand Daylight */
    { "idle",	tZONE,     -HOUR(12) },	/* International Date Line East */
    {  NULL,	0,	   0 }
};

/* Military timezone table. */
static TABLE const MilitaryTable[] = {
    { "a",	tZONE,	HOUR(  1) },
    { "b",	tZONE,	HOUR(  2) },
    { "c",	tZONE,	HOUR(  3) },
    { "d",	tZONE,	HOUR(  4) },
    { "e",	tZONE,	HOUR(  5) },
    { "f",	tZONE,	HOUR(  6) },
    { "g",	tZONE,	HOUR(  7) },
    { "h",	tZONE,	HOUR(  8) },
    { "i",	tZONE,	HOUR(  9) },
    { "k",	tZONE,	HOUR( 10) },
    { "l",	tZONE,	HOUR( 11) },
    { "m",	tZONE,	HOUR( 12) },
    { "n",	tZONE,	HOUR(- 1) },
    { "o",	tZONE,	HOUR(- 2) },
    { "p",	tZONE,	HOUR(- 3) },
    { "q",	tZONE,	HOUR(- 4) },
    { "r",	tZONE,	HOUR(- 5) },
    { "s",	tZONE,	HOUR(- 6) },
    { "t",	tZONE,	HOUR(- 7) },
    { "u",	tZONE,	HOUR(- 8) },
    { "v",	tZONE,	HOUR(- 9) },
    { "w",	tZONE,	HOUR(-10) },
    { "x",	tZONE,	HOUR(-11) },
    { "y",	tZONE,	HOUR(-12) },
    { "z",	tZONE,	HOUR(  0) },
    { NULL,	0,	0 }
};

static int
yyerror(PARSER *lp, const char *s)
{
    (void)lp;
    (void)s;
    return 0;
}

static time_t
ToSeconds(time_t Hours, time_t Minutes, time_t Seconds, MERIDIAN Meridian)
{
    if (Minutes < 0 || Minutes > 59 || Seconds < 0 || Seconds > 59)
	return -1;
    switch (Meridian) {
    case MER24:
	if (Hours < 0 || Hours > 23)
	    return -1;
	return (Hours * 60L + Minutes) * 60L + Seconds;
    case MERam:
	if (Hours < 1 || Hours > 12)
	    return -1;
	if (Hours == 12)
	    Hours = 0;
	return (Hours * 60L + Minutes) * 60L + Seconds;
    case MERpm:
	if (Hours < 1 || Hours > 12)
	    return -1;
	if (Hours == 12)
	    Hours = 0;
	return ((Hours + 12) * 60L + Minutes) * 60L + Seconds;
    default:
	abort ();
    }
    /* NOTREACHED */
}

/*
 * Year is either:
 * - A negative number, which means to use its absolute value (why?)
 * - A number from 0 to 99, which means a year from 1900 to 1999, or
 * - The actual year (>=100).
 */
static time_t
Convert(PARSER *lp, time_t Month, time_t Day, time_t Year,
	time_t Hours, time_t Minutes, time_t Seconds,
	MERIDIAN Meridian, DSTMODE DSTmode)
{
    int DaysInMonth[12] = {
	31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    time_t	tod, Julian;
    struct tm	tm;
    int		i;

    if (Year < 0)
	Year = -Year;
    if (Year < 69)
	Year += 2000;
    else if (Year < 100)
	Year += 1900;
    DaysInMonth[1] = Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0)
		    ? 29 : 28;
    /* Checking for 2038 bogusly assumes that time_t is 32 bits.  But
       I'm too lazy to try to check for time_t overflow in another way.  */
    if (Year < EPOCH || Year > 2038
     || Month < 1 || Month > 12
     /* Lint fluff:  "conversion from long may lose accuracy" */
     || Day < 1 || Day > DaysInMonth[(int)--Month])
	return -1;

    for (Julian = Day - 1, i = 0; i < Month; i++)
	Julian += DaysInMonth[i];
    for (i = EPOCH; i < Year; i++)
	Julian += 365 + (i % 4 == 0);
    Julian *= SECSPERDAY;
    Julian += lp->yyTimezone * 60L;
    if ((tod = ToSeconds(Hours, Minutes, Seconds, Meridian)) < 0)
	return -1;
    Julian += tod;
    if (DSTmode == DSTon
     || (DSTmode == DSTmaybe && pmLocaltime(&Julian, &tm)->tm_isdst))
	Julian -= 60 * 60;
    return Julian;
}

static time_t
DSTcorrect(time_t Start, time_t Future)
{
    struct tm	tm;
    time_t	StartDay, FutureDay;

    StartDay = (pmLocaltime(&Start, &tm)->tm_hour + 1) % 24;
    FutureDay = (pmLocaltime(&Future, &tm)->tm_hour + 1) % 24;
    return (Future - Start) + (StartDay - FutureDay) * 60L * 60L;
}

static time_t
RelativeDate(time_t Start, time_t DayOrdinal, time_t DayNumber)
{
    struct tm	*tmp, tm;
    time_t	now;

    now = Start;
    tmp = pmLocaltime(&now, &tm);
    now += SECSPERDAY * ((DayNumber - tmp->tm_wday + 7) % 7);
    now += 7 * SECSPERDAY * (DayOrdinal <= 0 ? DayOrdinal : DayOrdinal - 1);
    return DSTcorrect(Start, now);
}

static time_t
RelativeMonth(PARSER *lp, time_t Start, time_t RelMonth)
{
    struct tm	*tmp, tm;
    time_t	Month, Year;

    if (RelMonth == 0)
	return 0;
    tmp = pmLocaltime(&Start, &tm);
    Month = 12 * (tmp->tm_year + 1900) + tmp->tm_mon + RelMonth;
    Year = Month / 12;
    Month = Month % 12 + 1;
    return DSTcorrect(Start,
	    Convert(lp, Month, (time_t)tmp->tm_mday, Year,
		(time_t)tmp->tm_hour, (time_t)tmp->tm_min, (time_t)tmp->tm_sec,
		MER24, DSTmaybe));
}

static int
LookupWord(union YYSTYPE *lvalp, char *buff)
{
    char	*p, *q;
    const TABLE	*tp;
    int		i, abbrev;

    /* Make it lowercase. */
    for (p = buff; *p; p++)
	if (isupper(*p))
	    *p = tolower(*p);

    if (strcmp(buff, "am") == 0 || strcmp(buff, "a.m.") == 0) {
	lvalp->Meridian = MERam;
	return tMERIDIAN;
    }
    if (strcmp(buff, "pm") == 0 || strcmp(buff, "p.m.") == 0) {
	lvalp->Meridian = MERpm;
	return tMERIDIAN;
    }

    /* See if we have an abbreviation for a month. */
    if (strlen(buff) == 3)
	abbrev = 1;
    else if (strlen(buff) == 4 && buff[3] == '.') {
	abbrev = 1;
	buff[3] = '\0';
    }
    else
	abbrev = 0;

    for (tp = MonthDayTable; tp->name; tp++) {
	if (abbrev) {
	    if (strncmp(buff, tp->name, 3) == 0) {
		lvalp->Number = tp->value;
		return tp->type;
	    }
	}
	else if (strcmp(buff, tp->name) == 0) {
	    lvalp->Number = tp->value;
	    return tp->type;
	}
    }

    for (tp = TimezoneTable; tp->name; tp++)
	if (strcmp(buff, tp->name) == 0) {
	    lvalp->Number = tp->value;
	    return tp->type;
	}

    if (strcmp(buff, "dst") == 0) 
	return tDST;

    for (tp = UnitsTable; tp->name; tp++)
	if (strcmp(buff, tp->name) == 0) {
	    lvalp->Number = tp->value;
	    return tp->type;
	}

    /* Strip off any plural and try the units table again. */
    i = strlen(buff) - 1;
    if (buff[i] == 's') {
	buff[i] = '\0';
	for (tp = UnitsTable; tp->name; tp++)
	    if (strcmp(buff, tp->name) == 0) {
		lvalp->Number = tp->value;
		return tp->type;
	    }
	buff[i] = 's';		/* Put back for "this" in OtherTable. */
    }

    for (tp = OtherTable; tp->name; tp++)
	if (strcmp(buff, tp->name) == 0) {
	    lvalp->Number = tp->value;
	    return tp->type;
	}

    /* Military timezones. */
    if (buff[1] == '\0' && isalpha(*buff)) {
	for (tp = MilitaryTable; tp->name; tp++)
	    if (strcmp(buff, tp->name) == 0) {
		lvalp->Number = tp->value;
		return tp->type;
	    }
    }

    /* Drop out any periods and try the timezone table again. */
    for (i = 0, p = q = buff; *q; q++)
	if (*q != '.')
	    *p++ = *q;
	else
	    i++;
    *p = '\0';
    if (i)
	for (tp = TimezoneTable; tp->name; tp++)
	    if (strcmp(buff, tp->name) == 0) {
		lvalp->Number = tp->value;
		return tp->type;
	    }

    return tID;
}

static int
yylex(union YYSTYPE *lvalp, PARSER *lp)
{
    char	c;
    char	*p;
    char	buff[20];
    int		Count;
    int		sign;

    for ( ; ; ) {
	while (isspace(*lp->yyInput))
	    lp->yyInput++;

	if (isdigit(c = *lp->yyInput) || c == '-' || c == '+') {
	    if (c == '-' || c == '+') {
		sign = c == '-' ? -1 : 1;
		if (!isdigit(*++lp->yyInput))
		    /* skip the '-' sign */
		    continue;
	    }
	    else
		sign = 0;
	    for (lvalp->Number = 0; isdigit(c = *lp->yyInput++); )
		lvalp->Number = 10 * lvalp->Number + c - '0';
	    lp->yyInput--;
	    if (sign < 0)
		lvalp->Number = -lvalp->Number;
	    return sign ? tSNUMBER : tUNUMBER;
	}
	if (isalpha(c)) {
	    for (p = buff; isalpha(c = *lp->yyInput++) || c == '.'; )
		if (p < &buff[sizeof buff - 1])
		    *p++ = c;
	    *p = '\0';
	    lp->yyInput--;
	    return LookupWord(lvalp, buff);
	}
	if (c != '(')
	    return *lp->yyInput++;
	Count = 0;
	do {
	    c = *lp->yyInput++;
	    if (c == '\0')
		return c;
	    if (c == '(')
		Count++;
	    else if (c == ')')
		Count--;
	} while (Count > 0);
    }
}

/* Yield A - B, measured in seconds.  */
static long
difftm(struct tm *a, struct tm *b)
{
    int ay = a->tm_year + (TM_YEAR_ORIGIN - 1);
    int by = b->tm_year + (TM_YEAR_ORIGIN - 1);
    int days = (
	      /* difference in day of year */
	      a->tm_yday - b->tm_yday
	      /* + intervening leap days */
	      +  ((ay >> 2) - (by >> 2))
	      -  (ay/100 - by/100)
	      +  ((ay/100 >> 2) - (by/100 >> 2))
	      /* + difference in years * 365 */
	      +  (long)(ay-by) * 365
	      );
    return (60*(60*(24*days + (a->tm_hour - b->tm_hour))
	      + (a->tm_min - b->tm_min))
	      + (a->tm_sec - b->tm_sec));
}

int
__pmGetDate(struct timespec *result, const char *p, struct timespec const *now)
{
    PARSER		yp;
    struct tm		*tm, tmp, *gmt_ptr, gmt;
    struct timespec	gettime_buffer;
    int			tzoff;
    time_t		Start;
    time_t		tod;

    if (!now) {
	__pmGetTimespec(&gettime_buffer);
	now = &gettime_buffer;
    }

    memset(&tmp, 0, sizeof(struct tm));
    memset(&gmt, 0, sizeof(struct tm));

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    gmt_ptr = gmtime(&now->tv_sec);
    if (gmt_ptr != NULL)
	gmt = *gmt_ptr;
    PM_UNLOCK(__pmLock_libpcp);

    if ((tm = pmLocaltime(&now->tv_sec, &tmp)) == NULL)
	return -1;

    if (gmt_ptr != NULL)
	tzoff = difftm(&gmt, tm) / 60;
    else
	tzoff = 0;

    if (tm->tm_isdst)
	tzoff += 60;

    tm = pmLocaltime(&now->tv_sec, (struct tm *)tm);
    memset(&yp, 0, sizeof(PARSER));
    yp.yyInput = (char *)p;
    yp.yyYear = tm->tm_year + 1900;
    yp.yyMonth = tm->tm_mon + 1;
    yp.yyDay = tm->tm_mday;
    yp.yyTimezone = tzoff;
    yp.yyDSTmode = DSTmaybe;
    yp.yyMeridian = MER24;
    yp.yyInput = (char *)p;

    if (yyparse(&yp) ||
	yp.yyHaveTime > 1 || yp.yyHaveZone > 1 ||
	yp.yyHaveDate > 1 || yp.yyHaveDay > 1)
	return -1;

    if (yp.yyHaveDate || yp.yyHaveTime || yp.yyHaveDay) {
	Start = Convert(&yp, yp.yyMonth, yp.yyDay, yp.yyYear,
			yp.yyHour, yp.yyMinutes, yp.yySeconds,
			yp.yyMeridian, yp.yyDSTmode);
	if (Start < 0)
	    return -1;
    }
    else {
	Start = now->tv_sec;
	if (!yp.yyHaveRel)
	    Start -= ((tm->tm_hour * 60L + tm->tm_min) * 60L) + tm->tm_sec;
    }

    Start += yp.yyRelSeconds;
    Start += RelativeMonth(&yp, Start, yp.yyRelMonth);

    if (yp.yyHaveDay && !yp.yyHaveDate) {
	tod = RelativeDate(Start, yp.yyDayOrdinal, yp.yyDayNumber);
	Start += tod;
    }

    result->tv_sec = Start;
    result->tv_nsec = 0;
    return 0;
}
