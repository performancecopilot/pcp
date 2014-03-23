/*
 *
 * Simple cpp replacement to be used to pre-process a PMNS from
 * pmLoadNameSpace() in libpcp.
 *
 * Supports ...
 * - #define name value
 *   no spaces in value, no quotes or escapes
 *   name begins with an alpha or _, then zero or more alphanumeric or _
 *   value is optional and defaults to the empty string
 * - macro substitution
 * - standard C-style comment stripping
 * - #include "file" or #include <file>
 *   up to a depth of 5 levels, for either syntax the directory search
 *   is hard-wired to <file>, the directory of command line file (if any)
 *   and then $PCP_VAR_DIR/pmns
 * - #ifdef ... #endif and #ifndef ... #endif
 *
 * Does NOT support ...
 * - #if
 * - nested #ifdef
 * - error recovery - first error is fatal
 * - C++ style // comments
 *
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "pmapi.h"
#include "impl.h"
#include <ctype.h>
#include <sys/stat.h>

static char	ibuf[256];		/* input line buffer */

/*
 optind #include file control
 * allow MAXLEVEL-1 levels of #include
 */
#define MAXLEVEL	5
static struct {
    char	*fname;
    FILE	*fin;
    int		lineno;
} file_ctl[MAXLEVEL], *currfile = NULL;

/*
 * macro definitions via #define
 */
typedef struct {
    int		len;
    char	*name;
    char	*value;
} macro_t;
static macro_t	*macro = NULL;
static int	nmacro = 0;

static void err(char *) __attribute__((noreturn));

/*
 * use pmprintf for fatal messages as we're usually run from
 * pmLoadNameSpace() in libpcp
 */
static void
err(char *msg)
{
    fflush(stdout);
    if (currfile != NULL) {
	if (currfile->lineno > 0)
	    pmprintf("pmcpp: %s[%d]: %s", currfile->fname, currfile->lineno, ibuf);
	else
	    pmprintf("pmcpp: %s:\n", currfile->fname);
    }
    pmprintf("pmcpp: Error: %s\n", msg);
    pmflush();
    exit(1);
}

#define OP_DEFINE	1
#define OP_UNDEF	2
#define OP_IFDEF	3
#define OP_IFNDEF	4
#define OP_ENDIF	5
#define IF_FALSE	0
#define IF_TRUE		1
#define IF_NONE		2
/*
 * handle # cpp directives
 * - return 0 for do nothing and include following lines
 * - return 1 to skip following lines
 */
