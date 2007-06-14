/* A Bison parser, made from gram.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	LSQB	257
# define	RSQB	258
# define	COMMA	259
# define	LBRACE	260
# define	RBRACE	261
# define	COLON	262
# define	SEMICOLON	263
# define	LOG	264
# define	MANDATORY	265
# define	ADVISORY	266
# define	ON	267
# define	OFF	268
# define	MAYBE	269
# define	EVERY	270
# define	ONCE	271
# define	DEFAULT	272
# define	MSEC	273
# define	SECOND	274
# define	MINUTE	275
# define	HOUR	276
# define	ACCESS	277
# define	ENQUIRE	278
# define	ALLOW	279
# define	DISALLOW	280
# define	ALL	281
# define	EXCEPT	282
# define	NAME	283
# define	STRING	284
# define	IPSPEC	285
# define	HOSTNAME	286
# define	NUMBER	287

#line 19 "gram.y"

#ident "$Id: gram.save.c,v 1.1 2004/07/02 05:52:57 kenmcd Exp $"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "pmapi.h"
#include "impl.h"
#include "./logger.h"

int		mystate = GLOBAL;

__pmHashCtl	pm_hash;
task_t		*tasklist;
fetchctl_t	*fetchroot;

static int	sts;
static int	numinst;
static int	*intlist;
static char	**extlist;
static task_t	*tp;
static fetchctl_t	*fp;
static int	numvalid;
static int	warn = 1;

extern int	lineno;
extern int	errno;


typedef struct _hl {
    struct _hl	*hl_next;
    char	*hl_name;
    int		hl_line;
} hostlist_t;

static hostlist_t	*hl_root = NULL;
static hostlist_t	*hl_last = NULL;
static hostlist_t	*hlp;
static hostlist_t	*prevhlp;
static int		opmask = 0;
static int		specmask = 0;
static int		allow;
static int		state = 0;
static char		* metricName;

#line 64 "gram.y"
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



#define	YYFINAL		82
#define	YYFLAG		-32768
#define	YYNTBASE	34

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 287 ? yytranslate[x] : 63)

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
      26,    27,    28,    29,    30,    31,    32,    33
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     3,     5,     6,     8,    11,    14,    17,    19,
      20,    24,    27,    30,    32,    34,    35,    39,    41,    43,
      45,    46,    48,    50,    52,    54,    55,    60,    62,    64,
      67,    71,    72,    76,    80,    81,    83,    86,    90,    92,
      94,    96,   101,   102,   104,   107,   113,   115,   117,   119,
     123,   125,   127,   129,   131,   133,   135,   139,   141,   145,
     147,   149
};
static const short yyrhs[] =
{
      35,    53,     0,    36,     0,     0,    37,     0,    36,    37,
       0,    38,    45,     0,    39,    40,     0,    10,     0,     0,
      41,    13,    42,     0,    41,    14,     0,    11,    15,     0,
      11,     0,    12,     0,     0,    43,    33,    44,     0,    17,
       0,    18,     0,    16,     0,     0,    19,     0,    20,     0,
      21,     0,    22,     0,     0,     6,    46,    47,     7,     0,
      48,     0,    48,     0,    47,    48,     0,    47,     5,    48,
       0,     0,    29,    49,    50,     0,     3,    51,     4,     0,
       0,    52,     0,    52,    51,     0,    52,     5,    51,     0,
      29,     0,    33,     0,    30,     0,     3,    23,     4,    54,
       0,     0,    55,     0,    55,    54,     0,    56,    57,     8,
      60,     9,     0,    25,     0,    26,     0,    58,     0,    58,
       5,    57,     0,    59,     0,    31,     0,    32,     0,    29,
       0,    61,     0,    27,     0,    27,    28,    61,     0,    62,
       0,    62,     5,    61,     0,    12,     0,    11,     0,    24,
       0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,    92,    95,    96,    99,   100,   103,   128,   142,   143,
     146,   166,   167,   176,   177,   178,   181,   182,   183,   190,
     191,   194,   195,   196,   197,   200,   200,   201,   204,   205,
     206,   209,   209,   252,   253,   256,   257,   258,   261,   262,
     263,   266,   267,   270,   271,   274,   301,   302,   305,   306,
     309,   328,   329,   330,   333,   339,   347,   355,   356,   359,
     360,   361
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "LSQB", "RSQB", "COMMA", "LBRACE", "RBRACE", 
  "COLON", "SEMICOLON", "LOG", "MANDATORY", "ADVISORY", "ON", "OFF", 
  "MAYBE", "EVERY", "ONCE", "DEFAULT", "MSEC", "SECOND", "MINUTE", "HOUR", 
  "ACCESS", "ENQUIRE", "ALLOW", "DISALLOW", "ALL", "EXCEPT", "NAME", 
  "STRING", "IPSPEC", "HOSTNAME", "NUMBER", "config", "specopt", "spec", 
  "stmt", "dowhat", "logopt", "action", "cntrl", "frequency", "everyopt", 
  "timeunits", "somemetrics", "@1", "metriclist", "metricspec", "@2", 
  "optinst", "instancelist", "instance", "accessopt", "ctllist", "ctl", 
  "allow", "hostlist", "host", "hostspec", "operation", "operlist", "op", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    34,    35,    35,    36,    36,    37,    38,    39,    39,
      40,    40,    40,    41,    41,    41,    42,    42,    42,    43,
      43,    44,    44,    44,    44,    46,    45,    45,    47,    47,
      47,    49,    48,    50,    50,    51,    51,    51,    52,    52,
      52,    53,    53,    54,    54,    55,    56,    56,    57,    57,
      58,    59,    59,    59,    60,    60,    60,    61,    61,    62,
      62,    62
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     2,     1,     0,     1,     2,     2,     2,     1,     0,
       3,     2,     2,     1,     1,     0,     3,     1,     1,     1,
       0,     1,     1,     1,     1,     0,     4,     1,     1,     2,
       3,     0,     3,     3,     0,     1,     2,     3,     1,     1,
       1,     4,     0,     1,     2,     5,     1,     1,     1,     3,
       1,     1,     1,     1,     1,     1,     3,     1,     3,     1,
       1,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       9,     8,    42,     9,     4,     0,    15,     0,     1,     5,
      25,    31,     6,    27,    13,    14,     7,     0,     0,     0,
      34,    12,    20,    11,     0,     0,    28,     0,    32,    19,
      17,    18,    10,     0,    46,    47,    41,    43,     0,     0,
      26,    29,    38,    40,    39,     0,    35,     0,    44,    53,
      51,    52,     0,    48,    50,    30,    33,     0,    36,    21,
      22,    23,    24,    16,     0,     0,    37,    60,    59,    61,
      55,     0,    54,    57,    49,     0,    45,     0,    56,    58,
       0,     0,     0
};

static const short yydefgoto[] =
{
      80,     2,     3,     4,     5,     6,    16,    17,    32,    33,
      63,    12,    19,    25,    13,    20,    28,    45,    46,     8,
      36,    37,    38,    52,    53,    54,    71,    72,    73
};

static const short yypact[] =
{
       8,-32768,    14,    31,-32768,     0,     1,   -20,-32768,-32768,
  -32768,-32768,-32768,-32768,    24,-32768,-32768,    19,    44,    23,
      50,-32768,    28,-32768,    25,    -3,-32768,   -10,-32768,-32768,
  -32768,-32768,-32768,    21,-32768,-32768,-32768,    25,    11,    23,
  -32768,-32768,-32768,-32768,-32768,    51,    -5,    16,-32768,-32768,
  -32768,-32768,    48,    52,-32768,-32768,-32768,   -10,-32768,-32768,
  -32768,-32768,-32768,-32768,     3,    11,-32768,-32768,-32768,-32768,
      30,    53,-32768,    54,-32768,    -2,-32768,    -2,-32768,-32768,
      60,    61,-32768
};

static const short yypgoto[] =
{
  -32768,-32768,-32768,    62,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,   -18,-32768,-32768,   -41,-32768,-32768,
      26,-32768,-32768,    -1,-32768,-32768,-32768,   -28,-32768
};


#define	YYLAST		65


static const short yytable[] =
{
      57,    26,    39,    18,    40,    58,    10,    41,    -3,    67,
      68,    -3,    14,    15,    67,    68,    66,     7,     1,    42,
      43,    55,    69,    44,    42,    43,    11,    69,    44,    11,
      70,    -2,    22,    23,    -2,    59,    60,    61,    62,    21,
      49,     1,    50,    51,    29,    30,    31,    78,    24,    79,
      34,    35,    11,    27,    47,    56,    64,    65,    75,    77,
      81,    82,    76,    48,    74,     9
};

static const short yycheck[] =
{
       5,    19,     5,    23,     7,    46,     6,    25,     0,    11,
      12,     3,    11,    12,    11,    12,    57,     3,    10,    29,
      30,    39,    24,    33,    29,    30,    29,    24,    33,    29,
      27,     0,    13,    14,     3,    19,    20,    21,    22,    15,
      29,    10,    31,    32,    16,    17,    18,    75,     4,    77,
      25,    26,    29,     3,    33,     4,     8,     5,    28,     5,
       0,     0,     9,    37,    65,     3
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

case 6:
#line 104 "gram.y"
{
                    mystate = GLOBAL;
                    if (numvalid) {
                        PMLC_SET_MAYBE(tp->t_state, 0);	/* clear req */
                        tp->t_next = tasklist;
                        tasklist = tp;
                        tp->t_fetch = fetchroot;
                        for (fp = fetchroot; fp != NULL; fp = fp->f_next)
                            /* link fetchctl back to task */
                            fp->f_aux = (void *)tp;

                        if (PMLC_GET_ON(state))
                            tp->t_afid = __pmAFregister(&tp->t_delta, 
                                                        (void *)tp, 
                                                        log_callback);
		    }
		    else
			free(tp);
                    
                    fetchroot = NULL;
                    state = 0;
                }
    break;
