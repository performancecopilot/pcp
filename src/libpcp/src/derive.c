/*
 * Copyright (c) 2009,2014-2016 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2016 Red Hat.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * Debug Flags
 *	DERIVE - high-level diagnostics
 *	DERIVE & APPL0 - configuration and static syntax analysis
 *	DERIVE & APPL1 - expression binding, semantic analysis, PMNS ops
 *	DERIVE & APPL2 - fetch handling
 *
 * Caveats
 * 0.	No unary negation operator.
 * 1.	No derived metrics for pmFetchArchive() as this routine does
 *	not use a target pmidlist[]
 * 2.	Derived metrics will not work with pmRequestTraversePMNS() and
 *	pmReceiveTraversePMNS() because the by the time the list of
 *	names is received, the original name at the root of the search
 *	is no longer available.
 * 3.	pmRegisterDerived() does not apply retrospectively to any open
 * 	contexts, so the normal use would be to make all calls to
 * 	pmRegisterDerived() (possibly via pmLoadDerivedConfig()) and then
 * 	call pmNewContext().
 * 4.	There is no pmUnregisterDerived(), so once registered a derived
 * 	metric persists for the life of the application.
 *
 * Thread-safe notes
 *
 * Need to call PM_INIT_LOCKS() in pmRegisterDerived() because we may
 * be called before a context has been created, and missed the
 * lock initialization in pmNewContext().
 *
 * registered.mutex is held throughout pmRegisterDerived() and this
 * protects all of the lexical scanner and parser state, i.e. tokbuf,
 * tokbuflen, string, lexpeek and this.  Same applies to pmid within
 * pmRegisterDerived().
 *
 * The return value from pmRegisterDerived is either a NULL or a pointer
 * back into the expr argument, so use of "this" to carry the return
 * value in the error case is OK.
 *
 * All access to registered is controlled by the registered.mutex.
 *
 * No locking needed in __dminit() to protect need_init and the getenv()
 * call, as we always lock the registered.mutex before calling __dminit().
 *
 * The context locking protocol ensures that when any of the routines
 * below are called with a __pmContext * argument, that argument is
 * not NULL and is associated with a context that is ALREADY locked
 * via ctxp->c_lock.  We should not unlock the context, that is the
 * responsibility of our callers.
 *
 * derive_errmsg needs to be thread-private
 */

#include <inttypes.h>
#include <assert.h>
#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"
#include "fault.h"
#include <sys/stat.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef IS_MINGW
extern const char *strerror_r(int, char *, size_t);
#endif

static int __dminit_configfile(const char *);

static int		need_init = 1;
static ctl_t		registered = {
#ifdef PM_MULTI_THREAD
#ifdef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
#else
    PTHREAD_MUTEX_INITIALIZER,
#endif
#endif
	0, NULL, 0, 0 };

/* parser and lexer variables */
static char		*tokbuf;
static int		tokbuflen;
static const char	*this;		/* start of current lexicon */
static int		lexpeek;
static const char	*string;

#ifdef PM_MULTI_THREAD
#ifdef HAVE___THREAD
/* using a gcc construct here to make derive_errmsg thread-private */
static __thread char	*derive_errmsg;
#endif
#else
static char		*derive_errmsg;
#endif

static const char	*type_dbg[] = {
	"ERROR", "EOF", "UNDEF", "NUMBER", "NAME", "PLUS", "MINUS",
	"STAR", "SLASH", "LPAREN", "RPAREN", "AVG", "COUNT", "DELTA",
	"MAX", "MIN", "SUM", "ANON", "RATE", "INSTANT" };
static const char	type_c[] = {
	'\0', '\0', '\0', '\0', '\0', '+', '-', '*', '/', '(', ')', '\0' };

/* function table for lexer */
static const struct {
    int		f_type;
    char	*f_name;
} func[] = {
    { L_AVG,	"avg" },
    { L_COUNT,	"count" },
    { L_DELTA,	"delta" },
    { L_MAX,	"max" },
    { L_MIN,	"min" },
    { L_SUM,	"sum" },
    { L_ANON,	"anon" },
    { L_RATE,	"rate" },
    { L_INSTANT,"instant" },
    { L_UNDEF,	NULL }
};

/* parser states */
#define P_INIT		0
#define P_LEAF		1
#define P_LEAF_PAREN	2
#define P_BINOP		3
#define P_FUNC_OP	4
#define P_FUNC_END	5
#define P_END		99

static const char	*state_dbg[] = {
	"INIT", "LEAF", "LEAF_PAREN", "BINOP", "FUNC_OP", "FUNC_END" };

#if defined(PM_MULTI_THREAD) && !defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
static void
initialize_mutex(void)
{
    static pthread_mutex_t	init = PTHREAD_MUTEX_INITIALIZER;
    static int			done;
    int				psts;
    char			errmsg[PM_MAXERRMSGLEN];

    if ((psts = pthread_mutex_lock(&init)) != 0) {
	strerror_r(psts, errmsg, sizeof(errmsg));
	fprintf(stderr, "initialize_mutex: pthread_mutex_lock failed: %s", errmsg);
	exit(4);
    }
    if (!done) {
	/*
	 * Unable to initialize at compile time, need to do it here in
	 * a one trip for all threads run-time initialization.
	 */
	pthread_mutexattr_t    attr;

	if ((psts = pthread_mutexattr_init(&attr)) != 0) {
	    strerror_r(psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "initialize_mutex: pthread_mutexattr_init failed: %s", errmsg);
	    exit(4);
	}
	if ((psts = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)) != 0) {
	    strerror_r(psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "initialize_mutex: pthread_mutexattr_settype failed: %s", errmsg);
	    exit(4);
	}
	if ((psts = pthread_mutex_init(&registered.mutex, &attr)) != 0) {
	    strerror_r(psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "initialize_mutex: pthread_mutex_init failed: %s", errmsg);
	    exit(4);
	}
	pthread_mutexattr_destroy(&attr);
	done = 1;
    }
    if ((psts = pthread_mutex_unlock(&init)) != 0) {
	strerror_r(psts, errmsg, sizeof(errmsg));
	fprintf(stderr, "initialize_mutex: pthread_mutex_unlock failed: %s", errmsg);
	exit(4);
    }
}
#else
# define initialize_mutex() do { } while (0)
#endif

/*
 * Handle one component of the ':' separated derived config spec.
 * Used for $PCP_DERIVED_CONFIG evaluation and pmLoadDerivedConfig.
 *
 * If descend is 1 and name is a directory then we will process all
 * the files in that directory, otherwise directories are skipped.
 * Note: for the current implementation, descend is always 1.
 *
 * If recover is 1 then we are tolerant of failures like missing or
 * inaccessible files/directories, otherwise we're not and an error
 * is propagated back to the caller.
 */
static int
__dminit_component(const char *name, int descend, int recover)
{
    struct stat	sbuf;
    int		sts = 0;

    if (stat(name, &sbuf) < 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_DERIVE) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "Warning: derived metrics path component: %s: %s\n",
		name, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	}
#endif
	sts = -oserror();
	goto finish;
    }
    if (S_ISREG(sbuf.st_mode)) {
	/* regular file or symlink to a regular file, load it */
	sts = __dminit_configfile(name);
#ifdef PCP_DEBUG
	if (sts < 0 && pmDebug & DBG_TRACE_DERIVE) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "pmLoadDerivedConfig(%s): %s\n", name, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
#endif
	goto finish;
    }
    if (descend && S_ISDIR(sbuf.st_mode)) {
	/* directory, descend to process all files in the directory */
	DIR		*dirp;
	struct dirent	*dp;

	if ((dirp = opendir(name)) == NULL) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_DERIVE) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "Warning: derived metrics path directory component: %s: %s\n",
		    name, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	    }
#endif
	    sts = -oserror();
	    goto finish;
	}
	while (setoserror(0), (dp = readdir(dirp)) != NULL) {
	    char	path[MAXPATHLEN+1];
	    int		localsts;
	    /*
	     * skip "." and ".." and recursively call __dminit_component()
	     * to process the directory entries ... descend is passed down
	     */
	    if (strcmp(dp->d_name, ".") == 0) continue;
	    if (strcmp(dp->d_name, "..") == 0) continue;
	    snprintf(path, sizeof(path), "%s%c%s", name, __pmPathSeparator(), dp->d_name);
	    if ((localsts = __dminit_component(path, descend, recover)) < 0) {
		sts = localsts;
		goto finish;
	    }
	    sts += localsts;
	}
#ifdef PCP_DEBUG
	/* error is most unlikely and ignore unless -Dderive specified */
	if (oserror() != 0 && pmDebug & DBG_TRACE_DERIVE) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "Warning: %s: readdir failed: %s\n",
		name, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	}
#endif
	closedir(dirp);
	goto finish;
    }
    /* otherwise not a file or symlink to a real file or a directory */
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DERIVE) {
	fprintf(stderr, "Warning: derived metrics path component: %s: unexpected st_mode=%o?\n",
	    name, sbuf.st_mode);
    }
