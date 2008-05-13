/* A Bison parser, made from grammar.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	ARROW	257
# define	SHELL	258
# define	ALARM	259
# define	SYSLOG	260
# define	PRINT	261
# define	SOME_QUANT	262
# define	ALL_QUANT	263
# define	PCNT_QUANT	264
# define	LEQ_REL	265
# define	GEQ_REL	266
# define	NEQ_REL	267
# define	EQ_REL	268
# define	AND	269
# define	SEQ	270
# define	OR	271
# define	ALT	272
# define	NOT	273
# define	RISE	274
# define	FALL	275
# define	MATCH	276
# define	NOMATCH	277
# define	MIN_AGGR	278
# define	MAX_AGGR	279
# define	AVG_AGGR	280
# define	SUM_AGGR	281
# define	COUNT_AGGR	282
# define	TIME_DOM	283
# define	INST_DOM	284
# define	HOST_DOM	285
# define	UNIT_SLASH	286
# define	INTERVAL	287
# define	EVENT_UNIT	288
# define	TIME_UNIT	289
# define	SPACE_UNIT	290
# define	NUMBER	291
# define	IDENT	292
# define	STRING	293
# define	TRU	294
# define	FALS	295
# define	VAR	296
# define	NAME_DELIM	297
# define	UMINUS	298
# define	RATE	299

#line 27 "grammar.y"

#include "dstruct.h"
#include "syntax.h"
#include "lexicon.h"
#include "pragmatics.h"
#include "syslog.h"
#include "show.h"

/* strings for error reporting */
char precede[]	 = "precede";
char follow[]	 = "follow";
char act_str[]	 = "action";
char bexp_str[]	 = "logical expression";
char aexp_str[]	 = "arithmetic expression";
char quant_str[] = "quantifier";
char aggr_str[]  = "aggregation operator";
char pcnt_str[]  = "percentage quantifier";
char host_str[]  = "host name";
char inst_str[]  = "instance name";
char sample_str[]  = "sample number(s)";
char tstr_str[]	 = "(time interval optional) and string";
char num_str[]	 = "number";

/* report grammatical error */
static void
gramerr(char *phrase, char *pos, char *op)
{
    fprintf(stderr, "%s expected to %s %s\n", phrase, pos, op);
    lexSync();
}