case 7:
#line 129 "gram.y"
{
                    if ((tp = (task_t *)calloc(1, sizeof(task_t))) == NULL) {
			char emess[256];
			sprintf(emess, "malloc failed: %s", strerror(errno));
			yyerror(emess);
			/*NOTREACHED*/
                    }
                    tp->t_delta.tv_sec = yyvsp[0].lval / 1000;
                    tp->t_delta.tv_usec = 1000 * (yyvsp[0].lval % 1000);
                    tp->t_state =  state;
                }
    break;
case 10:
#line 147 "gram.y"
{ 
		    char emess[256];
                    if (yyvsp[0].lval < 0) {
			sprintf(emess, 
				"Logging delta (%ld msec) must be positive",yyvsp[0].lval);
			yyerror(emess);
                        /*NOTREACHED*/
		    }
		    else if (yyvsp[0].lval >  PMLC_MAX_DELTA) {
			sprintf(emess, 
				"Logging delta (%ld msec) cannot be bigger "
				"than %d msec", yyvsp[0].lval, PMLC_MAX_DELTA);
			yyerror(emess);
			/*NOTREACHED*/
		    }

                    PMLC_SET_ON(state, 1); 
                    yyval.lval = yyvsp[0].lval;
                }
    break;
case 11:
#line 166 "gram.y"
{ PMLC_SET_ON(state, 0);yyval.lval = 0;}
    break;
