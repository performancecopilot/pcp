#ifndef BISON_GRAM_TAB_H
# define BISON_GRAM_TAB_H

#ifndef YYSTYPE
typedef union {
    long lval;
    char * str;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
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


extern YYSTYPE yylval;

#endif /* not BISON_GRAM_TAB_H */
