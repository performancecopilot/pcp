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


extern YYSTYPE yylval;

#endif /* not BISON_GRAM_TAB_H */