case 12:
#line 168 "gram.y"
{
                    PMLC_SET_MAND(state, 0);
                    PMLC_SET_ON(state, 0);
                    PMLC_SET_MAYBE(state, 1);
                    yyval.lval = 0;
                }
    break;
case 13:
#line 176 "gram.y"
{ PMLC_SET_MAND(state, 1); }
    break;
case 14:
#line 177 "gram.y"
{ PMLC_SET_MAND(state, 0); }
    break;
case 15:
#line 178 "gram.y"
{ PMLC_SET_MAND(state, 0); }
    break;
case 16:
#line 181 "gram.y"
{ yyval.lval = yyvsp[-1].lval*yyvsp[0].lval; }
    break;
case 17:
#line 182 "gram.y"
{ yyval.lval = 0; }
    break;
case 18:
#line 184 "gram.y"
{ 
                    extern struct timeval delta; /* default logging interval */
                    yyval.lval = delta.tv_sec*1000 + delta.tv_usec/1000;
                }
    break;
case 21:
#line 194 "gram.y"
{ yyval.lval = 1; }
    break;
case 22:
#line 195 "gram.y"
{ yyval.lval = 1000; }
    break;
case 23:
#line 196 "gram.y"
{ yyval.lval = 60000; }
    break;
case 24:
#line 197 "gram.y"
{ yyval.lval = 3600000; }
    break;
case 25:
#line 200 "gram.y"
{ numvalid = 0; mystate = INSPEC; }
    break;
case 31:
#line 210 "gram.y"
{ 
                    if ((metricName = strdup(yyvsp[0].str)) == NULL) {
			char emess[256];
			sprintf(emess, "malloc failed: %s", strerror(errno));
                        yyerror(emess);
			/*NOTREACHED*/
		    }
                }
    break;
case 32:
#line 219 "gram.y"
{
		    /*
		     * search cache for previously seen metrics for this task
		     */
		    int		j;
		    for (j = 0; j < tp->t_numpmid; j++) {
			if (tp->t_namelist[j] != NULL &&
			    strcmp(tp->t_namelist[j], metricName) == 0) {
			    break;
			}
		    }
		    if (j < tp->t_numpmid) {
			/* found in cache */
			dometric(metricName);
		    }
		    else {
		        /*
			 * either a new metric, and so it may be a
			 * non-terminal in the PMNS
			 */
			if ((sts = pmTraversePMNS(metricName, dometric)) < 0 ) {
			    char emess[256];
			    sprintf(emess, "Problem with lookup for metric \"%s\" "
					    "... logging not activated",metricName);
			    yywarn(emess);
			    fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
			}
		    }
                    freeinst(&numinst, intlist, extlist);
                    free (metricName);
                }
    break;
