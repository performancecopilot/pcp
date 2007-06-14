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
# define	NAME	260
# define	STRING	261
# define	NUMBER	262


extern YYSTYPE yylval;

#endif /* not BISON_GRAM_TAB_H */