#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		212
#define	YYFLAG		-32768
#define	YYNTBASE	59

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 299 ? yytranslate[x] : 80)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,    53,     2,     2,     2,     2,
      56,    57,    48,    46,     2,    47,     2,    49,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    52,     2,
      45,    55,    44,     2,    54,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    58,     2,     2,     2,     2,     2,
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
      36,    37,    38,    39,    40,    41,    42,    43,    50,    51
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     1,     5,     7,     9,    11,    13,    15,    17,
      21,    24,    28,    32,    36,    40,    43,    47,    50,    54,
      57,    61,    64,    68,    71,    75,    78,    82,    85,    88,
      91,    94,    96,    98,   101,   105,   107,   109,   111,   113,
     116,   119,   122,   126,   130,   134,   138,   141,   144,   147,
     150,   154,   157,   161,   164,   168,   171,   175,   179,   183,
     188,   192,   196,   201,   204,   208,   212,   216,   220,   224,
     228,   231,   235,   238,   242,   245,   249,   252,   256,   259,
     263,   266,   270,   274,   276,   278,   280,   282,   285,   288,
     292,   296,   300,   304,   307,   310,   313,   317,   320,   324,
     327,   331,   334,   338,   342,   346,   350,   354,   358,   362,
     366,   370,   374,   376,   378,   380,   385,   387,   388,   392,
     396,   397,   401,   405,   406,   409,   414,   417,   420,   421,
     424,   428,   430,   434,   436,   440,   442,   446
};
static const short yyrhs[] =
{
      -1,    38,    55,    60,     0,    60,     0,    61,     0,    65,
       0,    68,     0,    62,     0,    79,     0,    65,     3,    62,
       0,     1,     3,     0,    65,     3,     1,     0,    56,    62,
      57,     0,    62,    16,    62,     0,    62,    18,    62,     0,
       4,    63,     0,     4,    76,    63,     0,     5,    63,     0,
       5,    76,    63,     0,     6,    63,     0,     6,    76,    63,
       0,     7,    63,     0,     7,    76,    63,     0,     1,    16,
       0,    62,    16,     1,     0,     1,    18,     0,    62,    18,
       1,     0,     4,     1,     0,     5,     1,     0,     6,     1,
       0,     7,     1,     0,    64,     0,    39,     0,    39,    64,
       0,    56,    65,    57,     0,    67,     0,    66,     0,    40,
       0,    41,     0,    19,    65,     0,    20,    65,     0,    21,
      65,     0,    65,    15,    65,     0,    65,    17,    65,     0,
      22,    79,    65,     0,    23,    79,    65,     0,    19,     1,
       0,    20,     1,     0,    21,     1,     0,    22,     1,     0,
      22,    79,     1,     0,    23,     1,     0,    23,    79,     1,
       0,     1,    15,     0,    65,    15,     1,     0,     1,    17,
       0,    65,    17,     1,     0,     9,    70,    65,     0,     8,
      70,    65,     0,    37,    10,    70,    65,     0,     9,    70,
       1,     0,     8,    70,     1,     0,    37,    10,    70,     1,
       0,     1,    10,     0,    68,    14,    68,     0,    68,    13,
      68,     0,    68,    45,    68,     0,    68,    44,    68,     0,
      68,    11,    68,     0,    68,    12,    68,     0,     1,    14,
       0,    68,    14,     1,     0,     1,    13,     0,    68,    13,
       1,     0,     1,    45,     0,    68,    45,     1,     0,     1,
      44,     0,    68,    44,     1,     0,     1,    11,     0,    68,
      11,     1,     0,     1,    12,     0,    68,    12,     1,     0,
      56,    68,    57,     0,    71,     0,    76,     0,    42,     0,
      69,     0,    51,    68,     0,    47,    68,     0,    68,    46,
      68,     0,    68,    47,    68,     0,    68,    48,    68,     0,
      68,    49,    68,     0,    51,     1,     0,    47,     1,     0,
       1,    46,     0,    68,    46,     1,     0,     1,    47,     0,
      68,    47,     1,     0,     1,    48,     0,    68,    48,     1,
       0,     1,    49,     0,    68,    49,     1,     0,    27,    70,
      68,     0,    26,    70,    68,     0,    25,    70,    68,     0,
      24,    70,    68,     0,    28,    70,    65,     0,    27,    70,
       1,     0,    26,    70,     1,     0,    25,    70,     1,     0,
      24,    70,     1,     0,    31,     0,    30,     0,    29,     0,
      72,    73,    74,    75,     0,    38,     0,     0,    73,    52,
      38,     0,    73,    52,     1,     0,     0,    74,    53,    38,
       0,    74,    53,     1,     0,     0,    54,    37,     0,    54,
      37,    33,    37,     0,    54,     1,     0,    37,    77,     0,
       0,    77,    78,     0,    77,    32,    78,     0,    36,     0,
      36,    58,    37,     0,    35,     0,    35,    58,    37,     0,
      34,     0,    34,    58,    37,     0,    39,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   152,   154,   165,   176,   178,   180,   182,   184,   188,
     192,   195,   200,   202,   204,   206,   208,   210,   212,   214,
     218,   223,   225,   229,   233,   236,   239,   243,   246,   249,
     252,   257,   261,   263,   267,   269,   271,   273,   275,   277,
     279,   281,   283,   285,   287,   293,   297,   300,   303,   306,
     309,   312,   315,   319,   322,   325,   328,   334,   336,   338,
     342,   345,   348,   351,   356,   358,   360,   362,   364,   366,
     370,   373,   376,   379,   382,   385,   388,   391,   394,   397,
     400,   403,   408,   410,   412,   414,   416,   418,   420,   422,
     424,   426,   428,   432,   435,   439,   442,   445,   448,   451,
     454,   457,   460,   466,   468,   470,   472,   474,   478,   481,
     484,   487,   492,   494,   496,   500,   504,   508,   511,   517,
     523,   526,   532,   538,   541,   544,   555,   561,   565,   567,
     581,   597,   599,   602,   604,   607,   609,   614
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "ARROW", "SHELL", "ALARM", "SYSLOG", "PRINT", 
  "SOME_QUANT", "ALL_QUANT", "PCNT_QUANT", "LEQ_REL", "GEQ_REL", 
  "NEQ_REL", "EQ_REL", "AND", "SEQ", "OR", "ALT", "NOT", "RISE", "FALL", 
  "MATCH", "NOMATCH", "MIN_AGGR", "MAX_AGGR", "AVG_AGGR", "SUM_AGGR", 
  "COUNT_AGGR", "TIME_DOM", "INST_DOM", "HOST_DOM", "UNIT_SLASH", 
  "INTERVAL", "EVENT_UNIT", "TIME_UNIT", "SPACE_UNIT", "NUMBER", "IDENT", 
  "STRING", "TRU", "FALS", "VAR", "NAME_DELIM", "'>'", "'<'", "'+'", 
  "'-'", "'*'", "'/'", "UMINUS", "RATE", "':'", "'#'", "'@'", "'='", 
  "'('", "')'", "'^'", "stmnt", "exp", "rule", "act", "actarg", "arglist", 
  "bexp", "quant", "rexp", "aexp", "aggr", "dom", "fetch", "metric", 
  "hosts", "insts", "times", "num", "units", "unit", "str", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    59,    59,    59,    60,    60,    60,    60,    60,    61,
      61,    61,    62,    62,    62,    62,    62,    62,    62,    62,
      62,    62,    62,    62,    62,    62,    62,    62,    62,    62,
      62,    63,    64,    64,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    66,    66,    66,
      66,    66,    66,    66,    67,    67,    67,    67,    67,    67,
      67,    67,    67,    67,    67,    67,    67,    67,    67,    67,
      67,    67,    68,    68,    68,    68,    68,    68,    68,    68,
      68,    68,    68,    68,    68,    68,    68,    68,    68,    68,
      68,    68,    68,    69,    69,    69,    69,    69,    69,    69,
      69,    69,    70,    70,    70,    71,    72,    73,    73,    73,
      74,    74,    74,    75,    75,    75,    75,    76,    77,    77,
      77,    78,    78,    78,    78,    78,    78,    79
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     0,     3,     1,     1,     1,     1,     1,     1,     3,
       2,     3,     3,     3,     3,     2,     3,     2,     3,     2,
       3,     2,     3,     2,     3,     2,     3,     2,     2,     2,
       2,     1,     1,     2,     3,     1,     1,     1,     1,     2,
       2,     2,     3,     3,     3,     3,     2,     2,     2,     2,
       3,     2,     3,     2,     3,     2,     3,     3,     3,     4,
       3,     3,     4,     2,     3,     3,     3,     3,     3,     3,
       2,     3,     2,     3,     2,     3,     2,     3,     2,     3,
       2,     3,     3,     1,     1,     1,     1,     2,     2,     3,
       3,     3,     3,     2,     2,     2,     3,     2,     3,     2,
       3,     2,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     1,     1,     1,     4,     1,     0,     3,     3,
       0,     3,     3,     0,     2,     4,     2,     2,     0,     2,
       3,     1,     3,     1,     3,     1,     3,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   128,   116,
     137,    37,    38,    85,     0,     0,     0,     3,     4,     7,
       5,    36,    35,     6,    86,    83,   117,    84,     8,    10,
      63,    78,    80,    72,    70,    53,    23,    55,    25,    76,
      74,    95,    97,    99,   101,    27,   128,    32,    15,    31,
       0,    28,    17,     0,    29,    19,     0,    30,    21,     0,
     114,   113,   112,     0,     0,    46,   116,     0,    39,     0,
      47,    40,    48,    41,    49,     0,    51,     0,     0,     0,
       0,     0,     0,     0,   127,     0,    94,     0,    88,    93,
      87,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     120,    33,    16,    18,    20,    22,    61,    58,    60,    57,
       0,    50,    44,    52,    45,   111,   106,   110,   105,   109,
     104,   108,   103,   107,     0,     0,   135,   133,   131,   129,
       2,     0,     0,    12,    34,    82,    24,     0,    13,    26,
      14,    11,     9,    54,    42,    56,    43,    79,    68,    81,
      69,    73,    65,    71,    64,    77,    67,    75,    66,    96,
      89,    98,    90,   100,    91,   102,    92,     0,   123,    62,
      59,   130,     0,     0,     0,     0,   119,   118,     0,     0,
     115,   136,   134,   132,   122,   121,   126,   124,     0,   125,
       0,     0,     0
};