case 38:
#line 261 "gram.y"
{ buildinst(&numinst, &intlist, &extlist, -1, yyvsp[0].str); }
    break;
case 39:
#line 262 "gram.y"
{ buildinst(&numinst, &intlist, &extlist, yyvsp[0].lval, NULL); }
    break;
case 40:
#line 263 "gram.y"
{ buildinst(&numinst, &intlist, &extlist, -1, yyvsp[0].str); }
    break;
case 45:
#line 275 "gram.y"
{
                    prevhlp = NULL;
                    for (hlp = hl_root; hlp != NULL; hlp = hlp->hl_next) {
                        if (prevhlp != NULL) {
                            free(prevhlp->hl_name);
                            free(prevhlp);
                        }
                        sts = __pmAccAddHost(hlp->hl_name, specmask, 
                                             opmask, 0);
                        if (sts < 0) {
                            fprintf(stderr, "error was on line %d\n", 
                                hlp->hl_line);
                            YYABORT;
                        }
                        prevhlp = hlp;
                    }
                    if (prevhlp != NULL) {
                        free(prevhlp->hl_name);
                        free(prevhlp);
                    }
                    opmask = 0;
                    specmask = 0;
                    hl_root = hl_last = NULL;
                }
    break;
case 46:
#line 301 "gram.y"
{ allow = 1; }
    break;
case 47:
#line 302 "gram.y"
{ allow = 0; }
    break;
case 50:
#line 310 "gram.y"
{
                    hlp = (hostlist_t *)malloc(sts = sizeof(hostlist_t));
                    if (hlp == NULL) {
                        __pmNoMem("adding new host", sts, PM_FATAL_ERR);
                        /*NOTREACHED*/
                    }
                    if (hl_last != NULL) {
                        hl_last->hl_next = hlp;
                        hl_last = hlp;
                    }
                    else
                        hl_root = hl_last = hlp;
                    hlp->hl_next = NULL;
                    hlp->hl_name = strdup(yyvsp[0].str);
                    hlp->hl_line = lineno;
                }
    break;
case 54:
#line 334 "gram.y"
{
                    specmask = opmask;
                    if (allow)
                        opmask = ~opmask;
                }
    break;
case 55:
#line 340 "gram.y"
{
                    specmask = PM_OP_ALL;
                    if (allow)
                        opmask = PM_OP_NONE;
                    else
                        opmask = PM_OP_ALL;
                }
    break;
case 56:
#line 348 "gram.y"
{
                    specmask = PM_OP_ALL;
                    if (!allow)
                        opmask = ~opmask;
                }
    break;
case 59:
#line 359 "gram.y"
{ opmask |= PM_OP_LOG_ADV; }
    break;
case 60:
#line 360 "gram.y"
{ opmask |= PM_OP_LOG_MAND; }
    break;
case 61:
#line 361 "gram.y"
{ opmask |= PM_OP_LOG_ENQ; }
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
#line 364 "gram.y"


/*
 * Assumed calling context ...
 *	tp		the correct task for the requested metric
 *	numinst		number of instances associated with this request
 *	extlist[]	external instance names if numinst > 0
 *	intlist[]	internal instance identifier if numinst > 0 and
 *			corresponding extlist[] entry is NULL
 */

