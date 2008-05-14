/* A Bison parser, made from gram.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	LSQB	257
# define	RSQB	258
# define	COMMA	259
# define	LBRACE	260
# define	RBRACE	261
# define	AT	262
# define	EOL	263
# define	LOG	264
# define	MANDATORY	265
# define	ADVISORY	266
# define	ON	267
# define	OFF	268
# define	MAYBE	269
# define	EVERY	270
# define	ONCE	271
# define	MSEC	272
# define	SECOND	273
# define	MINUTE	274
# define	HOUR	275
# define	QUERY	276
# define	SHOW	277
# define	LOGGER	278
# define	CONNECT	279
# define	PRIMARY	280
# define	QUIT	281
# define	STATUS	282
# define	HELP	283
# define	TIMEZONE	284
# define	LOCAL	285
# define	PORT	286
# define	NEW	287
# define	VOLUME	288
# define	SYNC	289
# define	NAME	290
# define	HOSTNAME	291
# define	STRING	292
# define	NUMBER	293

#line 5 "gram.y"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "pmapi.h"
#include "impl.h"
#include "./pmlc.h"

#ifdef YYDEBUG
int yydebug=1;
#endif

int	mystate = GLOBAL;
int	logfreq;
int	parse_stmt;
char	emess[160];
char	*hostname;
int	state;
int	control;

static int	sts;

extern int	port;
extern int	pid;

extern int	errno;


#line 37 "gram.y"
#ifndef YYSTYPE
typedef union {
    long lval;
    char * str;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		86
#define	YYFLAG		-32768
#define	YYNTBASE	40

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 293 ? yytranslate[x] : 62)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     1,     2,     8,    13,    18,    21,    24,    27,
      31,    35,    38,    40,    41,    43,    45,    50,    54,    58,
      60,    61,    63,    65,    69,    71,    73,    74,    76,    78,
      80,    82,    83,    88,    90,    92,    95,    99,   100,   104,
     108,   109,   111,   114,   118,   120,   122,   124,   126,   127,
     130,   133,   136,   139,   140,   142,   144,   147,   149,   151
};
static const short yyrhs[] =
{
      -1,     0,    43,    41,    50,    42,     9,     0,    23,    58,
      59,     9,     0,    25,    60,    59,     9,     0,    29,     9,
       0,    27,     9,     0,    28,     9,     0,    33,    34,     9,
       0,    30,    61,     9,     0,    35,     9,     0,     9,     0,
       0,    44,     0,    22,     0,    45,    46,    13,    47,     0,
      45,    46,    14,     0,    45,    11,    15,     0,    10,     0,
       0,    11,     0,    12,     0,    48,    39,    49,     0,    17,
       0,    16,     0,     0,    18,     0,    19,     0,    20,     0,
      21,     0,     0,     6,    51,    52,     7,     0,    53,     0,
      53,     0,    52,    53,     0,    52,     5,    53,     0,     0,
      36,    54,    55,     0,     3,    56,     4,     0,     0,    57,
       0,    57,    56,     0,    57,     5,    56,     0,    36,     0,
      39,     0,    38,     0,    24,     0,     0,     8,    36,     0,
       8,    37,     0,     8,    39,     0,     8,    38,     0,     0,
      26,     0,    39,     0,    32,    39,     0,    31,     0,    24,
       0,    38,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,    68,    68,    68,    92,    97,   102,   107,   112,   117,
     122,   127,   132,   137,   140,   143,   144,   145,   146,   153,
     154,   157,   158,   161,   162,   165,   166,   169,   170,   171,
     172,   175,   175,   176,   179,   180,   181,   184,   184,   209,
     210,   213,   214,   215,   218,   219,   220,   223,   224,   227,
     228,   229,   236,   237,   240,   241,   242,   245,   246,   247
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "LSQB", "RSQB", "COMMA", "LBRACE", "RBRACE", 
  "AT", "EOL", "LOG", "MANDATORY", "ADVISORY", "ON", "OFF", "MAYBE", 
  "EVERY", "ONCE", "MSEC", "SECOND", "MINUTE", "HOUR", "QUERY", "SHOW", 
  "LOGGER", "CONNECT", "PRIMARY", "QUIT", "STATUS", "HELP", "TIMEZONE", 
  "LOCAL", "PORT", "NEW", "VOLUME", "SYNC", "NAME", "HOSTNAME", "STRING", 
  "NUMBER", "stmt", "@1", "@2", "dowhat", "action", "logopt", "cntrl", 
  "frequency", "everyopt", "timeunits", "somemetrics", "@3", "metriclist", 
  "metricspec", "@4", "optinst", "instancelist", "instance", "loggersopt", 
  "hostopt", "towhom", "tzspec", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    41,    42,    40,    40,    40,    40,    40,    40,    40,
      40,    40,    40,    40,    43,    44,    44,    44,    44,    45,
      45,    46,    46,    47,    47,    48,    48,    49,    49,    49,
      49,    51,    50,    50,    52,    52,    52,    54,    53,    55,
      55,    56,    56,    56,    57,    57,    57,    58,    58,    59,
      59,    59,    59,    59,    60,    60,    60,    61,    61,    61
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     0,     0,     5,     4,     4,     2,     2,     2,     3,
       3,     2,     1,     0,     1,     1,     4,     3,     3,     1,
       0,     1,     1,     3,     1,     1,     0,     1,     1,     1,
       1,     0,     4,     1,     1,     2,     3,     0,     3,     3,
       0,     1,     2,     3,     1,     1,     1,     1,     0,     2,
       2,     2,     2,     0,     1,     1,     2,     1,     1,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
      20,    12,    19,    15,    48,     0,     0,     0,     0,     0,
       0,     0,     1,    14,     0,    47,    53,    54,     0,    55,
      53,     7,     8,     6,    58,    57,    59,     0,     0,    11,
       0,    21,    22,     0,     0,     0,    56,     0,    10,     9,
      31,    37,     2,    33,    18,    26,    17,    49,    50,    52,
      51,     4,     5,     0,    40,     0,    25,    24,    16,     0,
       0,    34,     0,    38,     3,     0,     0,    32,    35,    44,
      46,    45,     0,    41,    27,    28,    29,    30,    23,    36,
      39,     0,    42,    43,     0,     0,     0
};

static const short yydefgoto[] =
{
      84,    30,    55,    12,    13,    14,    33,    58,    59,    78,
      42,    53,    60,    43,    54,    63,    72,    73,    16,    35,
      20,    27
};

static const short yypact[] =
{
       0,-32768,-32768,-32768,   -20,   -24,    -3,    12,    17,   -19,
      14,    23,-32768,-32768,    40,-32768,    47,-32768,    18,-32768,
      47,-32768,-32768,-32768,-32768,-32768,-32768,    49,    50,-32768,
      -5,    41,-32768,     4,     2,    51,-32768,    52,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,    37,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,    26,    60,    55,-32768,-32768,-32768,    27,
       6,-32768,    11,-32768,-32768,    25,    26,-32768,-32768,-32768,
  -32768,-32768,    61,    -2,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,    11,-32768,-32768,    67,    68,-32768
};

static const short yypgoto[] =
{
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,   -46,-32768,-32768,   -57,-32768,-32768,    53,
  -32768,-32768
};


#define	YYLAST		73


static const short yytable[] =
{
     -13,    40,    17,    81,    15,    24,    21,    61,    18,     1,
       2,    66,    25,    67,    68,    19,    82,    45,    46,    26,
      79,    22,     3,     4,    83,     5,    23,     6,     7,     8,
       9,    41,    29,    10,    69,    11,    70,    71,    47,    48,
      49,    50,    41,    74,    75,    76,    77,    69,    28,    70,
      71,    31,    32,    56,    57,    34,    44,    36,    38,    39,
      51,    52,    41,    62,    64,    80,    65,    85,    86,     0,
       0,     0,     0,    37
};

static const short yycheck[] =
{
       0,     6,    26,     5,    24,    24,     9,    53,    32,     9,
      10,     5,    31,     7,    60,    39,    73,    13,    14,    38,
      66,     9,    22,    23,    81,    25,     9,    27,    28,    29,
      30,    36,     9,    33,    36,    35,    38,    39,    36,    37,
      38,    39,    36,    18,    19,    20,    21,    36,    34,    38,
      39,    11,    12,    16,    17,     8,    15,    39,     9,     9,
       9,     9,    36,     3,     9,     4,    39,     0,     0,    -1,
      -1,    -1,    -1,    20
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison/bison.simple"

/* Skeleton output parser for bison,

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser when
   the %semantic_parser declaration is not specified in the grammar.
   It was written by Richard Stallman by simplifying the hairy parser
   used when %semantic_parser is specified.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (yyoverflow) || defined (YYERROR_VERBOSE)

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
# if YYLSP_NEEDED
  YYLTYPE yyls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# if YYLSP_NEEDED
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAX)
# else
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)
# endif

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif


#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).

   When YYLLOC_DEFAULT is run, CURRENT is set the location of the
   first token.  By default, to implement support for ranges, extend
   its range to the last symbol.  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, &yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval, &yylloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			yylex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

#ifdef YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else /* !YYPARSE_PARAM */
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif /* !YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int yychar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE yylval;						\
							\
/* Number of parse errors so far.  */			\
int yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yychar1 = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
# define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  YYSIZE_T yystacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
#if YYLSP_NEEDED
  YYLTYPE yyloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