static const short yydefgoto[] =
{
     210,    27,    28,    29,    58,    59,    30,    31,    32,    79,
      34,    73,    35,    36,   120,   188,   200,    37,    94,   149,
      38
};

static const short yypact[] =
{
     134,   103,     6,    25,   143,   203,   148,   148,   284,   328,
     372,    53,   145,   148,   148,   148,   148,   148,    -6,   -47,
  -32768,-32768,-32768,-32768,     4,   757,   240,-32768,-32768,   171,
      65,-32768,-32768,   206,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,    -2,-32768,-32768,
      -2,-32768,-32768,    -2,-32768,-32768,    -2,-32768,-32768,    -2,
  -32768,-32768,-32768,   416,   460,   153,-32768,   504,-32768,   206,
     153,-32768,   153,-32768,-32768,   548,-32768,   592,   790,   796,
     829,   835,   504,   148,   201,   187,-32768,   868,-32768,   251,
  -32768,  1129,    55,    19,  1081,    18,    43,    52,   636,   680,
     874,   907,   913,   946,   952,   985,   991,  1024,  1030,  1063,
     -17,-32768,-32768,-32768,-32768,-32768,   153,-32768,   153,-32768,
     153,   153,-32768,   153,-32768,   251,-32768,   251,-32768,   251,
  -32768,   251,-32768,-32768,   724,   196,     9,    30,    39,-32768,
  -32768,   251,   226,-32768,-32768,-32768,   205,    89,-32768,   205,
  -32768,   205,   171,   153,-32768,   153,-32768,   251,   267,   251,
     267,   251,   267,   251,   267,   251,   267,   251,   267,   251,
     -28,   251,   -28,   251,-32768,   251,-32768,    32,    69,   153,
  -32768,-32768,    15,    46,    70,   205,-32768,-32768,    60,    26,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,    77,   132,-32768,
     183,   186,-32768
};

static const short yypgoto[] =
{
  -32768,   108,-32768,   -20,    12,   159,    -8,-32768,-32768,    14,
  -32768,    -4,-32768,-32768,-32768,-32768,-32768,   315,-32768,    94,
     245
};


#define	YYLAST		1178