void
dometric(const char *name)
{
    int		sts;
    int		inst;
    int		i;
    int		j;
    int		dup = -1;
    int		skip;
    pmID	pmid;
    pmDesc	*dp;
    optreq_t	*rqp;
    extern char	*chk_emess[];
	char emess[1024];

    /*
     * search cache for previously seen metrics for this task
     */
    for (j = 0; j < tp->t_numpmid; j++) {
	if (tp->t_namelist[j] != NULL &&
	    strcmp(tp->t_namelist[j], name) == 0) {
	    dup = j;
	    break;
	}
    }

    /*
     * need new malloc'd pmDesc, even if metric found in cache
     */
    dp = (pmDesc *)malloc(sizeof(pmDesc));
    if (dp == NULL)
	goto nomem;

    if (dup == -1) {

	/* Cast away const, pmLookupName should never modify name */
	if ((sts = pmLookupName(1, (char **)&name, &pmid)) < 0 || pmid == PM_ID_NULL) {
	   sprintf(emess, "Metric \"%s\" is unknown ... not logged", name);
	    goto defer;
	}

	if ((sts = pmLookupDesc(pmid, dp)) < 0) {
	    sprintf(emess, "Description unavailable for metric \"%s\" ... not logged", name);

	    goto defer;
	}
    }
    else {
	*dp = tp->t_desclist[dup];
	pmid = tp->t_pmidlist[dup];
    }

    tp->t_numpmid++;
    tp->t_pmidlist = (pmID *)realloc(tp->t_pmidlist, tp->t_numpmid * sizeof(pmID));
    if (tp->t_pmidlist == NULL)
	goto nomem;
    tp->t_namelist = (char **)realloc(tp->t_namelist, tp->t_numpmid * sizeof(char *));
    if (tp->t_namelist == NULL)
	goto nomem;
    tp->t_desclist = (pmDesc *)realloc(tp->t_desclist, tp->t_numpmid * sizeof(pmDesc));
    if (tp->t_desclist == NULL)
	goto nomem;
    if ((tp->t_namelist[tp->t_numpmid-1] = strdup(name)) == NULL)
	goto nomem;
    tp->t_pmidlist[tp->t_numpmid-1] = pmid;
    tp->t_desclist[tp->t_numpmid-1] = *dp;	/* struct assignment */

    rqp = (optreq_t *)calloc(1, sizeof(optreq_t));
    if (rqp == NULL)
	goto nomem;
    rqp->r_desc = dp;
    rqp->r_numinst = numinst;
    skip = 0;
    if (numinst) {
	/*
	 * malloc here, and keep ... gets buried in optFetch data structures
	 */
	rqp->r_instlist = (int *)malloc(numinst * sizeof(rqp->r_instlist[0]));
	if (rqp->r_instlist == NULL)
	    goto nomem;
	j = 0;
	for (i = 0; i < numinst; i++) {
	    if (extlist[i] != NULL) {
		sts = pmLookupInDom(dp->indom, extlist[i]);
		if (sts < 0) {
			sprintf(emess, "Instance \"%s\" is not defined for the metric \"%s\"", extlist[i], name);
			yywarn(emess);
		    rqp->r_numinst--;
		    continue;
		}
		inst = sts;
	    }
	    else {
		char	*p;
		sts = pmNameInDom(dp->indom, intlist[i], &p);
		if (sts < 0) {
           sprintf(emess, "Instance \"%d\" is not defined for the metric \"%s\"", intlist[i], name);
           yywarn(emess);

		    rqp->r_numinst--;
		    continue;
		}
		free(p);
		inst = intlist[i];
	    }
	    if ((sts = chk_one(tp, pmid, inst)) < 0) {
			sprintf(emess, "Incompatible request for metric \"%s\" and instance \"%s\"", name, extlist[i]);
			yywarn(emess);

			fprintf(stderr, "%s\n", chk_emess[-sts]);
			rqp->r_numinst--;
	    }
	    else
		rqp->r_instlist[j++] = inst;
	}
	if (rqp->r_numinst == 0)
	    skip = 1;
    }
    else {
	if ((sts = chk_all(tp, pmid)) < 0) {
            sprintf(emess, "Incompatible request for metric \"%s\"", name);
            yywarn(emess);

	    skip = 1;
	}
    }

    if (!skip) {
	if ((sts = __pmOptFetchAdd(&fetchroot, rqp)) < 0) {
       sprintf(emess, "__pmOptFetchAdd failed for metric \"%s\" ... logging not activated", name);
	    goto snarf;
	}

	if ((sts = __pmHashAdd(pmid, (void *)rqp, &pm_hash)) < 0) {
       sprintf(emess, "__pmHashAdd failed for metric \"%s\" ... logging not activated", name);

	    goto snarf;
	}
	numvalid++;
    }
    else {
	free(dp);
	free(rqp);
    }

    return;

defer:
    /* EXCEPTION PCP 2.0
     * The idea here is that we will sort all logging request into "good" and
     * "bad" (like pmie) ... the "bad" ones are "deferred" and at some point
     * later pmlogger would (periodically) revisit the "deferred" ones and see
     * if they can be added to the "good" set.
     */
    if (warn) {
        yywarn(emess);
        fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
    }
    if (dp != NULL)
        free(dp);
    return;

nomem:
    sprintf(emess, "malloc failed: %s", strerror(errno));
    yyerror(emess);
    /*NOTREACHED*/

snarf:
    yywarn(emess);
    fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
    free(dp);
    return;
}