#endif

finish:
    return recover ? 0 : sts;
}

/*
 * Parse, split and process ':' separated components from a
 * derived metrics path specification.
 */
static int
__dminit_parse(const char *path, int recover)
{
    const char	*p = path;
    const char	*q;
    int		sts = 0;
    int		lsts;

    while ((q = index(p, ':')) != NULL) {
	char	*name = strndup(p, q-p+1);
	name[q-p] = '\0';
	lsts = __dminit_component(name, 1, recover);
	if (lsts < 0)
	    return lsts;
	sts += lsts;
	free(name);
	p = q+1;
    }
    if (*p != '\0') {
	lsts = __dminit_component(p, 1, recover);
	if (lsts < 0)
	    return lsts;
	sts += lsts;
    }
    return sts;
}

/*
 * Initialization for Derived Metrics (and Anonymous Metrics for event
 * records) ...
 */
static void
__dminit(void)
{
    /*
     * no derived metrics for PMCD or PMDAs
     */
    if (need_init && __pmGetInternalState() == PM_STATE_PMCS)
	need_init = 0;

    if (need_init) {
	char	*configpath;
	int	sts;
	char	global[MAXPATHLEN+1];

	/* anon metrics for event record unpacking */
PM_FAULT_POINT("libpcp/" __FILE__ ":7", PM_FAULT_PMAPI);
	sts = __pmRegisterAnon("event.flags", PM_TYPE_U32);
	if (sts < 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s: Warning: failed to register event.flags: %s\n",
		    pmProgname, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
PM_FAULT_POINT("libpcp/" __FILE__ ":8", PM_FAULT_PMAPI);
	sts = __pmRegisterAnon("event.missed", PM_TYPE_U32);
	if (sts < 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s: Warning: failed to register event.missed: %s\n",
		    pmProgname, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}

	/*
	 * If PCP_DERIVED_CONFIG is NOT set, then by default we load global
	 * derived configs from the directory $PCP_VAR_DIR/config/derived.
	 *
	 * If PCP_DERIVED_CONFIG is set to a zero length string, then don't
	 * load any derived metrics definitions.
	 *
	 * Else if PCP_DERIVED_CONFIG is set then load user-defined derived
	 * metrics from one or more files or directories separated by ':'.
	 *
	 */
	if ((configpath = getenv("PCP_DERIVED_CONFIG")) == NULL) {
	    snprintf(global, sizeof(global), "%s/config/derived", pmGetConfig("PCP_VAR_DIR"));
	    if (access(global, F_OK) == 0)
		configpath = global;
	}
	if (configpath && strnlen(configpath, MAXPATHLEN) > 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_DERIVE) {
		fprintf(stderr, "Derived metric initialization from %s\n",
		    configpath == global ? global : "$PCP_DERIVED_CONFIG");
	    }
#endif
	    __dminit_parse(configpath, 1 /*recovering*/);
	}
	need_init = 0;
    }
}

static void
unget(int c)
{
    lexpeek = c;
}

static int
get()
{
    int		c;
    if (lexpeek != 0) {
	c = lexpeek;
	lexpeek = 0;
	return c;
    }
    c = *string;
    if (c == '\0') {
	return L_EOF;
    }
    string++;
    return c;
}

