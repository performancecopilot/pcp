#ifndef BISON_GRAM_TAB_H
# define BISON_GRAM_TAB_H

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


extern YYSTYPE yylval;

#endif /* not BISON_GRAM_TAB_H */
