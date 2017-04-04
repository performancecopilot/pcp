/*
 *
 * Simple cpp replacement to be used to pre-process a PMNS from
 * pmLoadNameSpace() in libpcp.
 *
 * Supports ...
 * - #define name value
 *   #define name 'value'
 *   #define name "value"
 *   no spaces in unquoted value, no escapes, no newlines
 *   name begins with an alpha or _, then zero or more alphanumeric or _
 *   value is optional and defaults to the empty string
 * - macro substitution
 * - standard C-style comment stripping
 * - #include "file" or #include <file>
 *   up to a depth of 5 levels (#shell and/or #include)
 *   for either syntax the directory search is hard-wired to <file>,
 *   the directory of command line file (if any), any -I command line
 *   dirs and then $PCP_VAR_DIR/pmns
 * - #shell "cmd" or #shell 'cmd'
 *   up to a depth of 5 levels (#shell and/or #include)
 * - #ifdef ... #endif and #ifndef ... #endif
 *
 * Does NOT support ...
 * - macros with parameters
 * - #if <expr>
 * - nested #ifdef
 * - C++ style // comments
 * - error recovery - first error is fatal
 * - -o, -W, -U, -x, ... command line options
 *
 * STYLE_SH (-s) variant
 * - intended for configuration files with sh-like comment convention,
 *   i.e. # introduces a comment
 * - pmcpp control character becomes % instead of #, so %include, %ifdef, etc
 * - no # lineno "filename" linemarker lines
 *
 * Copyright (c) 2011-2015 Ken McDonell.  All Rights Reserved.
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
#include <sys/types.h>
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#include <stdarg.h>
#include <string.h>

static int debug;			/* -d sets this to 1 for debugging */

static char	*ibuf;			/* input line buffer */
static int	ibuflen;
static int	inlen;			/* input line length */
static int	nline_in;		/* number of input lines read */
static int	nline_out;		/* number of output lines written */
static int	nline_sub;		/* number of lines requiring some macro substitution */
static int	nsub;			/* number of macro substitutions */

/*
 * ?include file control
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
    int		namelen;		/* length of name[] */
    char	*name;
    int		valuelen;		/* length of value[] */
    char	*value;
} macro_t;
static macro_t	*macro = NULL;
static int	nmacro;

/*
 * include file directories
 */
typedef struct {
    char	*dirname;
} incdir_t;
static incdir_t	*incdir = NULL;
static int	nincdir;

static int	sep;

#define STYLE_C	1
#define STYLE_SH 2
static int	style = STYLE_C;	/* STYLE_SH if -s on command line */
static char	ctl = '#';

static int	Pflag;			/* set if -P */

#define IF_FALSE	0
#define IF_TRUE		1
#define IF_NONE		2
static int	in_if = IF_NONE;	/* #if... control */
static int	if_lineno;		/* lineno of last #if... */

static int	rflag;			/* 1 if -r on the command line */

static void err(const char *, ...) __attribute__((noreturn));

/*
 * use stderr for fatal messages ...
 */
static void
err(const char *msg, ...)
{
    va_list	arg;

    fflush(stdout);
    if (currfile != NULL) {
	if (currfile->lineno > 0)
	    fprintf(stderr, "pmcpp: %s[%d]: %s", currfile->fname, currfile->lineno, ibuf);
	else
	    fprintf(stderr, "pmcpp: %s:\n", currfile->fname);
    }
    va_start(arg, msg);
    fprintf(stderr, "pmcpp: Error: ");
    vfprintf(stderr, msg, arg);
    fprintf(stderr, "\n");
    exit(1);
}