#if YYLSP_NEEDED
  yylsp = yyls;
#endif
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *yyls1 = yyls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
# else
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);
# endif
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (yyls);
# endif
# undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
#if YYLSP_NEEDED
      yylsp = yyls + yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  yyloc = yylsp[1-yylen];
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] > 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif

  switch (yyn) {

case 1:
#line 69 "gram.y"
{
		    mystate |= INSPEC;
		    if (!connected()) {
			metric_cnt = -1;
			return 0;
		    }
		    if (ConnectPMCD()) {
			yyerror("");
			metric_cnt = -1;
			return 0;
		    }
		    beginmetrics();
		}
    break;
case 2:
#line 83 "gram.y"
{
		    mystate = GLOBAL;
		    endmetrics();
		}
    break;
case 3:
#line 88 "gram.y"
{
		    parse_stmt = LOG;
		    YYACCEPT;
		}
    break;
case 4:
#line 93 "gram.y"
{
		    parse_stmt = SHOW;
		    YYACCEPT;
		}
    break;
case 5:
#line 98 "gram.y"
{
		    parse_stmt = CONNECT;
		    YYACCEPT;
		}
    break;
case 6:
#line 103 "gram.y"
{
		    parse_stmt = HELP;
		    YYACCEPT;
		}
    break;
case 7:
#line 108 "gram.y"
{
		    parse_stmt = QUIT;
		    YYACCEPT;
		}
    break;
case 8:
#line 113 "gram.y"
{
		    parse_stmt = STATUS;
		    YYACCEPT;
		}
    break;
case 9:
#line 118 "gram.y"
{
		    parse_stmt = NEW;
		    YYACCEPT;
		}
    break;
case 10:
#line 123 "gram.y"
{
		    parse_stmt = TIMEZONE;
		    YYACCEPT;
		}
    break;
case 11:
#line 128 "gram.y"
{
		    parse_stmt = SYNC;
		    YYACCEPT;
		}
    break;
case 12:
#line 133 "gram.y"
{
		    parse_stmt = 0;
		    YYACCEPT;
		}
    break;
case 13:
#line 137 "gram.y"
{ YYERROR; }
    break;
case 15:
#line 143 "gram.y"
{ state = PM_LOG_ENQUIRE; }
    break;
case 16:
#line 144 "gram.y"
{ state = PM_LOG_ON; }
    break;
case 17:
#line 145 "gram.y"
{ state = PM_LOG_OFF; }
    break;
case 18:
#line 147 "gram.y"
{
		    control = PM_LOG_MANDATORY;
		    state = PM_LOG_MAYBE;
		}
    break;
case 21:
#line 157 "gram.y"
{ control = PM_LOG_MANDATORY; }
    break;
case 22:
#line 158 "gram.y"
{ control = PM_LOG_ADVISORY; }
    break;
case 23:
#line 161 "gram.y"
{ logfreq = yyvsp[-1].lval * yyvsp[0].lval; }
    break;
case 24:
#line 162 "gram.y"
{ logfreq = -1; }
    break;
case 27:
#line 169 "gram.y"
{ yyval.lval = 1; }
    break;
case 28:
#line 170 "gram.y"
{ yyval.lval = 1000; }
    break;
case 29:
#line 171 "gram.y"
{ yyval.lval = 60000; }
    break;
case 30:
#line 172 "gram.y"
{ yyval.lval = 3600000; }
    break;
case 31:
#line 175 "gram.y"
{ mystate |= INSPECLIST; }
    break;
case 37:
#line 185 "gram.y"
{
		    beginmetgrp();
		    if ((sts = pmTraversePMNS(yyvsp[0].str, addmetric)) < 0)
			/* metric_cnt is set by addmetric but if
			 * traversePMNS fails, set it so that the bad
			 * news is visible to other routines
			 */
			metric_cnt = sts;
		    else if (metric_cnt < 0) /* addmetric failed */
			sts = metric_cnt;

		    if (sts < 0 || metric_cnt == 0) {
			sprintf(emess, 
				"Problem with lookup for metric \"%s\" ...",yyvsp[0].str);
			yywarn(emess);
			if (sts < 0) {
			    fprintf(stderr, "Reason: ");
			    fprintf(stderr, "%s\n", pmErrStr(sts));
			}
		    }
		}
    break;
case 38:
#line 206 "gram.y"
{ endmetgrp(); }
    break;
case 44:
#line 218 "gram.y"
{ addinst(yyvsp[0].str, 0); }
    break;
case 45:
#line 219 "gram.y"
{ addinst(NULL, yyvsp[0].lval); }
    break;
case 46:
#line 220 "gram.y"
{ addinst(yyvsp[0].str, 0); }
    break;
case 49:
#line 227 "gram.y"
{ hostname = strdup(yyvsp[0].str); }
    break;
case 50:
#line 228 "gram.y"
{ hostname = strdup(yyvsp[0].str); }
    break;
case 51:
#line 230 "gram.y"
{ 
			/* That MUST be a mistake! */
			char tb[64];
			sprintf (tb, "%d", (int)yyvsp[0].lval);
			hostname = strdup(tb); 
		}
    break;
case 52:
#line 236 "gram.y"
{ hostname = strdup(yyvsp[0].str); }
    break;
case 54:
#line 240 "gram.y"
{ pid = PM_LOG_PRIMARY_PID; port = PM_LOG_NO_PORT; }
    break;
case 55:
#line 241 "gram.y"
{ pid = yyvsp[0].lval; port = PM_LOG_NO_PORT; }
    break;
case 56:
#line 242 "gram.y"
{ pid = PM_LOG_NO_PID; port = yyvsp[0].lval; }
    break;
case 57:
#line 245 "gram.y"
{ tztype = TZ_LOCAL; }
    break;
case 58:
#line 246 "gram.y"
{ tztype = TZ_LOGGER; }
    break;
case 59:
#line 248 "gram.y"
{
		    tztype = TZ_OTHER;
		    /* ignore the quotes: skip the leading one and
		     * clobber the trailing one with a null to
		     * terminate the string really required.
		     */
		    if (tz != NULL)
			free(tz);
		    if ((tz = strdup(yyvsp[0].str)) == NULL) {
			__pmNoMem("setting up timezone",
				 strlen(yyvsp[0].str), PM_FATAL_ERR);
		    }
		}
    break;
}