static const short yytable[] =
{
      78,    81,    83,    74,    93,    96,   102,    55,    95,    88,
      89,    90,    91,    92,    33,    62,    65,    68,   103,   156,
     118,   119,     2,     3,     4,     5,    61,   206,    13,    14,
      15,    16,    17,   196,   108,   187,   109,    57,    98,   100,
     104,    56,    76,    56,   159,    57,    23,     2,     3,     4,
       5,    24,   201,   161,    84,    25,     2,     3,     4,     5,
      97,   204,    56,   207,    57,   127,   129,   192,   107,   103,
     197,   105,   122,   106,   157,   123,   154,   132,   124,   134,
     108,   125,   109,   202,   143,   158,   160,   162,   193,   144,
     195,   104,    20,     2,     3,     4,     5,   194,   205,   157,
     164,   166,   136,   138,   140,   142,    39,   203,   157,    33,
     208,   152,   153,    40,    41,    42,    43,    44,    45,    46,
      47,    48,   198,   199,   168,   170,   172,   174,   176,   178,
     180,   182,   184,   186,    -1,     1,   190,   102,     2,     3,
       4,     5,     6,     7,    64,   157,    86,    49,    50,    51,
      52,    53,    54,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    40,    41,    42,    43,    44,    45,   209,
      47,    18,    19,    20,    21,    22,    23,    70,    71,    72,
      56,    24,    57,   211,    20,    25,   212,   105,     1,   106,
      26,     2,     3,     4,     5,     6,     7,    49,    50,    51,
      52,    53,    54,   150,    67,     0,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,   121,   110,   111,   112,
     113,    46,     0,    48,    18,    76,    20,    21,    22,    23,
     146,   147,   148,   145,    24,   146,   147,   148,    25,   191,
      56,   101,    57,    26,     2,     3,     4,     5,     6,     7,
     114,   115,   116,   117,   118,   119,    85,    87,     0,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,     0,
       0,     0,   116,   117,   118,   119,     0,    18,    76,     0,
      21,    22,    23,   155,     0,    75,     0,    24,     0,     0,
       0,    25,     6,     7,     0,     0,    26,    51,    52,    53,
      54,     0,     0,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,   116,   117,   118,   119,    60,    63,    66,
      69,    18,    76,     0,    21,    22,    23,     0,     0,    80,
       0,    24,     0,     0,     0,    25,     6,     7,     0,     0,
      77,     0,     0,     0,     0,     0,     0,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,     0,     0,     0,
       0,     0,     0,     0,     0,    18,    76,     0,    21,    22,
      23,     0,     0,    82,     0,    24,     0,     0,     0,    25,
       6,     7,     0,     0,    77,     0,     0,     0,     0,     0,
       0,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,     0,     0,     0,     0,     0,     0,     0,     0,    18,
      76,     0,    21,    22,    23,     0,     0,   126,     0,    24,
       0,     0,     0,    25,     6,     7,     0,     0,    77,     0,
       0,     0,     0,     0,     0,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,     0,     0,     0,     0,     0,
       0,     0,     0,    18,    76,     0,    21,    22,    23,     0,
       0,   128,     0,    24,     0,     0,     0,    25,     6,     7,
       0,     0,    77,     0,     0,     0,     0,     0,     0,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,     0,
       0,     0,     0,     0,     0,     0,     0,    18,    76,     0,
      21,    22,    23,     0,     0,   130,     0,    24,     0,     0,
       0,    25,     6,     7,     0,     0,    77,     0,     0,     0,
       0,     0,     0,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,     0,     0,     0,     0,     0,     0,     0,
       0,    18,    76,     0,    21,    22,    23,     0,     0,   131,
       0,    24,     0,     0,     0,    25,     6,     7,     0,     0,
      77,     0,     0,     0,     0,     0,     0,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,     0,     0,     0,
       0,     0,     0,     0,     0,    18,    76,     0,    21,    22,
      23,     0,     0,   133,     0,    24,     0,     0,     0,    25,
       6,     7,     0,     0,    77,     0,     0,     0,     0,     0,
       0,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,     0,     0,     0,     0,     0,     0,     0,     0,    18,
      76,     0,    21,    22,    23,     0,     0,   163,     0,    24,
       0,     0,     0,    25,     6,     7,     0,     0,    77,     0,
       0,     0,     0,     0,     0,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,     0,     0,     0,     0,     0,
       0,     0,     0,    18,    76,     0,    21,    22,    23,     0,
       0,   165,     0,    24,     0,     0,     0,    25,     6,     7,
       0,     0,    77,     0,     0,     0,     0,     0,     0,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,     0,
       0,     0,     0,     0,     0,     0,     0,    18,    76,     0,
      21,    22,    23,     0,     0,   189,     0,    24,     0,     0,
       0,    25,     6,     7,     0,     0,    77,     0,     0,     0,
       0,     0,     0,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,     0,     0,     0,     0,     0,    99,     0,
       0,    18,    76,     0,    21,    22,    23,     0,     0,     0,
       0,    24,     0,     0,     0,    25,     0,     0,     0,     0,
      77,    13,    14,    15,    16,    17,     0,     0,     0,     0,
       0,   135,     0,     0,    56,    76,     0,   137,     0,    23,
       0,     0,     0,     0,    24,     0,     0,     0,    25,     0,
       0,     0,     0,    97,    13,    14,    15,    16,    17,     0,
      13,    14,    15,    16,    17,     0,     0,    56,    76,     0,
     139,     0,    23,    56,    76,     0,   141,    24,    23,     0,
       0,    25,     0,    24,     0,     0,    97,    25,     0,     0,
       0,     0,    97,    13,    14,    15,    16,    17,     0,    13,
      14,    15,    16,    17,     0,     0,    56,    76,     0,   151,
       0,    23,    56,    76,     0,   167,    24,    23,     0,     0,
      25,     0,    24,     0,     0,    97,    25,     0,     0,     0,
       0,    97,    13,    14,    15,    16,    17,     0,    13,    14,
      15,    16,    17,     0,     0,    56,    76,     0,   169,     0,
      23,    56,    76,     0,   171,    24,    23,     0,     0,    25,
       0,    24,     0,     0,    97,    25,     0,     0,     0,     0,
      97,    13,    14,    15,    16,    17,     0,    13,    14,    15,
      16,    17,     0,     0,    56,    76,     0,   173,     0,    23,
      56,    76,     0,   175,    24,    23,     0,     0,    25,     0,
      24,     0,     0,    97,    25,     0,     0,     0,     0,    97,
      13,    14,    15,    16,    17,     0,    13,    14,    15,    16,
      17,     0,     0,    56,    76,     0,   177,     0,    23,    56,
      76,     0,   179,    24,    23,     0,     0,    25,     0,    24,
       0,     0,    97,    25,     0,     0,     0,     0,    97,    13,
      14,    15,    16,    17,     0,    13,    14,    15,    16,    17,
       0,     0,    56,    76,     0,   181,     0,    23,    56,    76,
       0,   183,    24,    23,     0,     0,    25,     0,    24,     0,
       0,    97,    25,     0,     0,     0,     0,    97,    13,    14,
      15,    16,    17,     0,    13,    14,    15,    16,    17,     0,
       0,    56,    76,     0,   185,     0,    23,    56,    76,     0,
       0,    24,    23,     0,     0,    25,     0,    24,     0,     0,
      97,    25,     0,     0,     0,     0,    97,    13,    14,    15,
      16,    17,   110,   111,   112,   113,     0,     0,     0,     0,
      56,    76,     0,     0,     0,    23,     0,     0,     0,     0,
      24,     0,     0,     0,    25,     0,     0,     0,     0,    97,
       0,     0,     0,     0,     0,   114,   115,   116,   117,   118,
     119,     0,     0,     0,     0,     0,     0,     0,   155,    40,
      41,    42,    43,    44,    45,    46,    47,    48,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    49,    50,    51,    52,    53,    54
};