static int
directive(void)
{
    char	*ip;
    char	*name = NULL;
    char	*value = NULL;
    int		namelen = 0;		/* pander to gcc */
    int		valuelen = 0;		/* pander to gcc */
    int		op;
    int		i;
    static int	in_if = IF_NONE;

    if (strncmp(ibuf, "#define", strlen("#define")) == 0) {
	ip = &ibuf[strlen("#define")];
	op = OP_DEFINE;
    }
    else if (strncmp(ibuf, "#undef", strlen("#undef")) == 0) {
	ip = &ibuf[strlen("#undef")];
	op = OP_UNDEF;
    }
    else if (strncmp(ibuf, "#ifdef", strlen("#ifdef")) == 0) {
	ip = &ibuf[strlen("#ifdef")];
	op = OP_IFDEF;
    }
    else if (strncmp(ibuf, "#ifndef", strlen("#ifndef")) == 0) {
	ip = &ibuf[strlen("#ifndef")];
	op = OP_IFNDEF;
    }
    else if (strncmp(ibuf, "#endif", strlen("#endif")) == 0) {
	ip = &ibuf[strlen("#endif")];
	op = OP_ENDIF;
    }
    else {
	err("Unrecognized control line");
	/*NOTREACHED*/
    }

    while (*ip && isblank((int)*ip)) ip++;
    if (op != OP_ENDIF) {
	if (*ip == '\n' || *ip == '\0') {
	    err("Missing macro name");
	    /*NOTREACHED*/
	}
	name = ip;
	for ( ;*ip && !isblank((int)*ip); ip++) {
	    if (isalpha((int)*ip) || *ip == '_') continue;
	    if (ip > name &&
		(isdigit((int)*ip) || *ip == '_'))
		    continue;
	    break;
	}
	if (!isspace((int)*ip)) {
	    err("Illegal character in macro name");
	    /*NOTREACHED*/
	}
	namelen = ip - name;
	if (op == OP_DEFINE) {
	    if (*ip == '\n' || *ip == '\0') {
		value = "";
		valuelen = 0;
	    }
	    else {
		while (*ip && isblank((int)*ip)) ip++;
		value = ip;
		while (!isspace((int)*ip)) ip++;
		valuelen = ip - value;
	    }
	}
    }

    while (*ip && isblank((int)*ip)) ip++;
    if (*ip != '\n' && *ip != '\0') {
	err("Unexpected extra text in a control line");
	/*NOTREACHED*/
    }

    if (op == OP_ENDIF) {
	if (in_if != IF_NONE)
	    in_if = IF_NONE;
	else {
	    err("No matching #ifdef or #ifndef for #endif");
	    /*NOTREACHED*/
	}
	return 0;
    }

    if (op == OP_IFDEF || op == OP_IFNDEF) {
	if (in_if != IF_NONE) {
	    err("Nested #ifdef or #ifndef");
	    /*NOTREACHED*/
	}
    }

    if (in_if == IF_FALSE)
	/* skipping, waiting for #endif to match #if[n]def */
	return 1;
    
    for (i = 0; i < nmacro; i++) {
	if (macro[i].len != namelen ||
	    strncmp(name, macro[i].name, macro[i].len) != 0)
	    continue;
	/* found a match */
	if (op == OP_IFDEF) {
	    in_if = IF_TRUE;
	    return 0;
	}
	else if (op == OP_IFNDEF) {
	    in_if = IF_FALSE;
	    return 1;
	}
	else if (op == OP_UNDEF) {
	    macro[i].len = 0;
	    return 0;
	}
	else {
	    err("Macro redefinition");
	    /*NOTREACHED*/
	}
    }
    
    /* no matching macro name */
    if (op == OP_IFDEF) {
	in_if = IF_FALSE;
	return 1;
    }
    else if (op == OP_IFNDEF) {
	in_if = IF_TRUE;
	return 0;
    }
    else if (op == OP_UNDEF)
	/* silently accept #undef for something that was not defined */
	return 0;
    else {
	/* OP_DEFINE case */
	macro = (macro_t *)realloc(macro, (nmacro+1)*sizeof(macro_t));
	if (macro == NULL) {
	    __pmNoMem("pmcpp: macro[]", (nmacro+1)*sizeof(macro_t), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	macro[nmacro].len = namelen;
	macro[nmacro].name = (char *)malloc(namelen+1);
	if (macro[nmacro].name == NULL) {
	    __pmNoMem("pmcpp: name", namelen+1, PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	strncpy(macro[nmacro].name, name, namelen);
	macro[nmacro].name[namelen] = '\0';
	macro[nmacro].value = (char *)malloc(valuelen+1);
	if (macro[nmacro].value == NULL) {
	    __pmNoMem("pmcpp: value", valuelen+1, PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	if (value && valuelen)
	    strncpy(macro[nmacro].value, value, valuelen);
	macro[nmacro].value[valuelen] = '\0';
	nmacro++;
	return 0;
    }
}

static void
do_macro(void)
{
    /*
     * break line into words at white space or '.' or ':' boundaries
     * and apply macro substitution to each word
     */
    char	*ip = ibuf;	/* next from ibuf[] to be copied */
    char	*w;		/* start of word */
    int		len;
    char	tmp[256];	/* copy output line here */
    char	*op = tmp;
    int		sub = 0;	/* true if any substitution made */


    while (*ip && isblank((int)*ip))
	*op++ = *ip++;
    w = ip;
    for ( ; ; ) {
	if (isspace((int)*ip) || *ip == '.' || *ip == ':' || *ip == '\0') {
	    len = ip - w;
// printf("word=|%*.*s|\n", len, len, w);
	    if (len > 0) {
		int		i;
		int		match = 0;
		for (i = 0; i < nmacro; i++) {
		    if (len == macro[i].len &&
			strncmp(w, macro[i].name, len) == 0) {
			match = 1;
			sub++;
			strcpy(op, macro[i].value);
			op += strlen(macro[i].value);
			break;
		    }
		}
		if (match == 0) {
		    strncpy(op, w, len);
		    op += len;
		}
	    }
	    *op++ = *ip;
	    if (*ip == '\n' || *ip == '\0')
		break;
	    ip++;
	    while (*ip && isblank((int)*ip))
		*op++ = *ip++;
	    w = ip;
	}
	ip++;
    }

    if (sub) {
	*op = '\0';
	strcpy(ibuf, tmp);
    }
}

/*
 * Open a regular file for reading, checking that its regular and accessible
 */
FILE *
openfile(const char *fname)
{
    struct stat sbuf;
    FILE *fp = fopen(fname, "r");

    if (!fp)
	return NULL;
    if (fstat(fileno(fp), &sbuf) < 0) {
	fclose(fp);
	return NULL;
    }
    if (!S_ISREG(sbuf.st_mode)) {
	fclose(fp);
	setoserror(ENOENT);
	return NULL;
    }
    return fp;
}

static pmLongOptions longopts[] = {
    PMOPT_HELP,
    { "define", 1, 'D', "MACRO", "associate a value with a macro" },
    PMAPI_OPTIONS_END
};
static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[-Dname ...] [file]",
};

int
main(int argc, char **argv)
{
    int		c;
    int		skip_if_false = 0;
    int		incomment = 0;
    char	*ip;

    currfile = &file_ctl[0];

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'D':	/* define */
	    for (ip = opts.optarg; *ip; ip++) {
		if (*ip == '=')
		    *ip = ' ';
	    }
	    snprintf(ibuf, sizeof(ibuf), "#define %s\n", opts.optarg);
	    currfile->fname = "<arg>";
	    currfile->lineno = opts.optind;
	    directive();
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors || opts.optind < argc - 1) {
	pmUsageMessage(&opts);
	exit(1);
    }

    currfile->lineno = 0;
    if (opts.optind == argc) {
	currfile->fname = "<stdin>";
	currfile->fin = stdin;
    }
    else {
	currfile->fname = argv[opts.optind];
	currfile->fin = openfile(currfile->fname);
	if (currfile->fin == NULL) {
	    err((char *)pmErrStr(-oserror()));
	    /*NOTREACHED*/
	}
    }
    printf("# %d \"%s\"\n", currfile->lineno+1, currfile->fname);

    for ( ; ; ) {
	if (fgets(ibuf, sizeof(ibuf), currfile->fin) == NULL) {
	    fclose(currfile->fin);
	    if (currfile == &file_ctl[0])
		break;
	    free(currfile->fname);
	    currfile--;
	    printf("# %d \"%s\"\n", currfile->lineno+1, currfile->fname);
	    continue;
	}
	currfile->lineno++;
 
	/* strip comments ... */
	for (ip = ibuf; *ip ; ip++) {
	    if (incomment) {
		if (*ip == '*' && ip[1] == '/') {
		    /* end of comment */
		    incomment = 0;
		    *ip++ = ' ';
		    *ip = ' ';
		}
		else
		    *ip = ' ';
	    }
	    else {
		if (*ip == '/' && ip[1] == '*') {
		    /* start of comment */
		    incomment = currfile->lineno;
		    *ip++ = ' ';
		    *ip = ' ';
		}
	    }
	}
	ip--;
	while (ip >= ibuf && isspace((int)*ip)) ip--;
	*++ip = '\n';
	*++ip = '\0';
	if (incomment && ibuf[0] == '\n') {
	    printf("\n");
	    continue;
	}

	if (ibuf[0] == '#') {
	    /* cpp control line */
	    if (strncmp(ibuf, "#include", strlen("#include")) == 0) {
		char		*p;
		char		*pend;
		char		c;
		FILE		*f;
		static char	tmpbuf[MAXPATHLEN];

		if (skip_if_false) {
		    printf("\n");
		    continue;
		}
		p = &ibuf[strlen("#include")];
		while (*p && isblank((int)*p)) p++;
		if (*p != '"' && *p != '<') {
		    err("Expected \" or < after #include");
		    /*NOTREACHED*/
		}
		pend = ++p;
		while (*pend && *pend != '\n' &&
		       ((p[-1] != '"' || *pend != '"') &&
		        (p[-1] != '<' || *pend != '>'))) pend++;
		if (p[-1] == '"' && *pend != '"') {
		    err("Expected \" after file name");
		    /*NOTREACHED*/
		}
		if (p[-1] == '<' && *pend != '>') {
		    err("Expected > after file name");
		    /*NOTREACHED*/
		}
		if (currfile == &file_ctl[MAXLEVEL-1]) {
		    err("#include nesting too deep");
		    /*NOTREACHED*/
		}
		if (pend[1] != '\n' && pend[1] != '\0') {
		    err("Unexpected extra text in #include line");
		    /*NOTREACHED*/
		}
		c = *pend;
		*pend = '\0';
		f = openfile(p);
		if (f == NULL && file_ctl[0].fin != stdin) {
		    /* check in directory of file from command line */
		    static int	sep;
		    static char	*dir = NULL;
		    if (dir == NULL) {
			/*
			 * some versions of dirname() clobber the input
			 * argument, some do not ... hence the obscurity
			 * here
			 */
			static char	*dirbuf;
			dirbuf = strdup(file_ctl[0].fname);
			if (dirbuf == NULL) {
			    __pmNoMem("pmcpp: dir name alloc", strlen(file_ctl[0].fname)+1, PM_FATAL_ERR);
			    /*NOTREACHED*/
			}
			dir = dirname(dirbuf);
			sep = __pmPathSeparator();
		    }
		    snprintf(tmpbuf, sizeof(tmpbuf), "%s%c%s", dir, sep, p);
		    f = openfile(tmpbuf);
		    if (f != NULL)
			p = tmpbuf;
		}
		if (f == NULL) {
		    /* check in $PCP_VAR_DIR/pmns */
		    static int	sep;
		    static char	*var_dir = NULL;
		    if (var_dir == NULL) {
			var_dir = pmGetConfig("PCP_VAR_DIR");
			sep = __pmPathSeparator();
		    }
		    snprintf(tmpbuf, sizeof(tmpbuf), "%s%cpmns%c%s", var_dir, sep, sep, p);
		    f = openfile(tmpbuf);
		    if (f != NULL)
			p = tmpbuf;
		}
		if (f == NULL) {
		    *pend = c;
		    err("Cannot open file for #include");
		    /*NOTREACHED*/
		}
		currfile++;
		currfile->lineno = 0;
		currfile->fin = f;
		currfile->fname = strdup(p);
		*pend = c;
		if (currfile->fname == NULL) {
		    __pmNoMem("pmcpp: file name alloc", strlen(p)+1, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		printf("# %d \"%s\"\n", currfile->lineno+1, currfile->fname);
	    }
	    else {
		/* expect other cpp control ... */
		skip_if_false = directive();
		printf("\n");
	    }
	    continue;
	}
	if (skip_if_false)
	    printf("\n");
	else {
	    if (nmacro > 0)
		do_macro();
	    printf("%s", ibuf);
	}
    }

    /* EOF for the top level file */
    if (incomment) {
	char	msgbuf[80];
	snprintf(msgbuf, sizeof(msgbuf), "Comment at line %d not terminated before end of file", incomment);
	currfile->lineno = 0;
	err(msgbuf);
	exit(1);
    }

    exit(0);
}