#define OP_DEFINE	1
#define OP_UNDEF	2
#define OP_IFDEF	3
#define OP_IFNDEF	4
#define OP_ELSE		5
#define OP_ENDIF	6
/*
 * handle pmcpp directives
 * ibuf[0] contains a pmcpp directive - # (or % for style == STYLE_SH)
 * - return 0 for do nothing and include following lines
 * - return 1 to skip following lines
 * - return -1 if not a valid directive (action depends on rflag)
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

    if (strncmp(&ibuf[1], "define", strlen("define")) == 0) {
	ip = &ibuf[strlen("?define")];
	op = OP_DEFINE;
    }
    else if (strncmp(&ibuf[1], "undef", strlen("undef")) == 0) {
	ip = &ibuf[strlen("?undef")];
	op = OP_UNDEF;
    }
    else if (strncmp(&ibuf[1], "ifdef", strlen("ifdef")) == 0) {
	ip = &ibuf[strlen("?ifdef")];
	op = OP_IFDEF;
    }
    else if (strncmp(&ibuf[1], "ifndef", strlen("ifndef")) == 0) {
	ip = &ibuf[strlen("?ifndef")];
	op = OP_IFNDEF;
    }
    else if (strncmp(&ibuf[1], "endif", strlen("endif")) == 0) {
	ip = &ibuf[strlen("?endif")];
	op = OP_ENDIF;
    }
    else if (strncmp(&ibuf[1], "else", strlen("else")) == 0) {
	ip = &ibuf[strlen("?else")];
	op = OP_ELSE;
    }
    else {
	/* not a control line we recognize */
	return -1;
    }

    while (*ip && isblank((int)*ip)) ip++;
    if (op != OP_ENDIF && op != OP_ELSE) {
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
		char	quote = '\0';
		while (*ip && isblank((int)*ip)) ip++;
		if (*ip == '\'' || *ip == '"')
		    quote = *ip++;
		value = ip;
		while (*ip &&
		       ((quote == '\0' && !isspace((int)*ip)) ||
		        (quote != '\0' && *ip != quote))) ip++;
		if (quote != '\0' && *ip != quote) {
		    err("Unterminated value string in %cdefine", ctl);
		    return 1;
		}
		valuelen = ip - value;
		if (quote != '\0')
		    ip++;
	    }
	    if (debug)
		printf("<<macro %*.*s=\"%*.*s\"\n", namelen, namelen, name, valuelen, valuelen, value);
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
	    err("No matching %cifdef or %cifndef for %cendif", ctl, ctl, ctl);
	    /*NOTREACHED*/
	}
	return 0;
    }
    else if (op == OP_ELSE) {
	if (in_if != IF_NONE)
	    in_if = 1 - in_if;	/* reverse truth value */
	else {
	    err("No matching %cifdef or %cifndef for %celse", ctl, ctl, ctl);
	    /*NOTREACHED*/
	}
    }
    else if (op == OP_IFDEF || op == OP_IFNDEF) {
	if (in_if != IF_NONE) {
	    err("Nested %cifdef or %cifndef", ctl, ctl);
	    /*NOTREACHED*/
	}
    }

    if (in_if == IF_FALSE)
	/* skipping, waiting for ?endif to match ?if[n]def */
	return 1;
    
    for (i = 0; i < nmacro; i++) {
	if (macro[i].namelen != namelen ||
	    strncmp(name, macro[i].name, macro[i].namelen) != 0)
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
	    macro[i].namelen = 0;
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
	if_lineno = currfile->lineno;
	return 1;
    }
    else if (op == OP_IFNDEF) {
	in_if = IF_TRUE;
	if_lineno = currfile->lineno;
	return 0;
    }
    else if (op == OP_UNDEF)
	/* silently accept ?undef for something that was not defined */
	return 0;
    else {
	/* OP_DEFINE case */
	macro = (macro_t *)realloc(macro, (nmacro+1)*sizeof(macro_t));
	if (macro == NULL) {
	    __pmNoMem("pmcpp: macro[]", (nmacro+1)*sizeof(macro_t), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	macro[nmacro].namelen = namelen;
	macro[nmacro].name = (char *)malloc(namelen+1);
	if (macro[nmacro].name == NULL) {
	    __pmNoMem("pmcpp: name", namelen+1, PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	strncpy(macro[nmacro].name, name, namelen);
	macro[nmacro].name[namelen] = '\0';
	macro[nmacro].valuelen = valuelen;
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
     * break line into tokens at non-name character [a-zA-Z][a-zA-Z0-9_]*
     * or if -r then #... and #{...} boundaries and apply macro
     * substitution to each token
     */
    char	*ip = ibuf;	/* next from ibuf[] to be copied */
    char	*tp;		/* start of token */
    int		len;
    static char	*obuf = NULL;	/* copy output line here */
    static int	obuflen;
    char	*op = obuf;
    int		sub = 0;	/* true if any substitution made */

    /*
     * optimistic non-copying ... as we scan the input buffer we
     * do not copy to the output buffer until at least one macro
     * substitution has been made (sub > 0) ... this saves about
     * 9% cpu time for the common case where most lines processed
     * do not involve any macro processing.
     */

    /* get to the start of the first possible macro name */
    if (rflag) {
	while (*ip && *ip != ctl) {
	    ip++;
	}
    }
    else {
	while (*ip && !isalpha((int)*ip) && *ip != '_') {
	    ip++;
	}
    }
    if (*ip == '\0')
	return;

    tp = ip;
    for ( ; ; ) {
	int	tok_end;
	if (ip == tp)
	    /* skip first character of token */
	    tok_end = 0;
	else if (rflag) {
	    if (ip == &tp[1]) {
		/* second character could be { or start of name */
		if (*ip == '{' || isalnum(*ip))
		    tok_end = 0;
		else
		    tok_end = 1;
	    }
	    else if (ip == &tp[2] && tp[1] == '{')
		/* third character could be start of name if following { */
		if (isalnum(*ip))
		    tok_end = 0;
		else
		    tok_end = 1;
	    else if (tp[1] != '{' && (isalnum(*ip) || *ip == '_'))
		tok_end = 0;
	    else if (tp[1] == '{') {
		if (*ip != '}')
		    tok_end = 0;
		else {
		    ip++;
		    tok_end = 1;
		}
	    }
	    else
		tok_end = 1;
	}
	else {
	    if (!isalnum((int)*ip) && *ip != '_')
		tok_end = 1;
	    else
		tok_end = 0;
	}
	
	/* found the end of a possible macro name */
	if (tok_end) {
	    len = ip - tp;
	    if (len > 0) {
		int		i;
		int		match = 0;
		int		tlen = len;
		char		*token = tp;
		if (debug) printf("<<name=\"%*.*s\"\n", len, len, tp);
		if (rflag) {
		    if (len < 2) {
			/*
			 * single % or # and not alpha|underscore and not {
			 */
			tlen = 0;
		    }
		    else if (token[1] == '{') {
			/* token => skip ?{ at the start and } at the end */
			token = &token[2];
			tlen -= 3;
		    }
		    else {
			/* token => skip ? at the start */
			token = &token[1];
			tlen -= 1;
		    }
		}
		if (tlen > 0) {
		    for (i = 0; i < nmacro; i++) {
			if (tlen == macro[i].namelen &&
			    strncmp(token, macro[i].name, tlen) == 0) {
			    int	need;	/* expected output line length */
			    int	sofar;
			    if (debug) printf("<<value=\"%s\"\n", macro[i].value);
			    match = 1;
			    /* check output buffer is big enough ...  */
			    need = inlen - len + macro[i].valuelen;
			    if (need > obuflen) {
				/* resize output buffer */
				if (obuflen == 0)
				    obuflen = ibuflen;
				while (need > obuflen)
				    obuflen *= 2;
				sofar = op - obuf;
				if ((obuf = (char *)realloc(obuf, obuflen)) == NULL) {
				    __pmNoMem("pmcpp: obuf realloc", obuflen, PM_FATAL_ERR);
				    /*NOTREACHED*/
				}
				if (debug)
				    printf("<<increased obuf[] to %d chars\n", obuflen);
				op = &obuf[sofar];
			    }
			    if (sub == 0) {
				/*
				 * first macro replacement for this line, ...
				 * copy input line up to start of macro name
				 * to the output buffer and continue to copy
				 * while processing to the right of here ...
				 */
				nline_sub++;
				memcpy(obuf, ibuf, ip-len-ibuf);
				op = &obuf[ip-len-ibuf];
			    }
			    nsub++;
			    sub++;
			    memcpy(op, macro[i].value, macro[i].valuelen);
			    op += macro[i].valuelen;
			    break;
			}
		    }
		}
		if (match == 0 && sub > 0) {
		    strncpy(op, tp, len);
		    op += len;
		}
	    }
	    /* get to the start of the next possible macro name */
	    if (rflag) {
		while (*ip && *ip != ctl) {
		    if (sub)
			*op++ = *ip;
		    ip++;
		}
	    }
	    else {
		while (*ip && !isalpha((int)*ip) && *ip != '_') {
		    if (sub)
			*op++ = *ip;
		    ip++;
		}
	    }
	    if (*ip == '\0')
		break;
	    tp = ip;
	}
	ip++;
    }

    if (sub) {
	*op = '\0';
	memcpy(ibuf, obuf, op-obuf+1);
    }
}

/*
 * Open a regular file (or character device, like /dev/null!) for
 * reading, checking that it is accessible
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
    if (!S_ISREG(sbuf.st_mode) && !S_ISCHR(sbuf.st_mode)) {
	fclose(fp);
	setoserror(ENOENT);
	return NULL;
    }
    return fp;
}

static pmLongOptions longopts[] = {
    PMOPT_HELP,
    { "define", 1, 'D', "name[=value]", "define a macro with an optional value" },
    { "include", 1, 'I', "dir", "additional directory to search for include files" },
    { "", 0, 'P', "", "do not output # lineno \"filename\" linemarkers" },
    { "restrict", 0, 'r', "", "restrict macro expansion to #name or #{name}" },
    { "shell", 0, 's', "", "use alternate control syntax with % instead of #" },
    PMAPI_OPTIONS_END
};
static pmOptions opts = {
    .short_options = "dD:I:Prs?",
    .long_options = longopts,
    .short_usage = "[-Prs] [-Dname[=value] ...] [-I dir ...] [infile]",
};

/*
 * iname from ?include ?iname?
 * oname is defined to the path of the matching file, if any.
 * on failure, return NULL
 */
static FILE *
do_include(char *iname, char **oname)
{
    int		i;
    static char	tmpbuf[MAXPATHLEN];
    FILE	*f;
    
    if ((f = openfile(iname)) != NULL) {
	*oname = iname;
	return f;
    }
    if (iname[0] == sep)
	/* absolute pathname */
	return NULL;

    for (i = 0; i < nincdir; i++) {
	if (incdir[i].dirname == NULL)
	    continue;
	snprintf(tmpbuf, sizeof(tmpbuf), "%s%c%s", incdir[i].dirname, sep, iname);
	if (debug)
	    printf("<<include \"%s\"?\n", tmpbuf);
	if ((f = openfile(tmpbuf)) != NULL) {
	    *oname = tmpbuf;
	    return f;
	}
    }

    return NULL;
}

static FILE *
do_shell(const char *cmd)
{
    FILE	*f;

    f = popen(cmd, "r");

    return f;
}

int
main(int argc, char **argv)
{
    int		c;
    int		skip_if_false = 0;
    int		incomment = 0;
    char	*ip;

    currfile = &file_ctl[0];
    sep = __pmPathSeparator();

    /* input buffer starts at 256 chars, but will expand if needed */
    ibuflen = 256;
    if ((ibuf = (char *)malloc(ibuflen)) == NULL) {
	__pmNoMem("pmcpp: ibuf alloc", ibuflen, PM_FATAL_ERR);
	/*NOTREACHED*/
    }

    /*
     * incdir[0] is for directory of filename from command line
     * (if given) and will be filled in later
     * incdir[...] is for directory of filename from -I args
     * incdir[nincdir-1] is directory $PCP_VAR_DIR/pmns
     */
    nincdir = 2;
    incdir = (incdir_t *)malloc(nincdir*sizeof(incdir_t));
    if (incdir == NULL) {
	__pmNoMem("pmcpp: incdir[]", nincdir*sizeof(incdir_t), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    incdir[0].dirname = NULL;
    incdir[1].dirname = NULL;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'd':	/* debug */
	    debug = 1;
	    break;

	case 'D':	/* define */
	    for (ip = opts.optarg; *ip; ip++) {
		if (*ip == '=') {
		    *ip = ' ';
		    break;
		}
	    }
	    while (strlen(opts.optarg)+9 >= ibuflen) {
		ibuflen *= 2;
		if ((ibuf = (char *)realloc(ibuf, ibuflen)) == NULL) {
		    __pmNoMem("pmcpp: ibuf -D realloc", ibuflen, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		if (debug)
		    printf("<<-D doubled ibuf[] to %d chars\n", ibuflen);
	    }
	    snprintf(ibuf, ibuflen, "#define %s\n", opts.optarg);
	    currfile->fname = "<arg>";
	    currfile->lineno = opts.optind;
	    directive();
	    break;

	case 'I':	/* extra include search directory */
	    nincdir++;
	    incdir = (incdir_t *)realloc(incdir, nincdir*sizeof(incdir_t));
	    if (incdir == NULL) {
		__pmNoMem("pmcpp: realloc incdir[]", nincdir*sizeof(incdir_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    incdir[nincdir-2].dirname = opts.optarg;
	    incdir[nincdir-1].dirname = NULL;
	    break;

	case 'P':	/* no # lineno "filename" linemarker output lines */
	    Pflag = 1;
	    break;

	case 'r':	/* restrict macro expansion to #name or #{name} or */
			/* with -s, %name or %{name} */
	   rflag = 1;
	   break;

	case 's':	/* input text style is shell, not C */
	   style = STYLE_SH;
	   ctl = '%';
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
	/* input from a file */
	static char	*dirbuf;
	currfile->fname = argv[opts.optind];
	currfile->fin = openfile(currfile->fname);
	if (currfile->fin == NULL) {
	    err((char *)pmErrStr(-oserror()));
	    /*NOTREACHED*/
	}
	/*
	 * some versions of dirname() clobber the input argument,
	 * some do not ... hence the obscurity here
	 */
	dirbuf = strdup(currfile->fname);
	if (dirbuf == NULL) {
	    __pmNoMem("pmcpp: dir name alloc", strlen(currfile->fname)+1, PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	incdir[0].dirname = dirname(dirbuf);
    }

    /*
     * Fill in last incdir option ... $PCP_VAR_DIR/pmns ...
     * this is for backwards compatibility with the earlier version
     * of pmcpp and the use from pmLoadASCIINameSpace()
     */
    {
	static char	tmpbuf[MAXPATHLEN];
	snprintf(tmpbuf, sizeof(tmpbuf), "%s%cpmns", pmGetConfig("PCP_VAR_DIR"), sep);
	incdir[nincdir-1].dirname = tmpbuf;
    }

    if (!Pflag) {
	printf("# %d \"%s\"\n", currfile->lineno+1, currfile->fname);
	nline_out++;
    }

    for ( ; ; ) {
	ip = ibuf;
more:
	if (fgets(ip, ibuflen-(ip-ibuf), currfile->fin) == NULL) {
	    if (strcmp(currfile->fname, "<shell>") == 0) {
		int	sts;
		sts = pclose(currfile->fin);
#if defined(HAVE_SYS_WAIT_H)
		if (WIFEXITED(sts)) {
		    if (WEXITSTATUS(sts) != 0)
			fprintf(stderr, "Warning: %cshell cmd returns %d exit status\n", ctl, WEXITSTATUS(sts));
		}
		else if (WIFSIGNALED(sts)) {
		    fprintf(stderr, "Warning: %cshell cmd terminated by signal %d\n", ctl, WTERMSIG(sts));
		}
#else
		if (sts != 0)
		    fprintf(stderr, "Warning:  %cshell cmd popen() returns: %d\n", ctl, sts);
#endif
	    }
	    else
		fclose(currfile->fin);
	    if (currfile == &file_ctl[0])
		break;
	    /* all but the first stream have a strdup'd fname */
	    free(currfile->fname);
	    currfile--;
	    if (!Pflag) {
		printf("# %d \"%s\"\n", currfile->lineno+1, currfile->fname);
		nline_out++;
	    }
	    continue;
	}
	/* need to make sure we have a complete line of input */
	while (*ip != '\0')
	    ip++;
	if (ip[-1] != '\n') {
	    /*
	     * input line longer than ibuf[] ... expand buffer and
	     * go read some more
	     */
	    int	sofar = ip - ibuf;
	    ibuflen *= 2;
	    if ((ibuf = (char *)realloc(ibuf, ibuflen)) == NULL) {
		__pmNoMem("pmcpp: ibuf realloc", ibuflen, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    if (debug)
		printf("<<doubled ibuf[] to %d chars\n", ibuflen);
	    ip = &ibuf[sofar];
	    goto more;
	}
	inlen = ip - ibuf + 1;
	nline_in++;
	currfile->lineno++;
 
	if (style == STYLE_C) {
	    /* strip comments ... */
	    int		blanked = 0;
	    for (ip = ibuf; *ip != '\n' ; ip++) {
		if (incomment > 0) {
		    blanked = 1;
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
			blanked = 1;
			*ip++ = ' ';
			*ip = ' ';
		    }
		}
	    }
	    if (blanked) {
		/*
		 * found and stripped one or more comments or parts
		 * thereof ... strip any trailing white space (comment
		 * text was replaced by ' ' above)
		 */
		ip--;	/* currently at ' ' or '\n' */
		while (ip >= ibuf && isspace((int)*ip))
		    ip--;
		*++ip = '\n';
		*++ip = '\0';
	    }
	}
	if (incomment && ibuf[0] == '\n') {
	    printf("\n");
	    nline_out++;
	    continue;
	}

	if (ibuf[0] == ctl) {
	    /* pmcpp control line */
	    if (strncmp(&ibuf[1], "include", strlen("include")) == 0) {
		char		*p;
		char		*q;
		char		*pend;
		char		c;
		FILE		*f;

		if (skip_if_false) {
		    printf("\n");
		    nline_out++;
		    continue;
		}
		p = &ibuf[strlen("?include")];
		while (*p && isblank((int)*p)) p++;
		if (*p != '"' && *p != '<') {
		    err("Expected \" or < after %cinclude", ctl);
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
		    err("%cinclude and/or %cshell nesting too deep", ctl, ctl);
		    /*NOTREACHED*/
		}
		for (q = &pend[1]; isspace((int)*q); q++) ;
		if (*q != '\n' && *q != '\0') {
		    err("Unexpected extra text in %cinclude line", ctl);
		    /*NOTREACHED*/
		}
		c = *pend;
		*pend = '\0';
		f = do_include(p, &p);
		if (f == NULL) {
		    *pend = c;
		    err("Cannot open file for %cinclude", ctl);
		    /*NOTREACHED*/
		}
		currfile++;
		currfile->lineno = 0;
		currfile->fin = f;
		currfile->fname = strdup(p);
		*pend = c;
		if (currfile->fname == NULL) {
		    __pmNoMem("pmcpp: include file name alloc", strlen(p)+1, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		if (!Pflag) {
		    printf("# %d \"%s\"\n", currfile->lineno+1, currfile->fname);
		    nline_out++;
		}
	    }
	    else if (strncmp(&ibuf[1], "shell", strlen("shell")) == 0) {
		char		*p;
		char		*q;
		char		*pend;
		char		c;
		FILE		*f;

		if (skip_if_false) {
		    printf("\n");
		    nline_out++;
		    continue;
		}
		p = &ibuf[strlen("?shell")];
		while (*p && isblank((int)*p)) p++;
		if (*p != '"' && *p != '\'') {
		    err("Expected \" or ' after %cshell", ctl);
		    /*NOTREACHED*/
		}
		pend = ++p;
		while (*pend && *pend != '\n' &&
		       ((p[-1] != '"' || *pend != '"') &&
		        (p[-1] != '\'' || *pend != '\''))) pend++;
		if (p[-1] == '"' && *pend != '"') {
		    err("Expected \" after command");
		    /*NOTREACHED*/
		}
		if (p[-1] == '\'' && *pend != '\'') {
		    err("Expected ' after command");
		    /*NOTREACHED*/
		}
		if (currfile == &file_ctl[MAXLEVEL-1]) {
		    err("%cshell and/or %cinclude nesting too deep", ctl, ctl);
		    /*NOTREACHED*/
		}
		for (q = &pend[1]; isspace((int)*q); q++) ;
		if (*q != '\n' && *q != '\0') {
		    err("Unexpected extra text in %cshell line", ctl);
		    /*NOTREACHED*/
		}
		c = *pend;
		*pend = '\0';
		f = do_shell(p);
		if (f == NULL) {
		    /*
		     * this almost never happens ... if the shell cmd
		     * is bad we still get a valid FILE *, but an error
		     * message from sh(1) goes to stderr and pclose()
		     * will return badness
		     */
		    *pend = c;
		    err("Cannot open pipe for %cshell", ctl);
		    /*NOTREACHED*/
		}
		currfile++;
		currfile->lineno = 0;
		currfile->fin = f;
		currfile->fname = strdup("<shell>");
		*pend = c;
		if (currfile->fname == NULL) {
		    __pmNoMem("pmcpp: shell file name alloc", strlen(p)+1, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		if (!Pflag) {
		    printf("# %d \"%s\"\n", currfile->lineno+1, currfile->fname);
		    nline_out++;
		}
	    }
	    else {
		/* expect other pmcpp control ... */
		skip_if_false = directive();
		if (skip_if_false == -1) {
		    /*
		     * could be a macro expansion request, e.g. #foo
		     * or #{foo} or * %foo or %{foo} ... or just a
		     * random line beginning with a control character
		     * ... charge on
		     */
		    skip_if_false = 0;
		    goto process;
		}
		printf("\n");
		nline_out++;
	    }
	    continue;
	}
	if (skip_if_false) {
	    /* within an if-block that is false */
	    printf("\n");
	    nline_out++;
	}
	else {
process:
	    if (nmacro > 0)
		do_macro();
	    printf("%s", ibuf);
	    nline_out++;
	}
    }

    /* EOF for the top level file */
    if (incomment) {
	currfile->lineno = 0;
	err("Comment at line %d not terminated before end of file", incomment);
	exit(1);
    }

    if (in_if != IF_NONE) {
	currfile->lineno = 0;
	err("End of input and no matching %cendif for %cifdef or %cifndef at line %d", ctl, ctl, ctl, if_lineno);
	exit(1);
    }

    if (debug)
	printf("<<lines: in %d out %d (modified %d) substitutions: %d\n", nline_in, nline_out, nline_sub, nsub);

    exit(0);
}
