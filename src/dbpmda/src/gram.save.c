/* A Bison parser, made from gram.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	NUMBER2D	257
# define	NUMBER3D	258
# define	NUMBER	259
# define	NEGNUMBER	260
# define	FLAG	261
# define	NAME	262
# define	PATHNAME	263
# define	MACRO	264
# define	STRING	265
# define	COMMA	266
# define	EQUAL	267
# define	OPEN	268
# define	CLOSE	269
# define	DESC	270
# define	GETDESC	271
# define	FETCH	272
# define	INSTANCE	273
# define	PROFILE	274
# define	HELP	275
# define	WATCH	276
# define	DBG	277
# define	QUIT	278
# define	STATUS	279
# define	STORE	280
# define	TEXT	281
# define	TIMER	282
# define	NAMESPACE	283
# define	WAIT	284
# define	DSO	285
# define	PIPE	286
# define	ADD	287
# define	DELETE	288
# define	ALL	289
# define	NONE	290
# define	INDOM	291
# define	ON	292
# define	OFF	293
# define	PLUS	294
# define	EOL	295

#line 24 "gram.y"

#ident "$Id: gram.save.c,v 1.1 2004/07/02 05:52:57 kenmcd Exp $"

#include <stdio.h>
#include "./dbpmda.h"
#include "./lex.h"

extern int stmt_type;

static union {
    pmID	whole;
    __pmID_int	part;
} pmid;

static union {
    pmInDom		whole;
    __pmInDom_int	part;
} indom;


static int	sts;
static int	inst;
static char	*str;
static char	warnStr[80];

param_t	param;



#line 54 "gram.y"
#ifndef YYSTYPE
typedef union {
	char		*y_str;
	int		y_num;
	twodot_num      y_2num;
	threedot_num    y_3num;
	} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		147
#define	YYFLAG		-32768
#define	YYNTBASE	42

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 295 ? yytranslate[x] : 51)

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
      36,    37,    38,    39,    40,    41
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     3,    10,    16,    19,    22,    26,    29,    33,
      36,    41,    44,    48,    53,    56,    60,    65,    70,    73,
      78,    83,    89,    95,    98,   102,   106,   109,   113,   117,
     121,   125,   129,   133,   137,   141,   145,   149,   153,   157,
     161,   165,   169,   173,   176,   179,   183,   187,   191,   194,
     197,   201,   205,   208,   212,   216,   219,   223,   225,   226,
     228,   230,   232,   233,   235,   237,   239,   241,   243,   245,
     247,   249,   252,   256,   257,   259,   261,   263,   265,   268
};
static const short yyrhs[] =
{
      14,    41,     0,    14,    31,    43,     8,    44,    41,     0,
      14,    32,    43,    48,    41,     0,    15,    41,     0,    16,
      41,     0,    16,    45,    41,     0,    18,    41,     0,    18,
      47,    41,     0,    26,    41,     0,    26,    45,    11,    41,
       0,    27,    41,     0,    27,    45,    41,     0,    27,    37,
      46,    41,     0,    19,    41,     0,    19,    46,    41,     0,
      19,    46,     5,    41,     0,    19,    46,    49,    41,     0,
      20,    41,     0,    20,    46,    35,    41,     0,    20,    46,
      36,    41,     0,    20,    46,    33,     5,    41,     0,    20,
      46,    34,     5,    41,     0,    22,    41,     0,    22,    43,
      41,     0,    29,    43,    41,     0,    21,    41,     0,    21,
      14,    41,     0,    21,    15,    41,     0,    21,    16,    41,
       0,    21,    18,    41,     0,    21,    17,    41,     0,    21,
      19,    41,     0,    21,    29,    41,     0,    21,    20,    41,
       0,    21,    22,    41,     0,    21,    23,    41,     0,    21,
      24,    41,     0,    21,    25,    41,     0,    21,    26,    41,
       0,    21,    27,    41,     0,    21,    28,    41,     0,    21,
      30,    41,     0,    24,    41,     0,    23,    41,     0,    23,
      35,    41,     0,    23,    36,    41,     0,    23,    50,    41,
       0,    25,    41,     0,    28,    41,     0,    28,    38,    41,
       0,    28,    39,    41,     0,    17,    41,     0,    17,    38,
      41,     0,    17,    39,    41,     0,    30,    41,     0,    30,
       5,    41,     0,    41,     0,     0,     8,     0,     9,     0,
       5,     0,     0,     5,     0,     3,     0,     4,     0,     8,
       0,     5,     0,     6,     0,     3,     0,    45,     0,    47,
      45,     0,    47,    12,    45,     0,     0,    11,     0,     8,
       0,     5,     0,     8,     0,     5,    50,     0,     8,    50,
       0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,    98,   102,   106,   110,   113,   117,   121,   125,   128,
     132,   137,   141,   146,   151,   155,   161,   167,   173,   177,
     187,   197,   208,   219,   223,   227,   231,   235,   239,   243,
     247,   251,   255,   259,   263,   267,   271,   275,   279,   283,
     287,   291,   295,   299,   300,   304,   308,   312,   316,   319,
     323,   326,   329,   333,   336,   339,   343,   346,   347,   357,
     358,   361,   362,   365,   377,   391,   406,   416,   420,   424,
     432,   433,   434,   438,   441,   442,   445,   446,   455,   456
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "NUMBER2D", "NUMBER3D", "NUMBER", 
  "NEGNUMBER", "FLAG", "NAME", "PATHNAME", "MACRO", "STRING", "COMMA", 
  "EQUAL", "OPEN", "CLOSE", "DESC", "GETDESC", "FETCH", "INSTANCE", 
  "PROFILE", "HELP", "WATCH", "DBG", "QUIT", "STATUS", "STORE", "TEXT", 
  "TIMER", "NAMESPACE", "WAIT", "DSO", "PIPE", "ADD", "DELETE", "ALL", 
  "NONE", "INDOM", "ON", "OFF", "PLUS", "EOL", "stmt", "fname", 
  "optdomain", "metric", "indom", "metriclist", "arglist", "inst", 
  "debug", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    42,    42,    42,    42,    42,    42,    42,    42,    42,
      42,    42,    42,    42,    42,    42,    42,    42,    42,    42,
      42,    42,    42,    42,    42,    42,    42,    42,    42,    42,
      42,    42,    42,    42,    42,    42,    42,    42,    42,    42,
      42,    42,    42,    42,    42,    42,    42,    42,    42,    42,
      42,    42,    42,    42,    42,    42,    42,    42,    42,    43,
      43,    44,    44,    45,    45,    45,    45,    46,    46,    46,
      47,    47,    47,    48,    49,    49,    50,    50,    50,    50
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     2,     6,     5,     2,     2,     3,     2,     3,     2,
       4,     2,     3,     4,     2,     3,     4,     4,     2,     4,
       4,     5,     5,     2,     3,     3,     2,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     2,     2,     3,     3,     3,     2,     2,
       3,     3,     2,     3,     3,     2,     3,     1,     0,     1,
       1,     1,     0,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     3,     0,     1,     1,     1,     1,     2,     2
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
      58,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    57,     0,
       0,     1,     4,    64,    65,    63,    66,     5,     0,     0,
       0,    52,     7,    70,     0,    69,    67,    68,    14,     0,
      18,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    26,    59,
      60,    23,     0,    76,    77,     0,     0,    44,     0,    43,
      48,     9,     0,     0,    11,     0,     0,     0,    49,     0,
       0,    55,     0,    73,     6,    53,    54,     0,     8,    71,
       0,    75,    74,    15,     0,     0,     0,     0,     0,    27,
      28,    29,    31,    30,    32,    34,    35,    36,    37,    38,
      39,    40,    41,    33,    42,    24,    78,    79,    45,    46,
      47,     0,     0,    12,    50,    51,    25,    56,    62,     0,
      72,    16,    17,     0,     0,    19,    20,    10,    13,    61,
       0,     3,    21,    22,     2,     0,     0,     0
};

static const short yydefgoto[] =
{
     145,    62,   140,    28,    39,    34,   129,    94,    68
};

static const short yypact[] =
{
      69,    21,   -27,    13,    37,    19,    36,    40,    89,    -3,
      20,   -21,   -15,    27,    -1,    86,    49,     8,-32768,    49,
      49,-32768,-32768,-32768,-32768,-32768,-32768,-32768,    -8,    10,
      18,-32768,-32768,-32768,     7,-32768,-32768,-32768,-32768,    39,
  -32768,    87,    22,    30,    31,    59,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,-32768,-32768,
  -32768,-32768,   102,    29,    29,   103,   104,-32768,   105,-32768,
  -32768,-32768,    68,   123,-32768,   106,   107,   108,-32768,   109,
     110,-32768,   144,-32768,-32768,-32768,-32768,    61,-32768,-32768,
     112,-32768,-32768,-32768,   113,   150,   151,   116,   117,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,   118,   119,-32768,-32768,-32768,-32768,-32768,   156,   121,
  -32768,-32768,-32768,   122,   124,-32768,-32768,-32768,-32768,-32768,
     125,-32768,-32768,-32768,-32768,   164,   167,-32768
};

static const short yypgoto[] =
{
  -32768,    54,-32768,    -5,    -6,-32768,-32768,-32768,    38
};


#define	YYLAST		167


static const short yytable[] =
{
      33,    41,    23,    24,    25,    59,    60,    26,    72,    75,
      23,    24,    25,    80,    22,    26,    23,    24,    25,    87,
      69,    26,    23,    24,    25,    63,    70,    26,    64,    89,
      23,    24,    25,    84,    63,    26,    73,    64,    61,    35,
      74,    36,    37,    35,    90,    36,    37,    91,    88,    81,
      92,    85,    19,    20,    27,    65,    66,    59,    60,    86,
      32,    67,    21,    99,    23,    24,    25,   122,    71,    26,
      79,   100,   101,    82,    83,    29,    30,    38,    31,   121,
      93,    40,   130,     1,     2,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
     102,   116,   117,    42,    43,    44,    45,    46,    47,    48,
      18,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      95,    96,    97,    98,    76,    77,    35,    78,    36,    37,
      58,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   118,   119,   120,   123,   124,   125,
     126,   127,   128,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   141,   142,   146,   143,   144,   147
};

static const short yycheck[] =
{
       5,     7,     3,     4,     5,     8,     9,     8,    13,    14,
       3,     4,     5,     5,    41,     8,     3,     4,     5,    12,
      41,     8,     3,     4,     5,     5,    41,     8,     8,    34,
       3,     4,     5,    41,     5,     8,    37,     8,    41,     3,
      41,     5,     6,     3,     5,     5,     6,     8,    41,    41,
      11,    41,    31,    32,    41,    35,    36,     8,     9,    41,
      41,    41,    41,    41,     3,     4,     5,    73,    41,     8,
      16,    41,    41,    19,    20,    38,    39,    41,    41,    11,
      41,    41,    87,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      41,    63,    64,    14,    15,    16,    17,    18,    19,    20,
      41,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      33,    34,    35,    36,    38,    39,     3,    41,     5,     6,
      41,    41,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41,     8,    41,    41,     5,     5,    41,    41,    41,
      41,     5,    41,    41,     0,    41,    41,     0
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
#line 98 "gram.y"
{
		param.number = OPEN; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 2:
#line 102 "gram.y"
{
		opendso(yyvsp[-3].y_str, yyvsp[-2].y_str, yyvsp[-1].y_num);
		stmt_type = OPEN; YYACCEPT;
	    }
    break;
case 3:
#line 106 "gram.y"
{
		openpmda(yyvsp[-2].y_str);
		stmt_type = OPEN; YYACCEPT;
	    }
    break;
case 4:
#line 110 "gram.y"
{ 
		stmt_type = CLOSE; YYACCEPT; 
	    }
    break;
case 5:
#line 113 "gram.y"
{
		param.number = DESC; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 6:
#line 117 "gram.y"
{
		param.pmid = yyvsp[-1].y_num;
		stmt_type = DESC; YYACCEPT;
	    }
    break;
case 7:
#line 121 "gram.y"
{
		param.number = FETCH; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 8:
#line 125 "gram.y"
{ 
		stmt_type = FETCH; YYACCEPT; 
	    }
    break;
case 9:
#line 128 "gram.y"
{
		param.number = STORE; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 10:
#line 132 "gram.y"
{
		param.name = yyvsp[-1].y_str;
		param.pmid = (pmID)yyvsp[-2].y_num;
		stmt_type = STORE; YYACCEPT;
	    }
    break;
case 11:
#line 137 "gram.y"
{
		param.number = TEXT; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 12:
#line 141 "gram.y"
{
		param.number = PM_TEXT_PMID;
		param.pmid = (pmID)yyvsp[-1].y_num;
		stmt_type = TEXT; YYACCEPT;
	    }
    break;
case 13:
#line 146 "gram.y"
{
		param.number = PM_TEXT_INDOM;
		param.indom = indom.whole;
		stmt_type = TEXT; YYACCEPT;
	    }
    break;
case 14:
#line 151 "gram.y"
{
		param.number = INSTANCE; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 15:
#line 155 "gram.y"
{
		param.indom = indom.whole;
		param.number = PM_IN_NULL;
		param.name = NULL;
		stmt_type = INSTANCE; YYACCEPT;
	    }
    break;
case 16:
#line 161 "gram.y"
{
		param.indom = indom.whole;
		param.number = yyvsp[-1].y_num;
		param.name = NULL;
		stmt_type = INSTANCE; YYACCEPT;
	    }
    break;
case 17:
#line 167 "gram.y"
{
		param.indom = indom.whole;
		param.number = PM_IN_NULL;
		param.name = yyvsp[-1].y_str;
		stmt_type = INSTANCE; YYACCEPT;
	    }
    break;
case 18:
#line 173 "gram.y"
{
		param.number = PROFILE; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 19:
#line 177 "gram.y"
{
		sts = pmAddProfile(yyvsp[-2].y_num, 0, NULL);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		profile_changed = 1;
		stmt_type = EOL;
		YYACCEPT;
	    }
    break;
case 20:
#line 187 "gram.y"
{
		sts = pmDelProfile(yyvsp[-2].y_num, 0, NULL);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		profile_changed = 1;
		stmt_type = EOL;
		YYACCEPT;
	    }
    break;
case 21:
#line 197 "gram.y"
{
		inst = yyvsp[-1].y_num;
		sts = pmAddProfile(yyvsp[-3].y_num, 1, &inst);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		profile_changed = 1;
		stmt_type = EOL;
		YYACCEPT;
	    }
    break;
case 22:
#line 208 "gram.y"
{
		inst = yyvsp[-1].y_num;
		sts = pmDelProfile(yyvsp[-3].y_num, 1, &inst);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		profile_changed = 1;
		stmt_type = EOL;
		YYACCEPT;
	    }
    break;
case 23:
#line 219 "gram.y"
{
		param.number = WATCH; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 24:
#line 223 "gram.y"
{
		watch(yyvsp[-1].y_str);
		stmt_type = WATCH; YYACCEPT;
	    }
    break;
case 25:
#line 227 "gram.y"
{
		param.name = yyvsp[-1].y_str;
		stmt_type = NAMESPACE; YYACCEPT;
	    }
    break;
case 26:
#line 231 "gram.y"
{ 
		param.number = -1; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT; 
	    }
    break;
case 27:
#line 235 "gram.y"
{
		param.number = OPEN; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 28:
#line 239 "gram.y"
{
		param.number = CLOSE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 29:
#line 243 "gram.y"
{
		param.number = DESC; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 30:
#line 247 "gram.y"
{
		param.number = FETCH; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 31:
#line 251 "gram.y"
{
		param.number = GETDESC; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 32:
#line 255 "gram.y"
{
		param.number = INSTANCE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 33:
#line 259 "gram.y"
{
		param.number = NAMESPACE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 34:
#line 263 "gram.y"
{
		param.number = PROFILE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 35:
#line 267 "gram.y"
{
		param.number = WATCH; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 36:
#line 271 "gram.y"
{
		param.number = DBG; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 37:
#line 275 "gram.y"
{
		param.number = QUIT; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 38:
#line 279 "gram.y"
{
		param.number = STATUS; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 39:
#line 283 "gram.y"
{
		param.number = STORE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 40:
#line 287 "gram.y"
{
		param.number = TEXT; param.pmid = HELP_FULL;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 41:
#line 291 "gram.y"
{
		param.number = TIMER; param.pmid = HELP_FULL;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 42:
#line 295 "gram.y"
{
		param.number = WAIT; param.pmid = HELP_FULL;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 43:
#line 299 "gram.y"
{ stmt_type = QUIT; YYACCEPT; }
    break;
case 44:
#line 300 "gram.y"
{
		param.number = DBG; param.pmid = HELP_USAGE; 
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 45:
#line 304 "gram.y"
{
		param.number = -1;
		stmt_type = DBG; YYACCEPT;
	    }
    break;
case 46:
#line 308 "gram.y"
{
		param.number = 0;
		stmt_type = DBG; YYACCEPT;
	    }
    break;
case 47:
#line 312 "gram.y"
{
		param.number = yyvsp[-1].y_num;
	        stmt_type = DBG; YYACCEPT;
	    }
    break;
case 48:
#line 316 "gram.y"
{ 
		stmt_type = STATUS; YYACCEPT; 
	    }
    break;
case 49:
#line 319 "gram.y"
{
		param.number = TIMER; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 50:
#line 323 "gram.y"
{ 
		timer = 1; stmt_type = EOL; YYACCEPT; 
	    }
    break;
case 51:
#line 326 "gram.y"
{ 
		timer = 0; stmt_type = EOL; YYACCEPT; 
	    }
    break;
case 52:
#line 329 "gram.y"
{
		param.number = GETDESC; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 53:
#line 333 "gram.y"
{ 
		get_desc = 1; stmt_type = EOL; YYACCEPT; 
	    }
    break;
case 54:
#line 336 "gram.y"
{ 
		get_desc = 0; stmt_type = EOL; YYACCEPT; 
	    }
    break;
case 55:
#line 339 "gram.y"
{
		param.number = WAIT; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
    break;
case 56:
#line 343 "gram.y"
{ 
		stmt_type = EOL; sleep(yyvsp[-1].y_num); YYACCEPT;
	    }
    break;
case 57:
#line 346 "gram.y"
{ stmt_type = EOL; YYACCEPT; }
    break;
case 58:
#line 347 "gram.y"
{
		if (yywrap())
		    YYACCEPT;
		else {
		    yyerror("Unrecognized command");
		    YYERROR;
		}
	    }
    break;
case 59:
#line 357 "gram.y"
{ yyval.y_str = strcons("./", yyvsp[0].y_str); }
    break;
case 60:
#line 358 "gram.y"
{ yyval.y_str = yyvsp[0].y_str; }
    break;
case 61:
#line 361 "gram.y"
{ yyval.y_num = yyvsp[0].y_num; }
    break;
case 62:
#line 362 "gram.y"
{ yyval.y_num = 0; }
    break;
case 63:
#line 365 "gram.y"
{ 
		pmid.whole = yyvsp[0].y_num;
		sts = pmNameID(pmid.whole, &str);
		if (sts < 0) {
		    sprintf(warnStr, "PMID (%s) is not defined in the PMNS",
			    pmIDStr(pmid.whole));
		    yywarn(warnStr);
		}
		else
		    free(str);
		yyval.y_num = (int)pmid.whole; 
	    }
    break;
case 64:
#line 377 "gram.y"
{
		pmid.whole = 0;
		pmid.part.cluster = yyvsp[0].y_2num.num1;
		pmid.part.item = yyvsp[0].y_2num.num2;
		sts = pmNameID(pmid.whole, &str);
		if (sts < 0) {
		    sprintf(warnStr, "PMID (%s) is not defined in the PMNS",
			    pmIDStr(pmid.whole));
		    yywarn(warnStr);
		}
		else
		    free(str);
		yyval.y_num = (int)pmid.whole;
	    }
    break;
case 65:
#line 391 "gram.y"
{
		pmid.whole = 0;
		pmid.part.domain = yyvsp[0].y_3num.num1;
		pmid.part.cluster = yyvsp[0].y_3num.num2;
		pmid.part.item = yyvsp[0].y_3num.num3;
		sts = pmNameID(pmid.whole, &str);
		if (sts < 0) {
		    sprintf(warnStr, "PMID (%s) is not defined in the PMNS",
			    pmIDStr(pmid.whole));
		    yywarn(warnStr);
		}
		else
		    free(str);
		yyval.y_num = (int)pmid.whole;
	    }
    break;
case 66:
#line 406 "gram.y"
{
		sts = pmLookupName(1, &yyvsp[0].y_str, &pmid.whole);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		yyval.y_num = (int)pmid.whole;
	    }
    break;
case 67:
#line 416 "gram.y"
{ 
		indom.whole = yyvsp[0].y_num;
	        yyval.y_num = (int)indom.whole;
	    }
    break;
case 68:
#line 420 "gram.y"
{ 
		indom.whole = yyvsp[0].y_num;
		yyval.y_num = (int)indom.whole;
	    }
    break;
case 69:
#line 424 "gram.y"
{
		indom.whole = 0;
		indom.part.domain = yyvsp[0].y_2num.num1;
		indom.part.serial = yyvsp[0].y_2num.num2;
		yyval.y_num = (int)indom.whole;
	    }
    break;
case 70:
#line 432 "gram.y"
{ addmetriclist((pmID)yyvsp[0].y_num); }
    break;
case 71:
#line 433 "gram.y"
{ addmetriclist((pmID)yyvsp[0].y_num); }
    break;
case 72:
#line 434 "gram.y"
{ addmetriclist((pmID)yyvsp[0].y_num); }
    break;
case 73:
#line 438 "gram.y"
{ doargs(); }
    break;
case 74:
#line 441 "gram.y"
{ yyval.y_str = yyvsp[0].y_str; }
    break;
case 75:
#line 442 "gram.y"
{ yyval.y_str = yyvsp[0].y_str; }
    break;
case 76:
#line 445 "gram.y"
{ yyval.y_num = yyvsp[0].y_num; }
    break;
case 77:
#line 446 "gram.y"
{
			sts = __pmParseDebug(yyvsp[0].y_str);
			if (sts < 0) {
			    sprintf(warnStr, "Bad debug flag (%s)", yyvsp[0].y_str);
			    yywarn(warnStr);
			    YYERROR;
			}
			yyval.y_num = sts;
		    }
    break;
case 78:
#line 455 "gram.y"
{ yyval.y_num = yyvsp[-1].y_num | yyvsp[0].y_num; }
    break;
case 79:
#line 456 "gram.y"
{
			sts = __pmParseDebug(yyvsp[-1].y_str);
			if (sts < 0) {
			    sprintf(warnStr, "Bad debug flag (%s)", yyvsp[-1].y_str);
			    yywarn(warnStr);
			    YYERROR;
			}
			yyval.y_num = sts | yyvsp[0].y_num;
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
#line 467 "gram.y"

