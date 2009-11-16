#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"

/*
 * Derived Metrics support
 */

typedef struct node {		/* expression tree node */
    int		type;
    struct node	*left;
    struct node	*right;
    void	*ref;
} node_t;

typedef struct {		/* map for all derived metrics */
    char	*name;
    pmID	pmid;
    node_t	*expr;
} dm_t;
static dm_t	*metriclist = NULL;
static int	nmetric = 0;

/* parser and lexer variables */
static char	*tokbuf = NULL;
static int	tokbuflen;
static char	*this;		/* start of current lexicon */
static int	lexpeek = 0;
static char	*string;
static char	*errmsg;

/* lexical types */
#define L_ERROR		-2
#define	L_EOF		-1
#define L_UNDEF		0
#define L_NUMBER	1
#define L_NAME		2
#define L_EQUALS	3
#define L_PLUS		4
#define L_MINUS		5
#define L_STAR		6
#define L_SLASH		7
#define L_LPAREN	8
#define L_RPAREN	9
#define L_DELTA		10

/* function table for lexer */
static struct {
    int		type;
    char	*name;
} func[] = {
    { L_DELTA,	"delta" },
    { L_UNDEF,	NULL }
};

/* parser states */
#define P_INIT		0
#define P_LEAF		1
#define P_BINOP		2
#define P_END		99

static void
unget(int c)
{
    lexpeek = c;
}

static int
get()
{
    static int	eof = 0;
    int		c;
    if (lexpeek != 0) {
	c = lexpeek;
	lexpeek = 0;
	return c;
    }
    if (eof) return L_EOF;
    do {
	c = *string;
	if (c == '\0') {
	    return L_EOF;
	    eof = 1;
	}
	else
	    string++;
    } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    return c;
}