#line 705 "/usr/share/bison/bison.simple"


  yyvsp -= yylen;
  yyssp -= yylen;
#if YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;
#if YYLSP_NEEDED
  *++yylsp = yyloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[YYTRANSLATE (yychar)]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[YYTRANSLATE (yychar)]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*--------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
yyerrlab1:
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;


/*-------------------------------------------------------------------.
| yyerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
yyerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  yyn = yydefact[yystate];
  if (yyn)
    goto yydefault;
#endif


/*---------------------------------------------------------------.
| yyerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
yyerrpop:
  if (yyssp == yyss)
    YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#if YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| yyerrhandle.  |
`--------------*/
yyerrhandle:
  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

/*---------------------------------------------.
| yyoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}
#line 264 "gram.y"


extern char	*configfile;
extern int	lineno;

void
yywarn(char *s)
{
    fprintf(stderr, "Warning [%s, line %d]\n",
	    configfile == NULL ? "<stdin>" : configfile, lineno);
    if (s != NULL && s[0] != '\0')
	fprintf(stderr, "%s\n", s);
}

void
yyerror(char *s)
{
    fprintf(stderr, "Error [%s, line %d]\n",
	    configfile == NULL ? "<stdin>" : configfile, lineno);
    if (s != NULL && s[0] != '\0')
	fprintf(stderr, "%s\n", s);

    skipAhead ();
    yyclearin;
    mystate = GLOBAL;
}