static const short yycheck[] =
{
       8,     9,    10,     7,    10,     1,    26,     1,    55,    13,
      14,    15,    16,    17,     0,     3,     4,     5,    26,     1,
      48,    49,     4,     5,     6,     7,     1,     1,    24,    25,
      26,    27,    28,     1,    15,    52,    17,    39,    24,    25,
      26,    37,    38,    37,     1,    39,    42,     4,     5,     6,
       7,    47,    37,     1,     1,    51,     4,     5,     6,     7,
      56,     1,    37,    37,    39,    73,    74,    58,     3,    77,
      38,    16,    60,    18,    56,    63,    57,    85,    66,    87,
      15,    69,    17,    37,    92,   105,   106,   107,    58,    93,
       1,    77,    39,     4,     5,     6,     7,    58,    38,    56,
     108,   109,    88,    89,    90,    91,     3,    37,    56,    95,
      33,    97,    57,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    53,    54,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,     0,     1,   144,   157,     4,     5,
       6,     7,     8,     9,     1,    56,     1,    44,    45,    46,
      47,    48,    49,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    10,    11,    12,    13,    14,    15,    37,
      17,    37,    38,    39,    40,    41,    42,    29,    30,    31,
      37,    47,    39,     0,    39,    51,     0,    16,     1,    18,
      56,     4,     5,     6,     7,     8,     9,    44,    45,    46,
      47,    48,    49,    95,     1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    57,    11,    12,    13,
      14,    16,    -1,    18,    37,    38,    39,    40,    41,    42,
      34,    35,    36,    32,    47,    34,    35,    36,    51,   145,
      37,     1,    39,    56,     4,     5,     6,     7,     8,     9,
      44,    45,    46,    47,    48,    49,    11,    12,    -1,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    -1,
      -1,    -1,    46,    47,    48,    49,    -1,    37,    38,    -1,
      40,    41,    42,    57,    -1,     1,    -1,    47,    -1,    -1,
      -1,    51,     8,     9,    -1,    -1,    56,    46,    47,    48,
      49,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    46,    47,    48,    49,     2,     3,     4,
       5,    37,    38,    -1,    40,    41,    42,    -1,    -1,     1,
      -1,    47,    -1,    -1,    -1,    51,     8,     9,    -1,    -1,
      56,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    -1,    -1,     1,    -1,    47,    -1,    -1,    -1,    51,
       8,     9,    -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    -1,    40,    41,    42,    -1,    -1,     1,    -1,    47,
      -1,    -1,    -1,    51,     8,     9,    -1,    -1,    56,    -1,
      -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    -1,    40,    41,    42,    -1,
      -1,     1,    -1,    47,    -1,    -1,    -1,    51,     8,     9,
      -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,    -1,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,
      40,    41,    42,    -1,    -1,     1,    -1,    47,    -1,    -1,
      -1,    51,     8,     9,    -1,    -1,    56,    -1,    -1,    -1,
      -1,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    -1,    -1,     1,
      -1,    47,    -1,    -1,    -1,    51,     8,     9,    -1,    -1,
      56,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    -1,    -1,     1,    -1,    47,    -1,    -1,    -1,    51,
       8,     9,    -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    -1,    40,    41,    42,    -1,    -1,     1,    -1,    47,
      -1,    -1,    -1,    51,     8,     9,    -1,    -1,    56,    -1,
      -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    -1,    40,    41,    42,    -1,
      -1,     1,    -1,    47,    -1,    -1,    -1,    51,     8,     9,
      -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,    -1,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,
      40,    41,    42,    -1,    -1,     1,    -1,    47,    -1,    -1,
      -1,    51,     8,     9,    -1,    -1,    56,    -1,    -1,    -1,
      -1,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    -1,    -1,    -1,    -1,    -1,     1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    -1,    -1,    -1,
      -1,    47,    -1,    -1,    -1,    51,    -1,    -1,    -1,    -1,
      56,    24,    25,    26,    27,    28,    -1,    -1,    -1,    -1,
      -1,     1,    -1,    -1,    37,    38,    -1,     1,    -1,    42,
      -1,    -1,    -1,    -1,    47,    -1,    -1,    -1,    51,    -1,
      -1,    -1,    -1,    56,    24,    25,    26,    27,    28,    -1,
      24,    25,    26,    27,    28,    -1,    -1,    37,    38,    -1,
       1,    -1,    42,    37,    38,    -1,     1,    47,    42,    -1,
      -1,    51,    -1,    47,    -1,    -1,    56,    51,    -1,    -1,
      -1,    -1,    56,    24,    25,    26,    27,    28,    -1,    24,
      25,    26,    27,    28,    -1,    -1,    37,    38,    -1,     1,
      -1,    42,    37,    38,    -1,     1,    47,    42,    -1,    -1,
      51,    -1,    47,    -1,    -1,    56,    51,    -1,    -1,    -1,
      -1,    56,    24,    25,    26,    27,    28,    -1,    24,    25,
      26,    27,    28,    -1,    -1,    37,    38,    -1,     1,    -1,
      42,    37,    38,    -1,     1,    47,    42,    -1,    -1,    51,
      -1,    47,    -1,    -1,    56,    51,    -1,    -1,    -1,    -1,
      56,    24,    25,    26,    27,    28,    -1,    24,    25,    26,
      27,    28,    -1,    -1,    37,    38,    -1,     1,    -1,    42,
      37,    38,    -1,     1,    47,    42,    -1,    -1,    51,    -1,
      47,    -1,    -1,    56,    51,    -1,    -1,    -1,    -1,    56,
      24,    25,    26,    27,    28,    -1,    24,    25,    26,    27,
      28,    -1,    -1,    37,    38,    -1,     1,    -1,    42,    37,
      38,    -1,     1,    47,    42,    -1,    -1,    51,    -1,    47,
      -1,    -1,    56,    51,    -1,    -1,    -1,    -1,    56,    24,
      25,    26,    27,    28,    -1,    24,    25,    26,    27,    28,
      -1,    -1,    37,    38,    -1,     1,    -1,    42,    37,    38,
      -1,     1,    47,    42,    -1,    -1,    51,    -1,    47,    -1,
      -1,    56,    51,    -1,    -1,    -1,    -1,    56,    24,    25,
      26,    27,    28,    -1,    24,    25,    26,    27,    28,    -1,
      -1,    37,    38,    -1,     1,    -1,    42,    37,    38,    -1,
      -1,    47,    42,    -1,    -1,    51,    -1,    47,    -1,    -1,
      56,    51,    -1,    -1,    -1,    -1,    56,    24,    25,    26,
      27,    28,    11,    12,    13,    14,    -1,    -1,    -1,    -1,
      37,    38,    -1,    -1,    -1,    42,    -1,    -1,    -1,    -1,
      47,    -1,    -1,    -1,    51,    -1,    -1,    -1,    -1,    56,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,
      49,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    57,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    44,    45,    46,    47,    48,    49
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
#line 153 "grammar.y"
{    parse = NULL; }
    break;
case 2:
#line 155 "grammar.y"
{   parse = statement(yyvsp[-2].s, yyvsp[0].x);
		    if ((agent || applet) && yyvsp[0].x != NULL &&
			(yyvsp[0].x->op == RULE ||
			 (yyvsp[0].x->op >= ACT_SEQ && yyvsp[0].x->op < NOP))) {
			synerr();
			fprintf(stderr, "operator %s not allowed in agent "
					"mode\n", opStrings(yyvsp[0].x->op));
			parse = NULL;
		    }
		}
    break;
case 3:
#line 166 "grammar.y"
{   parse = statement(NULL, yyvsp[0].x);
		    if (agent) {
			synerr();
			fprintf(stderr, "expressions must be named in agent "
					"mode\n");
			parse = NULL;
		    }
		}
    break;
case 4:
#line 177 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 5:
#line 179 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 6:
#line 181 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 7:
#line 183 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 8:
#line 185 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 9:
#line 189 "grammar.y"
{    yyval.x = ruleExpr(yyvsp[-2].x, yyvsp[0].x); }
    break;
case 10:
#line 193 "grammar.y"
{   gramerr(bexp_str, precede, opStrings(RULE));
		    yyval.x = NULL; }
    break;
case 11:
#line 196 "grammar.y"
{   gramerr(act_str, follow, opStrings(RULE));
		    yyval.x = NULL; }
    break;
case 12:
#line 201 "grammar.y"
{   yyval.x = yyvsp[-1].x; }
    break;
case 13:
#line 203 "grammar.y"
{   yyval.x = actExpr(ACT_SEQ, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 14:
#line 205 "grammar.y"
{   yyval.x = actExpr(ACT_ALT, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 15:
#line 207 "grammar.y"
{   yyval.x = actExpr(ACT_SHELL, yyvsp[0].x, NULL); }
    break;
case 16:
#line 209 "grammar.y"
{   yyval.x = actExpr(ACT_SHELL, yyvsp[0].x, yyvsp[-1].x); }
    break;
case 17:
#line 211 "grammar.y"
{   yyval.x = actExpr(ACT_ALARM, yyvsp[0].x, NULL); }
    break;
case 18:
#line 213 "grammar.y"
{   yyval.x = actExpr(ACT_ALARM, yyvsp[0].x, yyvsp[-1].x); }
    break;
case 19:
#line 215 "grammar.y"
{   do_syslog_args(yyvsp[0].x);
		    yyval.x = actExpr(ACT_SYSLOG, yyvsp[0].x, NULL);
		}
    break;
case 20:
#line 219 "grammar.y"
{
		    do_syslog_args(yyvsp[0].x);
		    yyval.x = actExpr(ACT_SYSLOG, yyvsp[0].x, yyvsp[-1].x);
		}
    break;
case 21:
#line 224 "grammar.y"
{   yyval.x = actExpr(ACT_PRINT, yyvsp[0].x, NULL); }
    break;
case 22:
#line 226 "grammar.y"
{   yyval.x = actExpr(ACT_PRINT, yyvsp[0].x, yyvsp[-1].x); }
    break;
case 23:
#line 230 "grammar.y"
{   gramerr(act_str, precede, opStrings(ACT_SEQ));
		    yyval.x = NULL; }
    break;
case 24:
#line 234 "grammar.y"
{   gramerr(act_str, follow, opStrings(ACT_SEQ));
		    yyval.x = NULL; }
    break;
case 25:
#line 237 "grammar.y"
{   gramerr(act_str, precede, opStrings(ACT_ALT));
		    yyval.x = NULL; }
    break;
case 26:
#line 240 "grammar.y"
{   gramerr(act_str, follow, opStrings(ACT_ALT));
		    yyval.x = NULL; }
    break;
case 27:
#line 244 "grammar.y"
{   gramerr(tstr_str, follow, opStrings(ACT_SHELL));
		    yyval.x = NULL; }
    break;
case 28:
#line 247 "grammar.y"
{   gramerr(tstr_str, follow, opStrings(ACT_ALARM));
		    yyval.x = NULL; }
    break;
case 29:
#line 250 "grammar.y"
{   gramerr(tstr_str, follow, opStrings(ACT_SYSLOG));
		    yyval.x = NULL; }
    break;
case 30:
#line 253 "grammar.y"
{   gramerr(tstr_str, follow, opStrings(ACT_PRINT));
		    yyval.x = NULL; }
    break;
case 31:
#line 258 "grammar.y"
{   yyval.x = actArgExpr(yyvsp[0].x, NULL); }
    break;
case 32:
#line 262 "grammar.y"
{   yyval.x = actArgList(NULL, yyvsp[0].s); }
    break;
case 33:
#line 264 "grammar.y"
{   yyval.x = actArgList(yyvsp[0].x, yyvsp[-1].s); }
    break;
case 34:
#line 268 "grammar.y"
{   yyval.x = yyvsp[-1].x; }
    break;
case 35:
#line 270 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 36:
#line 272 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 37:
#line 274 "grammar.y"
{   yyval.x = boolConst(TRUE); }
    break;
case 38:
#line 276 "grammar.y"
{   yyval.x = boolConst(FALSE); }
    break;
case 39:
#line 278 "grammar.y"
{   yyval.x = unaryExpr(CND_NOT, yyvsp[0].x); }
    break;
case 40:
#line 280 "grammar.y"
{   yyval.x = boolMergeExpr(CND_RISE, yyvsp[0].x); }
    break;
case 41:
#line 282 "grammar.y"
{   yyval.x = boolMergeExpr(CND_FALL, yyvsp[0].x); }
    break;
case 42:
#line 284 "grammar.y"
{   yyval.x = binaryExpr(CND_AND, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 43:
#line 286 "grammar.y"
{   yyval.x = binaryExpr(CND_OR, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 44:
#line 288 "grammar.y"
{   /*
		     * note args are reversed so bexp is to the "left"
		     * of the operand node in the expr tree
		     */
		    yyval.x = binaryExpr(CND_MATCH, yyvsp[0].x, yyvsp[-1].x); }
    break;
case 45:
#line 294 "grammar.y"
{   yyval.x = binaryExpr(CND_NOMATCH, yyvsp[0].x, yyvsp[-1].x); }
    break;
case 46:
#line 298 "grammar.y"
{   gramerr(bexp_str, follow, opStrings(CND_NOT));
		    yyval.x = NULL; }
    break;
case 47:
#line 301 "grammar.y"
{   gramerr(bexp_str, follow, opStrings(CND_RISE));
		    yyval.x = NULL; }
    break;
case 48:
#line 304 "grammar.y"
{   gramerr(bexp_str, follow, opStrings(CND_FALL));
		    yyval.x = NULL; }
    break;
case 49:
#line 307 "grammar.y"
{   gramerr("regular expression", follow, opStrings(CND_MATCH));
		    yyval.x = NULL; }
    break;
case 50:
#line 310 "grammar.y"
{   gramerr(bexp_str, follow, "regular expression");
		    yyval.x = NULL; }
    break;
case 51:
#line 313 "grammar.y"
{   gramerr("regular expression", follow, opStrings(CND_NOMATCH));
		    yyval.x = NULL; }
    break;
case 52:
#line 316 "grammar.y"
{   gramerr(bexp_str, follow, "regular expression");
		    yyval.x = NULL; }
    break;
case 53:
#line 320 "grammar.y"
{   gramerr(bexp_str, precede, opStrings(CND_AND));
		    yyval.x = NULL; }
    break;
case 54:
#line 323 "grammar.y"
{   gramerr(bexp_str, follow, opStrings(CND_AND));
		    yyval.x = NULL; }
    break;
case 55:
#line 326 "grammar.y"
{   gramerr(bexp_str, precede, opStrings(CND_AND));
		    yyval.x = NULL; }
    break;
case 56:
#line 329 "grammar.y"
{   gramerr(bexp_str, follow, opStrings(CND_AND));
		    yyval.x = NULL; }
    break;
case 57:
#line 335 "grammar.y"
{   yyval.x = domainExpr(CND_ALL_HOST, yyvsp[-1].i, yyvsp[0].x); }
    break;
case 58:
#line 337 "grammar.y"
{   yyval.x = domainExpr(CND_SOME_HOST, yyvsp[-1].i, yyvsp[0].x); }
    break;
case 59:
#line 339 "grammar.y"
{   yyval.x = percentExpr(yyvsp[-3].d, yyvsp[-1].i, yyvsp[0].x); }
    break;
case 60:
#line 343 "grammar.y"
{   gramerr(bexp_str, follow, quant_str);
		    yyval.x = NULL; }
    break;
case 61:
#line 346 "grammar.y"
{   gramerr(bexp_str, follow, quant_str);
		    yyval.x = NULL; }
    break;
case 62:
#line 349 "grammar.y"
{   gramerr(bexp_str, follow, quant_str);
		    yyval.x = NULL; }
    break;
case 63:
#line 352 "grammar.y"
{   gramerr(num_str, precede, pcnt_str);
		    yyval.x = NULL; }
    break;
case 64:
#line 357 "grammar.y"
{   yyval.x = relExpr(CND_EQ, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 65:
#line 359 "grammar.y"
{   yyval.x = relExpr(CND_NEQ, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 66:
#line 361 "grammar.y"
{   yyval.x = relExpr(CND_LT, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 67:
#line 363 "grammar.y"
{   yyval.x = relExpr(CND_GT, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 68:
#line 365 "grammar.y"
{   yyval.x = relExpr(CND_LTE, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 69:
#line 367 "grammar.y"
{   yyval.x = relExpr(CND_GTE, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 70:
#line 371 "grammar.y"
{   gramerr(aexp_str, precede, opStrings(CND_EQ));
		    yyval.x = NULL; }
    break;
case 71:
#line 374 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_EQ));
		    yyval.x = NULL; }
    break;
case 72:
#line 377 "grammar.y"
{   gramerr(aexp_str, precede, opStrings(CND_NEQ));
		    yyval.x = NULL; }
    break;
case 73:
#line 380 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_NEQ));
		    yyval.x = NULL; }
    break;
case 74:
#line 383 "grammar.y"
{   gramerr(aexp_str, precede, opStrings(CND_LT));
		    yyval.x = NULL; }
    break;
case 75:
#line 386 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_LT));
		    yyval.x = NULL; }
    break;
case 76:
#line 389 "grammar.y"
{   gramerr(aexp_str, precede, opStrings(CND_GT));
		    yyval.x = NULL; }
    break;
case 77:
#line 392 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_GT));
		    yyval.x = NULL; }
    break;
case 78:
#line 395 "grammar.y"
{   gramerr(aexp_str, precede, opStrings(CND_LTE));
		    yyval.x = NULL; }
    break;
case 79:
#line 398 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_LTE));
		    yyval.x = NULL; }
    break;
case 80:
#line 401 "grammar.y"
{   gramerr(aexp_str, precede, opStrings(CND_GTE));
		    yyval.x = NULL; }
    break;
case 81:
#line 404 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_GTE));
		    yyval.x = NULL; }
    break;
case 82:
#line 409 "grammar.y"
{   yyval.x = yyvsp[-1].x; }
    break;
case 83:
#line 411 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 84:
#line 413 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 85:
#line 415 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 86:
#line 417 "grammar.y"
{   yyval.x = yyvsp[0].x; }
    break;
case 87:
#line 419 "grammar.y"
{   yyval.x = numMergeExpr(CND_RATE, yyvsp[0].x); }
    break;
case 88:
#line 421 "grammar.y"
{   yyval.x = unaryExpr(CND_NEG, yyvsp[0].x); }
    break;
case 89:
#line 423 "grammar.y"
{   yyval.x = binaryExpr(CND_ADD, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 90:
#line 425 "grammar.y"
{   yyval.x = binaryExpr(CND_SUB, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 91:
#line 427 "grammar.y"
{   yyval.x = binaryExpr(CND_MUL, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 92:
#line 429 "grammar.y"
{   yyval.x = binaryExpr(CND_DIV, yyvsp[-2].x, yyvsp[0].x); }
    break;
case 93:
#line 433 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_RATE));
		    yyval.x = NULL; }
    break;
case 94:
#line 436 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_NEG));
		    yyval.x = NULL; }
    break;
case 95:
#line 440 "grammar.y"
{   gramerr(aexp_str, precede, opStrings(CND_ADD));
		    yyval.x = NULL; }
    break;
case 96:
#line 443 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_ADD));
		    yyval.x = NULL; }
    break;
case 97:
#line 446 "grammar.y"
{   gramerr(aexp_str, precede, opStrings(CND_SUB));
		    yyval.x = NULL; }
    break;
case 98:
#line 449 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_SUB));
		    yyval.x = NULL; }
    break;
case 99:
#line 452 "grammar.y"
{   gramerr(aexp_str, precede, opStrings(CND_MUL));
		    yyval.x = NULL; }
    break;
case 100:
#line 455 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_MUL));
		    yyval.x = NULL; }
    break;
case 101:
#line 458 "grammar.y"
{   gramerr(aexp_str, precede, opStrings(CND_DIV));
		    yyval.x = NULL; }
    break;
case 102:
#line 461 "grammar.y"
{   gramerr(aexp_str, follow, opStrings(CND_DIV));
		    yyval.x = NULL; }
    break;
case 103:
#line 467 "grammar.y"
{   yyval.x = domainExpr(CND_SUM_HOST, yyvsp[-1].i, yyvsp[0].x); }
    break;
case 104:
#line 469 "grammar.y"
{   yyval.x = domainExpr(CND_AVG_HOST, yyvsp[-1].i, yyvsp[0].x); }
    break;
case 105:
#line 471 "grammar.y"
{   yyval.x = domainExpr(CND_MAX_HOST, yyvsp[-1].i, yyvsp[0].x); }
    break;
case 106:
#line 473 "grammar.y"
{   yyval.x = domainExpr(CND_MIN_HOST, yyvsp[-1].i, yyvsp[0].x); }
    break;
case 107:
#line 475 "grammar.y"
{   yyval.x = domainExpr(CND_COUNT_HOST, yyvsp[-1].i, yyvsp[0].x); }
    break;
case 108:
#line 479 "grammar.y"
{   gramerr(aexp_str, follow, aggr_str);
		    yyval.x = NULL; }
    break;
case 109:
#line 482 "grammar.y"
{   gramerr(aexp_str, follow, aggr_str);
		    yyval.x = NULL; }
    break;
case 110:
#line 485 "grammar.y"
{   gramerr(aexp_str, follow, aggr_str);
		    yyval.x = NULL; }
    break;
case 111:
#line 488 "grammar.y"
{   gramerr(aexp_str, follow, aggr_str);
		    yyval.x = NULL; }
    break;
case 112:
#line 493 "grammar.y"
{   yyval.i = HOST_DOM; }
    break;
case 113:
#line 495 "grammar.y"
{   yyval.i = INST_DOM; }
    break;
case 114:
#line 497 "grammar.y"
{   yyval.i = TIME_DOM; }
    break;
case 115:
#line 501 "grammar.y"
{   yyval.x = fetchExpr(yyvsp[-3].s, yyvsp[-2].sa, yyvsp[-1].sa, yyvsp[0].t); }
    break;
case 116:
#line 505 "grammar.y"
{   yyval.s = yyvsp[0].s; }
    break;
case 117:
#line 509 "grammar.y"
{   yyval.sa.n = 0;
		    yyval.sa.ss = NULL; }
    break;
case 118:
#line 512 "grammar.y"
{   yyval.sa.n = yyvsp[-2].sa.n + 1;
		    yyval.sa.ss = (char **) ralloc(yyvsp[-2].sa.ss, yyval.sa.n * sizeof(char *));
		    yyval.sa.ss[yyvsp[-2].sa.n] = yyvsp[0].s; }
    break;
case 119:
#line 518 "grammar.y"
{   gramerr(host_str, follow, ":");
                    yyval.sa.n = 0;
		    yyval.sa.ss = NULL; }
    break;
case 120:
#line 524 "grammar.y"
{   yyval.sa.n = 0;
		    yyval.sa.ss = NULL; }
    break;
case 121:
#line 527 "grammar.y"
{   yyval.sa.n = yyvsp[-2].sa.n + 1;
		    yyval.sa.ss = (char **) ralloc(yyvsp[-2].sa.ss, yyval.sa.n * sizeof(char *));
		    yyval.sa.ss[yyvsp[-2].sa.n] = yyvsp[0].s; }
    break;
case 122:
#line 533 "grammar.y"
{   gramerr(inst_str, follow, "#");
                    yyval.sa.n = 0;
		    yyval.sa.ss = NULL; }
    break;
case 123:
#line 539 "grammar.y"
{   yyval.t.t1 = 0;
		    yyval.t.t2 = 0; }
    break;
case 124:
#line 542 "grammar.y"
{   yyval.t.t1 = yyvsp[0].d;
		    yyval.t.t2 = yyvsp[0].d; }
    break;
case 125:
#line 545 "grammar.y"
{   if (yyvsp[-2].d <= yyvsp[0].d) {
			yyval.t.t1 = yyvsp[-2].d;
			yyval.t.t2 = yyvsp[0].d;
		    }
		    else {
			yyval.t.t1 = yyvsp[0].d;
			yyval.t.t2 = yyvsp[-2].d;
		    } }
    break;
case 126:
#line 556 "grammar.y"
{   gramerr(sample_str, follow, "@");
                    yyval.t.t1 = 0;
                    yyval.t.t2 = 0; }
    break;
case 127:
#line 562 "grammar.y"
{   yyval.x = numConst(yyvsp[-1].d, yyvsp[0].u); }
    break;
case 128:
#line 566 "grammar.y"
{   yyval.u = noUnits; }
    break;
case 129:
#line 568 "grammar.y"
{   yyval.u = yyvsp[-1].u;
		    if (yyvsp[0].u.dimSpace) {
			yyval.u.dimSpace = yyvsp[0].u.dimSpace;
			yyval.u.scaleSpace = yyvsp[0].u.scaleSpace;
		    }
		    else if (yyvsp[0].u.dimTime) {
			yyval.u.dimTime = yyvsp[0].u.dimTime;
			yyval.u.scaleTime = yyvsp[0].u.scaleTime;
		    }
		    else {
			yyval.u.dimCount = yyvsp[0].u.dimCount;
			yyval.u.scaleCount = yyvsp[0].u.scaleCount;
		    } }
    break;
case 130:
#line 582 "grammar.y"
{   yyval.u = yyvsp[-2].u;
		    if (yyvsp[0].u.dimSpace) {
			yyval.u.dimSpace = -yyvsp[0].u.dimSpace;
			yyval.u.scaleSpace = yyvsp[0].u.scaleSpace;
		    }
		    else if (yyvsp[0].u.dimTime) {
			yyval.u.dimTime = -yyvsp[0].u.dimTime;
			yyval.u.scaleTime = yyvsp[0].u.scaleTime;
		    }
		    else {
			yyval.u.dimCount = -yyvsp[0].u.dimCount;
			yyval.u.scaleCount = yyvsp[0].u.scaleCount;
		    } }
    break;
case 131:
#line 598 "grammar.y"
{   yyval.u = yyvsp[0].u; }
    break;
case 132:
#line 600 "grammar.y"
{   yyval.u = yyvsp[-2].u;
		    yyval.u.dimSpace = yyvsp[0].d; }
    break;
case 133:
#line 603 "grammar.y"
{   yyval.u = yyvsp[0].u; }
    break;
case 134:
#line 605 "grammar.y"
{   yyval.u = yyvsp[-2].u;
		    yyval.u.dimTime = yyvsp[0].d; }
    break;
case 135:
#line 608 "grammar.y"
{   yyval.u = yyvsp[0].u; }
    break;
case 136:
#line 610 "grammar.y"
{   yyval.u = yyvsp[-2].u;
		    yyval.u.dimCount = yyvsp[0].d; }
    break;
case 137:
#line 615 "grammar.y"
{   yyval.x = strConst(yyvsp[0].s); }
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
#line 618 "grammar.y"