static int
lex(void)
{
    int		c;
    char	*p = tokbuf;
    int		type = L_UNDEF;
    int		i;
    int		firstch = 1;

    for ( ; ; ) {
	c = get();
	if (firstch) {
	    this = &string[-1];
	    firstch = 0;
	}
	if (c == L_EOF) {
	    if (type != L_UNDEF) {
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
		__pmNoMem("pmRegisterDerived: alloc tokbuf", tokbuflen, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	}
	else if (p >= &tokbuf[tokbuflen]) {
	    int		x = p - tokbuf;
	    tokbuflen *= 2;
	    if ((tokbuf = (char *)realloc(tokbuf, tokbuflen)) == NULL) {
		__pmNoMem("pmRegisterDerived: realloc tokbuf", tokbuflen, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    p = &tokbuf[x];
	}

	*p++ = (char)c;

	if (type == L_UNDEF) {
	    if (isdigit(c))
		type = L_NUMBER;
	    else if (isalpha(c))
		type = L_NAME;
	    else {
		switch (c) {
		    case '=':
			*p = '\0';
			return L_EQUALS;
			break;

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
	    if (type == L_NUMBER) {
		if (!isdigit(c)) {
		    unget(c);
		    p[-1] = '\0';
		    return L_NUMBER;
		}
	    }
	    else if (type == L_NAME) {
		if (isalpha(c) || isdigit(c) || c == '_' || c == '.')
		    continue;
		if (c == '(') {
		    /* check for functions ... */
		    int		namelen = p - tokbuf - 1;
		    for (i = 0; func[i].name != NULL; i++) {
			if (namelen == strlen(func[i].name) &&
			    strncmp(tokbuf, func[i].name, namelen) == 0) {
			    *p = '\0';
			    return func[i].type;
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
	__pmNoMem("pmRegisterDerived: newnode", sizeof(node_t), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    np->type = type;
    np->left = NULL;
    np->right = NULL;
    np->ref = NULL;
    return np;
}

static void
free_expr(node_t *np)
{
    if (np == NULL) return;
    if (np->left != NULL) free_expr(np->left);
    if (np->right != NULL) free_expr(np->right);
    if (np->ref != NULL) free(np->ref);
    free(np);
}

static void
dump_expr(node_t *np, int level)
{
    if (level == 0) fprintf(stderr, "\nExpr dump from %p...\n", np);
    if (np == NULL) return;
    fprintf(stderr, "%p node type=%d left=%p right=%p ref=%p", np, np->type, np->left, np->right, np->ref);
    if (np->type == L_NAME) fprintf(stderr, " [name=%s]", (char *)np->ref);
    if (np->type == L_NUMBER) fprintf(stderr, " [number=%s]", (char *)np->ref);
    if (np->type == L_PLUS) fprintf(stderr, " [+]");
    if (np->type == L_MINUS) fprintf(stderr, " [-]");
    if (np->type == L_STAR) fprintf(stderr, " [*]");
    if (np->type == L_SLASH) fprintf(stderr, " [/]");
    fputc('\n', stderr);
    if (np->left != NULL) dump_expr(np->left, level+1);
    if (np->right != NULL) dump_expr(np->right, level+1);
}

static node_t *
parse(void)
{
    int		state = P_INIT;
    int		type;
    node_t	*expr = NULL;
    node_t	*np;

    for ( ; ; ) {
	type = lex();
	// fprintf(stderr, "lex -> %d %s\n", type, type == L_EOF ? "" : tokbuf);
	/* handle lexicons that terminate the parsing */
	switch (type) {
	    case L_ERROR:
		errmsg = "Illegal character";
		free_expr(expr);
		return NULL;
		break;
	    case L_EOF:
		if (state == P_LEAF)
		    return expr;
		errmsg = "End of input";
		free_expr(expr);
		return NULL;
		break;
	    case L_RPAREN:
		if (state == P_LEAF)
		    return expr;
		errmsg = "Unexpected ')'";
		free_expr(expr);
		return NULL;
		break;
	}

	switch (state) {
	    case P_INIT:
		if (type == L_NAME || type == L_NUMBER) {
		    expr = newnode(type);
		    if ((expr->ref = (void *)strdup(tokbuf)) == NULL) {
			__pmNoMem("pmRegisterDerived: leaf node", strlen(tokbuf)+1, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    state = P_LEAF;
		}
		else if (type == L_LPAREN) {
		    expr = parse();
		    if (expr == NULL)
			return NULL;
		    state = P_LEAF;
		}
		else
		    return NULL;
		break;

	    case P_LEAF:
		if (type == L_PLUS || type == L_MINUS || type == L_STAR || type == L_SLASH) {
		    np = newnode(type);
		    np->left = expr;
		    expr = np;
		    state = P_BINOP;
		}
		else {
		    free_expr(expr);
		    return NULL;
		}
		break;

	    case P_BINOP:
		if (type == L_NAME || type == L_NUMBER) {
		    np = newnode(type);
		    expr->right = np;
		    if ((np->ref = (void *)strdup(tokbuf)) == NULL) {
			__pmNoMem("pmRegisterDerived: leaf node", strlen(tokbuf)+1, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    state = P_LEAF;
		}
		else if (type == L_LPAREN) {
		    np = parse();
		    if (np == NULL)
			return NULL;
		    expr->right = np;
		    state = P_LEAF;
		}
		else {
		    free_expr(expr);
		    return NULL;
		}
		break;

	    default:
		free_expr(expr);
		return NULL;
	}
    }
}

char *
pmRegisterDerived(char *name, char *expr)
{
    node_t		*np;
    static __pmID_int	pmid;

    errmsg = NULL;
    string = expr;
    np = parse();
    if (np == NULL) {
	/* parser error */
	return this;
    }

    nmetric++;
    metriclist = (dm_t *)realloc(metriclist, nmetric*sizeof(metriclist[0]));
    if (metriclist == NULL) {
	__pmNoMem("pmRegisterDerived: metriclist", nmetric*sizeof(metriclist[0]), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    if (nmetric == 1) {
	pmid.flag = 0;
	pmid.domain = DYNAMIC_PMID;
	pmid.cluster = 0;
    }
    metriclist[nmetric-1].name = strdup(name);
    pmid.item = nmetric;
    metriclist[nmetric-1].pmid = *((pmID *)&pmid);
    metriclist[nmetric-1].expr = np;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DERIVE) {
	fprintf(stderr, "pmRegisterDerived: register metric[%d] %s\n", nmetric-1, name);
	if (pmDebug & DBG_TRACE_APPL2)
	    dump_expr(np, 0);
    }
#endif

    return NULL;
}

int
pmLoadDerivedConfig(char *fname)
{
    FILE	*fp;
    int		buflen;
    char	*buf;
    char	*p;
    int		c;
    int		sts = 0;
    int		eq = -1;
    int		lineno = 1;

    if ((fp = fopen(fname, "r")) == NULL) {
	return -errno;
    }
    buflen = 128;
    if ((buf = (char *)malloc(buflen)) == NULL) {
	__pmNoMem("pmLoadDerivedConfig: alloc buf", buflen, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    p = buf;
    while ((c = fgetc(fp)) != EOF) {
	if (p == &buf[buflen]) {
	    if ((buf = (char *)realloc(buf, 2*buflen)) == NULL) {
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
	    *p = '\0';
	    if (eq != -1) {
		char	*np;
		char	*q;
		char	*errp;
		/* terminate and trim white from metric name */
		buf[eq] = '\0';
		if ((np = strdup(buf)) == NULL) {
		    __pmNoMem("pmLoadDerivedConfig: dupname", strlen(buf), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		q = &np[eq-1];
		while (q >= np && isspace(*q))
		    *q-- = '\0';
		q = np;
		while (*q && isspace(*q))
		    q++;
		if (*q == '\0') {
		    buf[eq] = '=';
		    pmprintf("[%s:%d] Error: pmLoadDerivedConfig: derived metric name missing\n%s\n", fname, lineno, buf);
		    pmflush();
		}
		else {
		    errp = pmRegisterDerived(q, &buf[eq+1]);
		    if (errp != NULL) {
			pmprintf("[%s:%d] Error: pmRegisterDerived(%s, ...) syntax error\n", fname, lineno, np);
			pmprintf("%s\n", &buf[eq+1]);
			for (q = &buf[eq+1]; *q; q++) {
			    if (q == errp) *q = '^';
			    else if (!isspace(*q)) *q = ' ';
			}
			pmprintf("%s\n", &buf[eq+1]);
			q = pmDerivedErrStr();
			if (q != NULL) pmprintf("%s\n", q);
			pmflush();
		    }
		    else
			sts++;
		}
	    }
	    else {
		/*
		 * error ... no = in the line, so no derived metric name
		 */
		pmprintf("[%s:%d] Error: pmLoadDerivedConfig: derived metric name missing\n%s\n", fname, lineno, buf);
		pmflush();
	    }
	    lineno++;
	    p = buf;
	    eq = -1;
	}
	else
	    *p++ = c;
    }
    return sts;
}

char *
pmDerivedErrStr(void)
{
    return errmsg;
}

/*
 * callbacks
 */

int
__dmtraverse(const char *name, char ***namelist)
{
    int		sts = 0;
    int		i;
    char	**list = NULL;
    int		matchlen = strlen(name);

    for (i = 0; i < nmetric; i++) {
	/*
	 * prefix match ... if name is "", then all names match
	 */
	if (matchlen == 0 ||
	    (strncmp(name, metriclist[i].name, matchlen) == 0 &&
	     (metriclist[i].name[matchlen] == '.' ||
	      metriclist[i].name[matchlen] == '\0'))) {
	    sts++;
	    if ((list = (char **)realloc(list, sts*sizeof(list[0]))) == NULL) {
		__pmNoMem("__dmtraverse: list", sts*sizeof(list[0]), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    list[sts-1] = metriclist[i].name;
	}
    }
    *namelist = list;

    return sts;
}

int
__dmgetpmid(const char *name, pmID *dp)
{
    int		i;

    for (i = 0; i < nmetric; i++) {
	if (strcmp(name, metriclist[i].name) == 0) {
	    *dp = metriclist[i].pmid;
	    return 0;
	}
    }
    return PM_ERR_NAME;
}