static int
lex(void)
{
    int		c;
    char	*p = tokbuf;
    int		ltype = L_UNDEF;
    int		i;
    int		firstch = 1;

    for ( ; ; ) {
	c = get();
	if (firstch) {
	    if (isspace((int)c)) continue;
	    this = &string[-1];
	    firstch = 0;
	}
	if (c == L_EOF) {
	    if (ltype != L_UNDEF) {
		/* force end of last token */
		c = 0;
	    }
	    else {
		/* really the end of the input */
		return L_EOF;
	    }
	}
	if (p == NULL) {
	    tokbuflen = 128;
	    if ((p = tokbuf = (char *)malloc(tokbuflen)) == NULL) {
		PM_UNLOCK(registered.mutex);
		__pmNoMem("pmRegisterDerived: alloc tokbuf", tokbuflen, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	}
	else if (p >= &tokbuf[tokbuflen]) {
	    int		x = p - tokbuf;
	    tokbuflen *= 2;
	    if ((tokbuf = (char *)realloc(tokbuf, tokbuflen)) == NULL) {
		PM_UNLOCK(registered.mutex);
		__pmNoMem("pmRegisterDerived: realloc tokbuf", tokbuflen, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    p = &tokbuf[x];
	}

	*p++ = (char)c;

	if (ltype == L_UNDEF) {
	    if (isdigit((int)c))
		ltype = L_NUMBER;
	    else if (isalpha((int)c))
		ltype = L_NAME;
	    else {
		switch (c) {
		    case '+':
			*p = '\0';
			return L_PLUS;
			break;

		    case '-':
			*p = '\0';
			return L_MINUS;
			break;

		    case '*':
			*p = '\0';
			return L_STAR;
			break;

		    case '/':
			*p = '\0';
			return L_SLASH;
			break;

		    case '(':
			*p = '\0';
			return L_LPAREN;
			break;

		    case ')':
			*p = '\0';
			return L_RPAREN;
			break;

		    default:
			return L_ERROR;
			break;
		}
	    }
	}
	else {
	    if (ltype == L_NUMBER) {
		if (!isdigit((int)c)) {
		    unget(c);
		    p[-1] = '\0';
		    return L_NUMBER;
		}
	    }
	    else if (ltype == L_NAME) {
		if (isalpha((int)c) || isdigit((int)c) || c == '_' || c == '.')
		    continue;
		if (c == '(') {
		    /* check for functions ... */
		    int		namelen = p - tokbuf - 1;
		    for (i = 0; func[i].f_name != NULL; i++) {
			if (namelen == strlen(func[i].f_name) &&
			    strncmp(tokbuf, func[i].f_name, namelen) == 0) {
			    *p = '\0';
			    return func[i].f_type;
			}
		    }
		}
		/* current character is end of name */
		unget(c);
		p[-1] = '\0';
		return L_NAME;
	    }
	}

    }
}

static node_t *
newnode(int type)
{
    node_t	*np;
    np = (node_t *)malloc(sizeof(node_t));
    if (np == NULL) {
	PM_UNLOCK(registered.mutex);
	__pmNoMem("pmRegisterDerived: newnode", sizeof(node_t), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    np->type = type;
    np->save_last = 0;
    np->left = NULL;
    np->right = NULL;
    np->value = NULL;
    np->info = NULL;
    return np;
}

static void
free_expr(node_t *np)
{
    if (np == NULL) return;
    if (np->left != NULL) free_expr(np->left);
    if (np->right != NULL) free_expr(np->right);
    /* value is only allocated once for the static nodes */
    if (np->info == NULL && np->value != NULL) free(np->value);
    if (np->info != NULL) free(np->info);
    free(np);
}

/*
 * copy a static expression tree to make the dynamic per context
 * expression tree and initialize the info block
 */
static node_t *
bind_expr(int n, node_t *np)
{
    node_t	*new;

    assert(np != NULL);
    new = newnode(np->type);
    if (np->left != NULL) {
	if ((new->left = bind_expr(n, np->left)) == NULL) {
	    /* error, reported deeper in the recursion, clean up */
	    free(new);
	    return(NULL);
	}
    }
    if (np->right != NULL) {
	if ((new->right = bind_expr(n, np->right)) == NULL) {
	    /* error, reported deeper in the recursion, clean up */
	    free(new);
	    return(NULL);
	}
    }
    if ((new->info = (info_t *)malloc(sizeof(info_t))) == NULL) {
	PM_UNLOCK(registered.mutex);
	__pmNoMem("bind_expr: info block", sizeof(info_t), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    new->info->pmid = PM_ID_NULL;
    new->info->numval = 0;
    new->info->mul_scale = new->info->div_scale = 1;
    new->info->ivlist = NULL;
    new->info->stamp.tv_sec = 0;
    new->info->stamp.tv_usec = 0;
    new->info->time_scale = -1;		/* one-trip initialization if needed */
    new->info->last_numval = 0;
    new->info->last_ivlist = NULL;
    new->info->last_stamp.tv_sec = 0;
    new->info->last_stamp.tv_usec = 0;

    /* need info to be non-null to protect copy of value in free_expr */
    new->value = np->value;

    new->save_last = np->save_last;

    if (new->type == L_NAME) {
	int	sts;

	sts = pmLookupName(1, &new->value, &new->info->pmid);
	if (sts < 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_DERIVE) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "bind_expr: error: derived metric %s: operand: %s: %s\n", registered.mlist[n].name, new->value, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
#endif
	    free(new->info);
	    free(new);
	    return NULL;
	}
	sts = pmLookupDesc(new->info->pmid, &new->desc);
	if (sts < 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_DERIVE) {
		char	strbuf[20];
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "bind_expr: error: derived metric %s: operand (%s [%s]): %s\n", registered.mlist[n].name, new->value, pmIDStr_r(new->info->pmid, strbuf, sizeof(strbuf)), pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
#endif
	    free(new->info);
	    free(new);
	    return NULL;
	}
    }
    else if (new->type == L_NUMBER) {
	new->desc = np->desc;
    }

    return new;
}

static
void report_sem_error(char *name, node_t *np)
{
    pmprintf("Semantic error: derived metric %s: ", name);
    switch (np->type) {
	case L_PLUS:
	case L_MINUS:
	case L_STAR:
	case L_SLASH:
	    if (np->left->type == L_NUMBER || np->left->type == L_NAME)
		pmprintf("%s ", np->left->value);
	    else
		pmprintf("<expr> ");
	    pmprintf("%c ", type_c[np->type+2]);
	    if (np->right->type == L_NUMBER || np->right->type == L_NAME)
		pmprintf("%s", np->right->value);
	    else
		pmprintf("<expr>");
	    break;
	case L_AVG:
	case L_COUNT:
	case L_DELTA:
	case L_RATE:
	case L_INSTANT:
	case L_MAX:
	case L_MIN:
	case L_SUM:
	case L_ANON:
	    pmprintf("%s(%s)", type_dbg[np->type+2], np->left->value);
	    break;
	default:
	    /* should never get here ... */
	    if (np->type+2 >= 0 && np->type+2 < sizeof(type_dbg)/sizeof(type_dbg[0]))
		pmprintf("botch @ node type %s?", type_dbg[np->type+2]);
	    else
		pmprintf("botch @ node type #%d?", np->type);
	    break;
    }
    pmprintf(": %s\n", PM_TPD(derive_errmsg));
    pmflush();
    PM_TPD(derive_errmsg) = NULL;
}

/* type promotion */
static const int promote[6][6] = {
    { PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE },
    { PM_TYPE_U32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE },
    { PM_TYPE_64, PM_TYPE_64, PM_TYPE_64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE },
    { PM_TYPE_U64, PM_TYPE_U64, PM_TYPE_U64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE },
    { PM_TYPE_FLOAT, PM_TYPE_FLOAT, PM_TYPE_FLOAT, PM_TYPE_FLOAT, PM_TYPE_FLOAT, PM_TYPE_DOUBLE },
    { PM_TYPE_DOUBLE, PM_TYPE_DOUBLE, PM_TYPE_DOUBLE, PM_TYPE_DOUBLE, PM_TYPE_DOUBLE, PM_TYPE_DOUBLE }
};

/* time scale conversion factors */
static const int timefactor[] = {
    1000,		/* NSEC -> USEC */
    1000,		/* USEC -> MSEC */
    1000,		/* MSEC -> SEC */
    60,			/* SEC -> MIN */
    60,			/* MIN -> HOUR */
};

/*
 * mapping pmUnits for the result, and refining pmDesc as we go ...
 * we start with the pmDesc from the left operand and adjust as
 * necessary
 *
 * scale conversion rules ...
 * Count - choose larger, divide/multiply smaller by 10^(difference)
 * Time - choose larger, divide/multiply smaller by appropriate scale
 * Space - choose larger, divide/multiply smaller by 1024^(difference)
 * and result is of type PM_TYPE_DOUBLE
 *
 * Need inverted logic to deal with numerator (dimension > 0) and
 * denominator (dimension < 0) cases.
 */
static void
map_units(node_t *np)
{
    pmDesc	*right = &np->right->desc;
    pmDesc	*left = &np->left->desc;
    int		diff;
    int		i;

    if (left->units.dimCount != 0 && right->units.dimCount != 0) {
	diff = left->units.scaleCount - right->units.scaleCount;
	if (diff > 0) {
	    /* use the left scaleCount, scale the right operand */
	    for (i = 0; i < diff; i++) {
		if (right->units.dimCount > 0)
		    np->right->info->div_scale *= 10;
		else
		    np->right->info->mul_scale *= 10;
	    }
	    np->desc.type = PM_TYPE_DOUBLE;
	}
	else if (diff < 0) {
	    /* use the right scaleCount, scale the left operand */
	    np->desc.units.scaleCount = right->units.scaleCount;
	    for (i = diff; i < 0; i++) {
		if (left->units.dimCount > 0)
		    np->left->info->div_scale *= 10;
		else
		    np->left->info->mul_scale *= 10;
	    }
	    np->desc.type = PM_TYPE_DOUBLE;
	}
    }
    if (left->units.dimTime != 0 && right->units.dimTime != 0) {
	diff = left->units.scaleTime - right->units.scaleTime;
	if (diff > 0) {
	    /* use the left scaleTime, scale the right operand */
	    for (i = right->units.scaleTime; i < left->units.scaleTime; i++) {
		if (right->units.dimTime > 0)
		    np->right->info->div_scale *= timefactor[i];
		else
		    np->right->info->mul_scale *= timefactor[i];
	    }
	    np->desc.type = PM_TYPE_DOUBLE;
	}
	else if (diff < 0) {
	    /* use the right scaleTime, scale the left operand */
	    np->desc.units.scaleTime = right->units.scaleTime;
	    for (i = left->units.scaleTime; i < right->units.scaleTime; i++) {
		if (right->units.dimTime > 0)
		    np->left->info->div_scale *= timefactor[i];
		else
		    np->left->info->mul_scale *= timefactor[i];
	    }
	    np->desc.type = PM_TYPE_DOUBLE;
	}
    }
    if (left->units.dimSpace != 0 && right->units.dimSpace != 0) {
	diff = left->units.scaleSpace - right->units.scaleSpace;
	if (diff > 0) {
	    /* use the left scaleSpace, scale the right operand */
	    for (i = 0; i < diff; i++) {
		if (right->units.dimSpace > 0)
		    np->right->info->div_scale *= 1024;
		else
		    np->right->info->mul_scale *= 1024;
	    }
	    np->desc.type = PM_TYPE_DOUBLE;
	}
	else if (diff < 0) {
	    /* use the right scaleSpace, scale the left operand */
	    np->desc.units.scaleSpace = right->units.scaleSpace;
	    for (i = diff; i < 0; i++) {
		if (right->units.dimSpace > 0)
		    np->left->info->div_scale *= 1024;
		else
		    np->left->info->mul_scale *= 1024;
	    }
	    np->desc.type = PM_TYPE_DOUBLE;
	}
    }

    if (np->type == L_STAR) {
	np->desc.units.dimCount = left->units.dimCount + right->units.dimCount;
	np->desc.units.dimTime = left->units.dimTime + right->units.dimTime;
	np->desc.units.dimSpace = left->units.dimSpace + right->units.dimSpace;
    }
    else if (np->type == L_SLASH) {
	np->desc.units.dimCount = left->units.dimCount - right->units.dimCount;
	np->desc.units.dimTime = left->units.dimTime - right->units.dimTime;
	np->desc.units.dimSpace = left->units.dimSpace - right->units.dimSpace;
    }
    
    /*
     * for division and multiplication, dimension may have come from
     * right operand, need to pick up scale from there also
     */
    if (np->desc.units.dimCount != 0 && left->units.dimCount == 0)
	np->desc.units.scaleCount = right->units.scaleCount;
    if (np->desc.units.dimTime != 0 && left->units.dimTime == 0)
	np->desc.units.scaleTime = right->units.scaleTime;
    if (np->desc.units.dimSpace != 0 && left->units.dimSpace == 0)
	np->desc.units.scaleSpace = right->units.scaleSpace;

}

static int
map_desc(int n, node_t *np)
{
    /*
     * pmDesc mapping for binary operators ...
     *
     * semantics		acceptable operators
     * counter, counter		+ -
     * non-counter, non-counter	+ - * /
     * counter, non-counter	* /
     * non-counter, counter	*
     *
     * in the non-counter and non-counter case, the semantics for the
     * result are PM_SEM_INSTANT, unless both operands are
     * PM_SEM_DISCRETE in which case the result is also PM_SEM_DISCRETE
     *
     * type promotion (similar to ANSI C)
     * PM_TYPE_STRING, PM_TYPE_AGGREGATE, PM_TYPE_AGGREGATE_STATIC,
     * PM_TYPE_EVENT and PM_TYPE_HIGHRES_EVENT are illegal operands
     * except for renaming (where no operator is involved)
     * for all operands, division => PM_TYPE_DOUBLE
     * else PM_TYPE_DOUBLE & any type => PM_TYPE_DOUBLE
     * else PM_TYPE_FLOAT & any type => PM_TYPE_FLOAT
     * else PM_TYPE_U64 & any type => PM_TYPE_U64
     * else PM_TYPE_64 & any type => PM_TYPE_64
     * else PM_TYPE_U32 & any type => PM_TYPE_U32
     * else PM_TYPE_32 & any type => PM_TYPE_32
     *
     * units mapping
     * operator			checks
     * +, -			same dimension
     * *, /			if only one is a counter, non-counter must
     *				have pmUnits of "none"
     */
    pmDesc	*right = &np->right->desc;
    pmDesc	*left = &np->left->desc;

    if (left->sem == PM_SEM_COUNTER) {
	if (right->sem == PM_SEM_COUNTER) {
	    if (np->type != L_PLUS && np->type != L_MINUS) {
		PM_TPD(derive_errmsg) = "Illegal operator for counters";
		goto bad;
	    }
	}
	else {
	    if (np->type != L_STAR && np->type != L_SLASH) {
		PM_TPD(derive_errmsg) = "Illegal operator for counter and non-counter";
		goto bad;
	    }
	}
    }
    else {
	if (right->sem == PM_SEM_COUNTER) {
	    if (np->type != L_STAR) {
		PM_TPD(derive_errmsg) = "Illegal operator for non-counter and counter";
		goto bad;
	    }
	}
	else {
	    if (np->type != L_PLUS && np->type != L_MINUS &&
		np->type != L_STAR && np->type != L_SLASH) {
		/*
		 * this is not possible at the present since only
		 * arithmetic operators are supported and all are
		 * acceptable here ... check added for completeness
		 */
		PM_TPD(derive_errmsg) = "Illegal operator for non-counters";
		goto bad;
	    }
	}
    }

    /*
     * Choose candidate descriptor ... prefer metric or expression
     * over constant
     */
    if (np->left->type != L_NUMBER)
	np->desc = *left;	/* struct copy */
    else
	np->desc = *right;	/* struct copy */

    /*
     * most non-counter expressions produce PM_SEM_INSTANT results
     */
    if (left->sem != PM_SEM_COUNTER && right->sem != PM_SEM_COUNTER) {
	if (left->sem == PM_SEM_DISCRETE && right->sem == PM_SEM_DISCRETE)
	    np->desc.sem = PM_SEM_DISCRETE;
	else
	    np->desc.sem = PM_SEM_INSTANT;
    }

    /*
     * type checking and promotion
     */
    switch (left->type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	case PM_TYPE_64:
	case PM_TYPE_U64:
	case PM_TYPE_FLOAT:
	case PM_TYPE_DOUBLE:
	    break;
	default:
	    PM_TPD(derive_errmsg) = "Non-arithmetic type for left operand";
	    goto bad;
    }
    switch (right->type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	case PM_TYPE_64:
	case PM_TYPE_U64:
	case PM_TYPE_FLOAT:
	case PM_TYPE_DOUBLE:
	    break;
	default:
	    PM_TPD(derive_errmsg) = "Non-arithmetic type for right operand";
	    goto bad;
    }
    np->desc.type = promote[left->type][right->type];
    if (np->type == L_SLASH) {
	/* for division result is real number */
	np->desc.type = PM_TYPE_DOUBLE;
    }

    if (np->type == L_PLUS || np->type == L_MINUS) {
	/*
	 * unit dimensions have to be identical
	 */
	if (left->units.dimCount != right->units.dimCount ||
	    left->units.dimTime != right->units.dimTime ||
	    left->units.dimSpace != right->units.dimSpace) {
	    PM_TPD(derive_errmsg) = "Dimensions are not the same";
	    goto bad;
	}
	map_units(np);
    }

    if (np->type == L_STAR || np->type == L_SLASH) {
	/*
	 * if multiply or divide and operands are a counter and a non-counter,
	 * then non-counter needs to be dimensionless
	 */
	if (left->sem == PM_SEM_COUNTER && right->sem != PM_SEM_COUNTER) {
	    if (right->units.dimCount != 0 ||
	        right->units.dimTime != 0 ||
	        right->units.dimSpace != 0) {
		PM_TPD(derive_errmsg) = "Non-counter and not dimensionless for right operand";
		goto bad;
	    }
	}
	if (left->sem != PM_SEM_COUNTER && right->sem == PM_SEM_COUNTER) {
	    if (left->units.dimCount != 0 ||
	        left->units.dimTime != 0 ||
	        left->units.dimSpace != 0) {
		PM_TPD(derive_errmsg) = "Non-counter and not dimensionless for left operand";
		goto bad;
	    }
	}
	map_units(np);
    }

    /*
     * if not both singular, then both operands must have the same
     * instance domain
     */
    if (left->indom != PM_INDOM_NULL && right->indom != PM_INDOM_NULL && left->indom != right->indom) {
	PM_TPD(derive_errmsg) = "Operands should have the same instance domain";
	goto bad;
    }

    return 0;

bad:
    report_sem_error(registered.mlist[n].name, np);
    return -1;
}

static int
check_expr(int n, node_t *np)
{
    int		sts;

    assert(np != NULL);

    if (np->type == L_NUMBER || np->type == L_NAME)
	return 0;

    /* otherwise, np->left is never NULL ... */
    assert(np->left != NULL);

    if ((sts = check_expr(n, np->left)) < 0)
	return sts;
    if (np->right != NULL) {
	if ((sts = check_expr(n, np->right)) < 0)
	    return sts;
	/* build pmDesc from pmDesc of both operands */
	if ((sts = map_desc(n, np)) < 0)
	    return sts;
    }
    else {
	np->desc = np->left->desc;	/* struct copy */
	/*
	 * special cases for functions ...
	 * delta()	expect numeric operand, result is instantaneous
	 * rate()	expect numeric operand, dimension of time must be
	 * 		0 or 1, result is instantaneous
	 * instant()	result is instantaneous
	 * aggr funcs	most expect numeric operand, result is instantaneous
	 *		and singular
	 */
	if (np->type == L_AVG || np->type == L_COUNT
	    || np->type == L_DELTA 
	    || np->type == L_RATE || np->type == L_INSTANT
	    || np->type == L_MAX || np->type == L_MIN || np->type == L_SUM) {
	    if (np->type == L_COUNT) {
		/* count() has its own type and units */
		np->desc.type = PM_TYPE_U32;
		memset((void *)&np->desc.units, 0, sizeof(np->desc.units));
		np->desc.units.dimCount = 1;
		np->desc.units.scaleCount = PM_COUNT_ONE;
		np->desc.sem = PM_SEM_INSTANT;
	    }
	    else if (np->type == L_INSTANT) {
		/*
		 * semantics are INSTANT if operand is COUNTER, else
		 * inherit the semantics of the operand
		 */
		if (np->left->desc.sem == PM_SEM_COUNTER)
		    np->desc.sem = PM_SEM_INSTANT;
		else
		    np->desc.sem = np->left->desc.sem;
	    }
	    else {
		/* others inherit, but need arithmetic operand */
		switch (np->left->desc.type) {
		    case PM_TYPE_32:
		    case PM_TYPE_U32:
		    case PM_TYPE_64:
		    case PM_TYPE_U64:
		    case PM_TYPE_FLOAT:
		    case PM_TYPE_DOUBLE:
			break;
		    default:
			PM_TPD(derive_errmsg) = "Non-arithmetic operand for function";
			report_sem_error(registered.mlist[n].name, np);
			return -1;
		}
		np->desc.sem = PM_SEM_INSTANT;
	    }
	    if (np->type == L_DELTA || np->type == L_RATE || np->type == L_INSTANT) {
		/* inherit indom */
		if (np->type == L_RATE) {
		    /*
		     * further restriction for rate() that dimension
		     * for time must be 0 (->counter/sec) or 1
		     * (->time utilization)
		     */
		    if (np->left->desc.units.dimTime != 0 && np->left->desc.units.dimTime != 1) {
			PM_TPD(derive_errmsg) = "Incorrect time dimension for operand";
			report_sem_error(registered.mlist[n].name, np);
			return -1;
		    }
		}
	    }
	    else {
		/* all the others are aggregate funcs with a singular value */
		np->desc.indom = PM_INDOM_NULL;
	    }
	    if (np->type == L_AVG) {
		/* avg() returns float result */
		np->desc.type = PM_TYPE_FLOAT;
	    }
	    if (np->type == L_RATE) {
		/* rate() returns double result and time dimension is
		 * reduced by one ... if time dimension is then 0, set
		 * the scale to be none (this is time utilization)
		 */
		np->desc.type = PM_TYPE_DOUBLE;
		np->desc.units.dimTime--;
		if (np->desc.units.dimTime == 0)
		    np->desc.units.scaleTime = 0;
		else
		    np->desc.units.scaleTime = PM_TIME_SEC;
	    }
	}
	else if (np->type == L_ANON) {
	    /* do nothing, pmDesc inherited "as is" from left node */
	    ;
	}
    }
    return 0;
}

static void
dump_value(int type, pmAtomValue *avp)
{
    switch (type) {
	case PM_TYPE_32:
	    fprintf(stderr, "%i", avp->l);
	    break;

	case PM_TYPE_U32:
	    fprintf(stderr, "%u", avp->ul);
	    break;

	case PM_TYPE_64:
	    fprintf(stderr, "%" PRId64, avp->ll);
	    break;

	case PM_TYPE_U64:
	    fprintf(stderr, "%" PRIu64, avp->ull);
	    break;

	case PM_TYPE_FLOAT:
	    fprintf(stderr, "%g", (double)avp->f);
	    break;

	case PM_TYPE_DOUBLE:
	    fprintf(stderr, "%g", avp->d);
	    break;

	case PM_TYPE_STRING:
	    fprintf(stderr, "%s", avp->cp);
	    break;

	case PM_TYPE_AGGREGATE:
	case PM_TYPE_AGGREGATE_STATIC:
	case PM_TYPE_EVENT:
	case PM_TYPE_HIGHRES_EVENT:
	case PM_TYPE_UNKNOWN:
	    fprintf(stderr, "[blob]");
	    break;

	case PM_TYPE_NOSUPPORT:
	    fprintf(stderr, "dump_value: bogus value, metric Not Supported\n");
	    break;

	default:
	    fprintf(stderr, "dump_value: unknown value type=%d\n", type);
    }
}

void
__dmdumpexpr(node_t *np, int level)
{
    char	strbuf[20];

    if (level == 0) fprintf(stderr, "Derived metric expr dump from " PRINTF_P_PFX "%p...\n", np);
    if (np == NULL) return;
    fprintf(stderr, "expr node " PRINTF_P_PFX "%p type=%s left=" PRINTF_P_PFX "%p right=" PRINTF_P_PFX "%p save_last=%d", np, type_dbg[np->type+2], np->left, np->right, np->save_last);
    if (np->type == L_NAME || np->type == L_NUMBER)
	fprintf(stderr, " [%s] master=%d", np->value, np->info == NULL ? 1 : 0);
    fputc('\n', stderr);
    if (np->info) {
	fprintf(stderr, "    PMID: %s ", pmIDStr_r(np->info->pmid, strbuf, sizeof(strbuf)));
	fprintf(stderr, "(%s from pmDesc) numval: %d", pmIDStr_r(np->desc.pmid, strbuf, sizeof(strbuf)), np->info->numval);
	if (np->info->div_scale != 1)
	    fprintf(stderr, " div_scale: %d", np->info->div_scale);
	if (np->info->mul_scale != 1)
	    fprintf(stderr, " mul_scale: %d", np->info->mul_scale);
	fputc('\n', stderr);
	__pmPrintDesc(stderr, &np->desc);
	if (np->info->ivlist) {
	    int		j;
	    int		max;

	    max = np->info->numval > np->info->last_numval ? np->info->numval : np->info->last_numval;

	    for (j = 0; j < max; j++) {
		fprintf(stderr, "[%d]", j);
		if (j < np->info->numval) {
		    fprintf(stderr, " inst=%d, val=", np->info->ivlist[j].inst);
		    dump_value(np->desc.type, &np->info->ivlist[j].value);
		}
		if (j < np->info->last_numval) {
		    fprintf(stderr, " (last inst=%d, val=", np->info->last_ivlist[j].inst);
		    dump_value(np->desc.type, &np->info->last_ivlist[j].value);
		    fputc(')', stderr);
		}
		fputc('\n', stderr);
	    }
	}
    }
    if (np->left != NULL) __dmdumpexpr(np->left, level+1);
    if (np->right != NULL) __dmdumpexpr(np->right, level+1);
}

/*
 * Parser FSA
 * state	lex		new state
 * P_INIT	L_NAME or	P_LEAF
 * 		L_NUMBER
 * P_INIT	L_<func>	P_FUNC_OP
 * P_INIT	L_LPAREN	if parse() != NULL then P_LEAF
 * P_LEAF	L_PLUS or	P_BINOP
 * 		L_MINUS or
 * 		L_STAR or
 * 		L_SLASH
 * P_BINOP	L_NAME or	P_LEAF
 * 		L_NUMBER
 * P_BINOP	L_LPAREN	if parse() != NULL then P_LEAF
 * P_BINOP	L_<func>	P_FUNC_OP
 * P_LEAF_PAREN	same as P_LEAF, but no precedence rules at next operator
 * P_FUNC_OP	L_NAME		P_FUNC_END
 * P_FUNC_END	L_RPAREN	P_LEAF
 */
static node_t *
parse(int level)
{
    int		state = P_INIT;
    int		type;
    node_t	*expr = NULL;
    node_t	*curr = NULL;
    node_t	*np;

    lexpeek = 0;	/* reset in case of error in previous parse() call */

    for ( ; ; ) {
	type = lex();
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL0)) {
	    fprintf(stderr, "parse(%d) state=P_%s type=L_%s \"%s\"\n", level, state_dbg[state], type_dbg[type+2], type == L_EOF ? "" : tokbuf);
	}
#endif
	/* handle lexicons that terminate the parsing */
	switch (type) {
	    case L_ERROR:
		PM_TPD(derive_errmsg) = "Illegal character";
		free_expr(expr);
		return NULL;
		break;
	    case L_EOF:
		if (level == 1 && (state == P_LEAF || state == P_LEAF_PAREN))
		    return expr;
		PM_TPD(derive_errmsg) = "End of input";
		free_expr(expr);
		return NULL;
		break;
	    case L_RPAREN:
		if (state == P_FUNC_END) {
		    state = P_LEAF;
		    continue;
		}
		if ((level > 1 && state == P_LEAF_PAREN) || state == P_LEAF)
		    return expr;
		PM_TPD(derive_errmsg) = "Unexpected ')'";
		free_expr(expr);
		return NULL;
		break;
	}

	switch (state) {
	    case P_INIT:
		/*
		 * Only come here at the start of parsing an expression.
		 * The assert() is designed to stop Coverity flagging a
		 * memory leak if we should come here after expr and/or
		 * curr have already been assigned values either directly
		 * from calling newnode() or via an assignment to np that
		 * was previously assigned a value from newnode()
		 */
		assert(expr == NULL && curr == NULL);

		if (type == L_NAME || type == L_NUMBER) {
		    expr = curr = newnode(type);
		    if ((curr->value = strdup(tokbuf)) == NULL) {
			PM_UNLOCK(registered.mutex);
			__pmNoMem("pmRegisterDerived: leaf node", strlen(tokbuf)+1, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    if (type == L_NUMBER) {
			char		*endptr;
			__uint64_t	check;
			check = strtoull(tokbuf, &endptr, 10);
			if (*endptr != '\0' || check > 0xffffffffUL) {
			    PM_TPD(derive_errmsg) = "Constant value too large";
			    free_expr(expr);
			    return NULL;
			}
			curr->desc.pmid = PM_ID_NULL;
			curr->desc.type = PM_TYPE_U32;
			curr->desc.indom = PM_INDOM_NULL;
			curr->desc.sem = PM_SEM_DISCRETE;
			memset(&curr->desc.units, 0, sizeof(pmUnits));
		    }
		    state = P_LEAF;
		}
		else if (type == L_LPAREN) {
		    expr = curr = parse(level+1);
		    if (expr == NULL)
			return NULL;
		    state = P_LEAF_PAREN;
		}
		else if (type == L_AVG || type == L_COUNT
			 || type == L_DELTA
			 || type == L_RATE || type == L_INSTANT
		         || type == L_MAX || type == L_MIN || type == L_SUM 
			 || type == L_ANON) {
		    expr = curr = newnode(type);
		    state = P_FUNC_OP;
		}
		else {
		    PM_TPD(derive_errmsg) = "Terminal element or function name or '(' expected";
		    free_expr(expr);
		    return NULL;
		}
		break;

	    case P_LEAF_PAREN:	/* fall through */
	    case P_LEAF:
		if (type == L_PLUS || type == L_MINUS || type == L_STAR || type == L_SLASH) {
		    np = newnode(type);
		    if (state == P_LEAF_PAREN ||
		        curr->type == L_NAME || curr->type == L_NUMBER ||
			curr->type == L_AVG || curr->type == L_COUNT ||
			curr->type == L_DELTA ||
			curr->type == L_RATE || curr->type == L_INSTANT ||
			curr->type == L_MAX || curr->type == L_MIN ||
			curr->type == L_SUM || curr->type == L_ANON ||
		        type == L_PLUS || type == L_MINUS) {
			/*
			 * first operator or equal or lower precedence
			 * make new root of tree and push previous
			 * expr down left descendent branch
			 */
			np->left = curr;
			expr = curr = np;
		    }
		    else {
			/*
			 * push previous right branch down one level
			 */
			np->left = curr->right;
			curr->right = np;
			curr = np;
		    }
		    state = P_BINOP;
		}
		else {
		    free_expr(expr);
		    PM_TPD(derive_errmsg) = "Arithmetic operator expected";
		    return NULL;
		}
		break;

	    case P_BINOP:
		if (type == L_NAME || type == L_NUMBER) {
		    np = newnode(type);
		    if ((np->value = strdup(tokbuf)) == NULL) {
			PM_UNLOCK(registered.mutex);
			__pmNoMem("pmRegisterDerived: leaf node", strlen(tokbuf)+1, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    if (type == L_NUMBER) {
			np->desc.pmid = PM_ID_NULL;
			np->desc.type = PM_TYPE_U32;
			np->desc.indom = PM_INDOM_NULL;
			np->desc.sem = PM_SEM_DISCRETE;
			memset(&np->desc.units, 0, sizeof(pmUnits));
		    }
		    curr->right = np;
		    curr = expr;
		    state = P_LEAF;
		}
		else if (type == L_LPAREN) {
		    np = parse(level+1);
		    if (np == NULL)
			return NULL;
		    curr->right = np;
		    state = P_LEAF_PAREN;
		}
		else if (type == L_AVG || type == L_COUNT
			 || type == L_DELTA
			 || type == L_RATE || type == L_INSTANT
		         || type == L_MAX || type == L_MIN || type == L_SUM
			 || type == L_ANON) {
		    np = newnode(type);
		    curr->right = np;
		    curr = np;
		    state = P_FUNC_OP;
		}
		else {
		    PM_TPD(derive_errmsg) = "Terminal element or function name or '(' expected";
		    free_expr(expr);
		    return NULL;
		}
		break;

	    case P_FUNC_OP:
		if (type == L_NAME) {
		    np = newnode(type);
		    if ((np->value = strdup(tokbuf)) == NULL) {
			PM_UNLOCK(registered.mutex);
			__pmNoMem("pmRegisterDerived: func op node", strlen(tokbuf)+1, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    np->save_last = 1;
		    if (curr->type == L_ANON) {
			/*
			 * anon(PM_TYPE_...) is a special case ... the
			 * argument defines the metric type, the remainder
			 * of the metadata is fixed and there are never any
			 * values available. So we build the pmDesc and then
			 * clobber the "left" node and prevent any attempt to
			 * contact a PMDA for metadata or values
			 */
			if (strcmp(np->value, "PM_TYPE_32") == 0)
			    np->desc.type = PM_TYPE_32;
			else if (strcmp(np->value, "PM_TYPE_U32") == 0)
			    np->desc.type = PM_TYPE_U32;
			else if (strcmp(np->value, "PM_TYPE_64") == 0)
			    np->desc.type = PM_TYPE_64;
			else if (strcmp(np->value, "PM_TYPE_U64") == 0)
			    np->desc.type = PM_TYPE_U64;
			else if (strcmp(np->value, "PM_TYPE_FLOAT") == 0)
			    np->desc.type = PM_TYPE_FLOAT;
			else if (strcmp(np->value, "PM_TYPE_DOUBLE") == 0)
			    np->desc.type = PM_TYPE_DOUBLE;
			else {
			    fprintf(stderr, "Error: type=%s not allowed for anon()\n", np->value);
			    free_expr(np);
			    return NULL;
			}
			np->desc.pmid = PM_ID_NULL;
			np->desc.indom = PM_INDOM_NULL;
			np->desc.sem = PM_SEM_DISCRETE;
			memset((void *)&np->desc.units, 0, sizeof(np->desc.units));
			np->type = L_NUMBER;
		    }
		    curr->left = np;
		    curr = expr;
		    state = P_FUNC_END;
		}
		else {
		    PM_TPD(derive_errmsg) = "Metric name expected";
		    free_expr(expr);
		    return NULL;
		}
		break;

	    case P_FUNC_END:
		PM_TPD(derive_errmsg) = "')' expected";
		free_expr(expr);
		return NULL;

	    default:
		PM_TPD(derive_errmsg) = "Parser botch";
		free_expr(expr);
		return NULL;
	}
    }
}

static int
checkname(char *p)
{
    int	firstch = 1;

    for ( ; *p; p++) {
	if (firstch) {
	    firstch = 0;
	    if (isalpha((int)*p)) continue;
	    return -1;
	}
	else {
	    if (isalpha((int)*p) || isdigit((int)*p) || *p == '_') continue;
	    if (*p == '.') {
		firstch = 1;
		continue;
	    }
	    return -1;
	}
    }
    return 0;
}

static char *
registerderived(const char *name, const char *expr, int isanon)
{
    node_t		*np;
    static __pmID_int	pmid;
    int			i;

    PM_INIT_LOCKS();
    initialize_mutex();
    PM_LOCK(registered.mutex);

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL0)) {
	fprintf(stderr, "pmRegisterDerived: name=\"%s\" expr=\"%s\"\n", name, expr);
    }
#endif

    for (i = 0; i < registered.nmetric; i++) {
	if (strcmp(name, registered.mlist[i].name) == 0) {
	    /* oops, duplicate name ... */
	    PM_TPD(derive_errmsg) = "Duplicate derived metric name";
	    PM_UNLOCK(registered.mutex);
	    return (char *)expr;
	}
    }

    PM_TPD(derive_errmsg) = NULL;
    string = expr;
    np = parse(1);
    if (np == NULL) {
	/* parser error */
	char	*sts = (char *)this;
	PM_UNLOCK(registered.mutex);
	return sts;
    }

    registered.nmetric++;
    registered.mlist = (dm_t *)realloc(registered.mlist, registered.nmetric*sizeof(dm_t));
    if (registered.mlist == NULL) {
	PM_UNLOCK(registered.mutex);
	__pmNoMem("pmRegisterDerived: registered mlist", registered.nmetric*sizeof(dm_t), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    if (registered.nmetric == 1) {
	pmid.flag = 0;
	pmid.domain = DYNAMIC_PMID;
	pmid.cluster = 0;
    }
    registered.mlist[registered.nmetric-1].name = strdup(name);
    registered.mlist[registered.nmetric-1].anon = isanon;
    pmid.item = registered.nmetric;
    registered.mlist[registered.nmetric-1].pmid = *((pmID *)&pmid);
    registered.mlist[registered.nmetric-1].expr = np;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DERIVE) {
	fprintf(stderr, "pmRegisterDerived: register metric[%d] %s = %s\n", registered.nmetric-1, name, expr);
	if (pmDebug & DBG_TRACE_APPL0)
	    __dmdumpexpr(np, 0);
    }
#endif

    PM_UNLOCK(registered.mutex);
    return NULL;
}

/* The original, and still the best. */
char *
pmRegisterDerived(const char *name, const char *expr)
{
    return registerderived(name, expr, 0);
}

/* Variant including error handling. */
int
pmRegisterDerivedMetric(const char *name, const char *expr, char **errmsg)
{
    size_t	length;
    char	*offset;
    char	*error;
    char	*dmsg;

    static const char	fmt[] = \
	"Error: pmRegisterDerivedMetric(\"%s\", ...) syntax error\n%s\n%*s^\n";

    *errmsg = NULL;
    if ((offset = registerderived(name, expr, 0)) == NULL)
	return 0;

    /* failed to register name/expr - build an error string to pass back */
    length = strlen(fmt);
    length += strlen(name);
    length += strlen(expr);
    length += (offset - expr);
    if ((dmsg = PM_TPD(derive_errmsg)) != NULL)
	length += strlen(dmsg) + 2;

    if ((error = malloc(length)) == NULL)
	__pmNoMem("pmRegisterDerivedMetric", length, PM_FATAL_ERR);
    snprintf(error, length, fmt, name, expr, (int)(expr - offset), " ");
    if (dmsg) {
	strcat(error, dmsg);
	strcat(error, "\n");
    }
    error[length-1] = '\0';

    *errmsg = error;
    return -1;
}

/* Register an anonymous metric */
int
__pmRegisterAnon(const char *name, int type)
{
    char	*msg;
    char	buf[21];	/* anon(PM_TYPE_XXXXXX) */

PM_FAULT_CHECK(PM_FAULT_PMAPI);
    switch (type) {
	case PM_TYPE_32:
	    snprintf(buf, sizeof(buf), "anon(PM_TYPE_32)");
	    break;
	case PM_TYPE_U32:
	    snprintf(buf, sizeof(buf), "anon(PM_TYPE_U32)");
	    break;
	case PM_TYPE_64:
	    snprintf(buf, sizeof(buf), "anon(PM_TYPE_64)");
	    break;
	case PM_TYPE_U64:
	    snprintf(buf, sizeof(buf), "anon(PM_TYPE_U64)");
	    break;
	case PM_TYPE_FLOAT:
	    snprintf(buf, sizeof(buf), "anon(PM_TYPE_FLOAT)");
	    break;
	case PM_TYPE_DOUBLE:
	    snprintf(buf, sizeof(buf), "anon(PM_TYPE_DOUBLE)");
	    break;
	default:
	    return PM_ERR_TYPE;
    }
    if ((msg = registerderived(name, buf, 1)) != NULL) {
	pmprintf("__pmRegisterAnon(%s, %d): @ \"%s\" Error: %s\n", name, type, msg, pmDerivedErrStr());
	pmflush();
	return PM_ERR_GENERIC;
    }
    return 0;
}

int
pmLoadDerivedConfig(const char *fname)
{
    PM_INIT_LOCKS();
    initialize_mutex();
    PM_LOCK(registered.mutex);
    __dminit();

    return __dminit_parse(fname, 0 /*non-recovering*/);
}

static int
__dminit_configfile(const char *fname)
{
    FILE	*fp;
    int		buflen;
    char	*buf;
    char	*p;
    int		c;
    int		sts = 0;
    int		eq = -1;
    int		lineno = 1;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DERIVE) {
	fprintf(stderr, "pmLoadDerivedConfig(\"%s\")\n", fname);
    }
#endif

    if ((fp = fopen(fname, "r")) == NULL) {
	return -oserror();
    }
    buflen = 128;
    if ((buf = (char *)malloc(buflen)) == NULL) {
	/* registered.mutex not locked in this case */
	__pmNoMem("pmLoadDerivedConfig: alloc buf", buflen, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    p = buf;
    while ((c = fgetc(fp)) != EOF) {
	if (p == &buf[buflen]) {
	    if ((buf = (char *)realloc(buf, 2*buflen)) == NULL) {
		/* registered.mutex not locked in this case */
		__pmNoMem("pmLoadDerivedConfig: expand buf", 2*buflen, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    p = &buf[buflen];
	    buflen *= 2;
	}
	if (c == '=' && eq == -1) {
	    /*
	     * mark first = in line ... metric name to the left and
	     * expression to the right
	     */
	    eq = p - buf;
	}
	if (c == '\n') {
	    if (p == buf || buf[0] == '#') {
		/* comment or empty line, skip it ... */
		goto next_line;
	    }
	    *p = '\0';
	    if (eq != -1) {
		char	*np;	/* copy of name */
		char	*ep;	/* start of expression */
		char	*q;
		char	*errp;
		buf[eq] = '\0';
		if ((np = strdup(buf)) == NULL) {
		    /* registered.mutex not locked in this case */
		    __pmNoMem("pmLoadDerivedConfig: dupname", strlen(buf), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		/* trim white space from tail of metric name */
		q = &np[eq-1];
		while (q >= np && isspace((int)*q))
		    *q-- = '\0';
		/* trim white space from head of metric name */
		q = np;
		while (*q && isspace((int)*q))
		    q++;
		if (*q == '\0') {
		    buf[eq] = '=';
		    pmprintf("[%s:%d] Error: pmLoadDerivedConfig: derived metric name missing\n%s\n", fname, lineno, buf);
		    pmflush();
		    free(np);
		    goto next_line;
		}
		if (checkname(q) < 0) {
		    pmprintf("[%s:%d] Error: pmLoadDerivedConfig: illegal derived metric name (%s)\n", fname, lineno, q);
		    pmflush();
		    free(np);
		    goto next_line;
		}
		ep = &buf[eq+1];
		while (*ep != '\0' && isspace((int)*ep))
		    ep++;
		if (*ep == '\0') {
		    buf[eq] = '=';
		    pmprintf("[%s:%d] Error: pmLoadDerivedConfig: expression missing\n%s\n", fname, lineno, buf);
		    pmflush();
		    free(np);
		    goto next_line;
		}
		errp = pmRegisterDerived(q, ep);
		if (errp != NULL) {
		    pmprintf("[%s:%d] Error: pmRegisterDerived(%s, ...) syntax error\n", fname, lineno, q);
		    pmprintf("%s\n", &buf[eq+1]);
		    for (q = &buf[eq+1]; *q; q++) {
			if (q == errp) *q = '^';
			else if (!isspace((int)*q)) *q = ' ';
		    }
		    pmprintf("%s\n", &buf[eq+1]);
		    q = pmDerivedErrStr();
		    if (q != NULL) pmprintf("%s\n", q);
		    pmflush();
		}
		else
		    sts++;
		free(np);
	    }
	    else {
		/*
		 * error ... no = in the line, so no derived metric name
		 */
		pmprintf("[%s:%d] Error: pmLoadDerivedConfig: missing ``='' after derived metric name\n%s\n", fname, lineno, buf);
		pmflush();
	    }
next_line:
	    lineno++;
	    p = buf;
	    eq = -1;
	}
	else
	    *p++ = c;
    }
    fclose(fp);
    free(buf);
    return sts;
}

char *
pmDerivedErrStr(void)
{
    PM_INIT_LOCKS();
    return PM_TPD(derive_errmsg);
}

/*
 * callbacks
 */

static ctl_t *
__dmctl(__pmContext *ctxp)
{
    ctl_t *cp;

    if (__pmGetInternalState() == PM_STATE_PMCS) {
	/* no derived metrics below PMCS, not even anon */
    	cp = NULL;
    }
    else if (ctxp == NULL) {
	/*
	 * No context, but we still need to traverse globally registered anon
	 * derived metrics using local pmns (but *only* anon, e.g. event.*).
	 */
    	cp = &registered;
    }
    else {
        /*
         * Else use the per-context control structure. Invalid derived metrics,
         * e.g. with missing operands, have cp->mlist[i].expr == NULL, which we
         * can check to effectively exclude them from the pmns for this context.
         * Note that anon derived metrics are assumed to always be valid, so we
         * can use the per-context control structure *or* the registered global
         * control structure (as above) for anon derived metrics.
         */
	cp = (ctl_t *)ctxp->c_dm;
    }

    return cp;
}

int
__dmtraverse(__pmContext *ctxp, const char *name, char ***namelist)
{
    int		sts = 0;
    int		i;
    char	**list = NULL;
    int		matchlen = strlen(name);
    ctl_t       *cp;
    
    if ((cp = __dmctl(ctxp)) == NULL)
	/* no derived metrics below PMCS, not even anon */
    	return PM_ERR_NAME;

    initialize_mutex();
    PM_LOCK(registered.mutex);
    __dminit();

    for (i = 0; i < cp->nmetric; i++) {
	/* skip invalid derived metrics, e.g. due to missing operands */
	if (!cp->mlist[i].anon) {
	    if (ctxp == NULL || cp->mlist[i].expr == NULL) {
		if (pmDebug & DBG_TRACE_DERIVE) {
		    fprintf(stderr, "__dmtraverse: name=\"%s\", omitting invalid child \"%s\"\n",
		    	name, cp->mlist[i].name);
		}
		continue;
	    }
	}
	/*
	 * prefix match ... if name is "", then all names match
	 */
	if (matchlen == 0 ||
	    (strncmp(name, cp->mlist[i].name, matchlen) == 0 &&
	     (cp->mlist[i].name[matchlen] == '.' ||
	      cp->mlist[i].name[matchlen] == '\0'))) {
	    sts++;
	    if ((list = (char **)realloc(list, sts*sizeof(list[0]))) == NULL) {
		PM_UNLOCK(registered.mutex);
		__pmNoMem("__dmtraverse: list", sts*sizeof(list[0]), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    list[sts-1] = cp->mlist[i].name;
	    if (pmDebug & DBG_TRACE_DERIVE)
	    	fprintf(stderr, "__dmtraverse: name=\"%s\" added \"%s\"\n", name, list[sts-1]);
	}
    }
    *namelist = list;

    PM_UNLOCK(registered.mutex);
    return sts;
}

int
__dmchildren(__pmContext *ctxp, const char *name, char ***offspring, int **statuslist)
{
    int		i;
    int		j;
    char	**children = NULL;
    int		*status = NULL;
    char	**n_children;
    char	*q;
    int		matchlen = strlen(name);
    int		start;
    int		len;
    int		num_chn = 0;
    size_t	need = 0;
    ctl_t       *cp;
    
    if ((cp = __dmctl(ctxp)) == NULL)
	/* no derived metrics below PMCS, not even anon */
    	return PM_ERR_NAME;

    initialize_mutex();
    PM_LOCK(registered.mutex);
    __dminit();

    for (i = 0; i < cp->nmetric; i++) {
	/* skip invalid derived metrics, e.g. due to missing operands */
	if (!cp->mlist[i].anon) {
	    if (ctxp == NULL || cp->mlist[i].expr == NULL) {
		if (pmDebug & DBG_TRACE_DERIVE) {
		    fprintf(stderr, "__dmchildren: name=\"%s\", omitting invalid child \"%s\"\n",
		    	name, cp->mlist[i].name);
		}
		continue;
	    }
	}
	/*
	 * prefix match ... pick off the unique next level names on match
	 */
	if (name[0] == '\0' ||
	    (strncmp(name, cp->mlist[i].name, matchlen) == 0 &&
	     (cp->mlist[i].name[matchlen] == '.' ||
	      cp->mlist[i].name[matchlen] == '\0'))) {
	    if (cp->mlist[i].name[matchlen] == '\0') {
		/*
		 * leaf node
		 * assert is for coverity, name uniqueness means we
		 * should only ever come here after zero passes through
		 * the block below where num_chn is incremented and children[]
		 * and status[] are realloc'd
		 */
		assert(num_chn == 0 && children == NULL && status == NULL);
		PM_UNLOCK(registered.mutex);
		return 0;
	    }
	    start = matchlen > 0 ? matchlen + 1 : 0;
	    for (j = 0; j < num_chn; j++) {
		len = strlen(children[j]);
		if (strncmp(&cp->mlist[i].name[start], children[j], len) == 0 &&
		    cp->mlist[i].name[start+len] == '.')
		    break;
	    }
	    if (j == num_chn) {
		/* first time for this one */
		num_chn++;
		if ((children = (char **)realloc(children, num_chn*sizeof(children[0]))) == NULL) {
		    PM_UNLOCK(registered.mutex);
		    __pmNoMem("__dmchildren: children", num_chn*sizeof(children[0]), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		for (len = 0; cp->mlist[i].name[start+len] != '\0' && cp->mlist[i].name[start+len] != '.'; len++)
		    ;
		if ((children[num_chn-1] = (char *)malloc(len+1)) == NULL) {
		    PM_UNLOCK(registered.mutex);
		    __pmNoMem("__dmchildren: name", len+1, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		strncpy(children[num_chn-1], &cp->mlist[i].name[start], len);
		children[num_chn-1][len] = '\0';
		need += len+1;
#ifdef PCP_DEBUG
		if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL1)) {
		    fprintf(stderr, "__dmchildren: offspring[%d] %s", num_chn-1, children[num_chn-1]);
		}
#endif

		if (statuslist != NULL) {
		    if ((status = (int *)realloc(status, num_chn*sizeof(status[0]))) == NULL) {
			PM_UNLOCK(registered.mutex);
			__pmNoMem("__dmchildren: statrus", num_chn*sizeof(status[0]), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    status[num_chn-1] = cp->mlist[i].name[start+len] == '\0' ? PMNS_LEAF_STATUS : PMNS_NONLEAF_STATUS;
#ifdef PCP_DEBUG
		    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL1)) {
			fprintf(stderr, " (status=%d)", status[num_chn-1]);
		}
#endif
		}
#ifdef PCP_DEBUG
		if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL1)) {
		    fputc('\n', stderr);
		}
#endif
	    }
	}
    }

    if (num_chn == 0) {
	PM_UNLOCK(registered.mutex);
	return PM_ERR_NAME;
    }

    /*
     * children[] is complete, but to ensure correct free()ing of
     * allocated space, we need to restructure this so that
     * n_children[] and all the names are allocated in a single
     * block, as per the pmGetChildren() semantics ... even though
     * n_children[] is never handed back to the caller, the stitch
     * and cleanup logic in pmGetChildrenStatus() assumes that
     * free(n_children) is all that is needed.
     */
    need += num_chn * sizeof(char *);
    if ((n_children = (char **)malloc(need)) == NULL) {
	__pmNoMem("__dmchildren: n_children", need, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    q = (char *)&n_children[num_chn];
    for (j = 0; j < num_chn; j++) {
	n_children[j] = q;
	strcpy(q, children[j]);
	q += strlen(children[j]) + 1;
	free(children[j]);
    }
    free(children);

    *offspring = n_children;
    if (statuslist != NULL)
	*statuslist = status;

    PM_UNLOCK(registered.mutex);
    return num_chn;
}

int
__dmgetpmid(const char *name, pmID *dp)
{
    int		i;

    initialize_mutex();
    PM_LOCK(registered.mutex);
    __dminit();

    for (i = 0; i < registered.nmetric; i++) {
	if (strcmp(name, registered.mlist[i].name) == 0) {
	    *dp = registered.mlist[i].pmid;
	    PM_UNLOCK(registered.mutex);
	    return 0;
	}
    }
    PM_UNLOCK(registered.mutex);
    return PM_ERR_NAME;
}

int
__dmgetname(pmID pmid, char ** name)
{
    int		i;

    initialize_mutex();
    PM_LOCK(registered.mutex);
    __dminit();

    for (i = 0; i < registered.nmetric; i++) {
	if (pmid == registered.mlist[i].pmid) {
	    *name = strdup(registered.mlist[i].name);
	    if (*name == NULL) {
		PM_UNLOCK(registered.mutex);
		return -oserror();
	    }
	    else {
		PM_UNLOCK(registered.mutex);
		return 0;
	    }
	}
    }
    PM_UNLOCK(registered.mutex);
    return PM_ERR_PMID;
}

void
__dmopencontext(__pmContext *ctxp)
{
    int		i;
    int		sts;
    ctl_t	*cp;

    initialize_mutex();
    PM_LOCK(registered.mutex);
    __dminit();

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL1)) {
	fprintf(stderr, "__dmopencontext(->ctx %d) called\n", __pmPtrToHandle(ctxp));
    }
#endif
    if (registered.nmetric == 0) {
	ctxp->c_dm = NULL;
	PM_UNLOCK(registered.mutex);
	return;
    }
    if ((cp = (void *)malloc(sizeof(ctl_t))) == NULL) {
	PM_UNLOCK(registered.mutex);
	__pmNoMem("pmNewContext: derived metrics (ctl)", sizeof(ctl_t), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    ctxp->c_dm = (void *)cp;
    cp->nmetric = registered.nmetric;
    if ((cp->mlist = (dm_t *)malloc(cp->nmetric*sizeof(dm_t))) == NULL) {
	PM_UNLOCK(registered.mutex);
	__pmNoMem("pmNewContext: derived metrics (mlist)", cp->nmetric*sizeof(dm_t), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    for (i = 0; i < cp->nmetric; i++) {
	pmID	pmid;
	cp->mlist[i].name = registered.mlist[i].name;
	cp->mlist[i].pmid = registered.mlist[i].pmid;
	cp->mlist[i].anon = registered.mlist[i].anon;
	assert(registered.mlist[i].expr != NULL);
	if (!registered.mlist[i].anon) {
	    /*
	     * Assume anonymous derived metric names are unique, but otherwise
	     * derived metric names must not clash with real metric names ...
	     * and if this happens, the real metric wins!
	     * Logic here depends on pmLookupName() returning before any
	     * derived metric searching is performed if the name is valid
	     * for a real metric in the current context.
	     */
	    sts = pmLookupName(1, &registered.mlist[i].name, &pmid);
	    if (sts >= 0 && !IS_DERIVED(pmid)) {
		char	strbuf[20];
		pmprintf("Warning: %s: derived name matches metric %s: derived ignored\n",
			registered.mlist[i].name, pmIDStr_r(pmid, strbuf, sizeof(strbuf)));
		pmflush();
		cp->mlist[i].expr = NULL;
		continue;
	    }
	}
	/* failures must be reported in bind_expr() or below */
	cp->mlist[i].expr = bind_expr(i, registered.mlist[i].expr);
	if (cp->mlist[i].expr != NULL) {
	    /* failures must be reported in check_expr() or below */
	    sts = check_expr(i, cp->mlist[i].expr);
	    if (sts < 0) {
		free_expr(cp->mlist[i].expr);
		cp->mlist[i].expr = NULL;
	    }
	    else {
		/* set correct PMID in pmDesc at the top level */
		cp->mlist[i].expr->desc.pmid = cp->mlist[i].pmid;
	    }
	}
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_DERIVE) && cp->mlist[i].expr != NULL) {
	    fprintf(stderr, "__dmopencontext: bind metric[%d] %s\n", i, registered.mlist[i].name);
	    if (pmDebug & DBG_TRACE_APPL1)
		__dmdumpexpr(cp->mlist[i].expr, 0);
	}
#endif
    }
    PM_UNLOCK(registered.mutex);
}

void
__dmclosecontext(__pmContext *ctxp)
{
    int		i;
    ctl_t	*cp = (ctl_t *)ctxp->c_dm;

    /* if needed, __dminit() called in __dmopencontext beforehand */

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DERIVE) {
	fprintf(stderr, "__dmclosecontext(->ctx %d) called dm->" PRINTF_P_PFX "%p %d metrics\n", __pmPtrToHandle(ctxp), cp, cp == NULL ? -1 : cp->nmetric);
    }
#endif
    if (cp == NULL) return;
    for (i = 0; i < cp->nmetric; i++) {
	free_expr(cp->mlist[i].expr); 
    }
    free(cp->mlist);
    free(cp);
    ctxp->c_dm = NULL;
}

int
__dmdesc(__pmContext *ctxp, pmID pmid, pmDesc *desc)
{
    int		i;
    ctl_t	*cp = (ctl_t *)ctxp->c_dm;

    /* if needed, __dminit() called in __dmopencontext beforehand */

    if (cp == NULL) return PM_ERR_PMID;

    for (i = 0; i < cp->nmetric; i++) {
	if (cp->mlist[i].pmid == pmid) {
	    if (cp->mlist[i].expr == NULL)
		/* bind failed for some reason, reported earlier */
		return PM_ERR_NAME;
	    *desc = cp->mlist[i].expr->desc;
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == registered.mutex ... no locking here to avoid
 * recursion ad nauseum
 */
int
__pmIsDeriveLock(void *lock)
{
    return lock == (void *)&registered.mutex;
}
#endif
