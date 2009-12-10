/*
 * Copyright (c) 2009 Ken McDonell.  All Rights Reserved.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 *
 * Debug Flags
 *	DERIVE - high-level diagnostics
 *	DERIVE & APPL0 - configuration and static syntax analysis
 *	DERIVE & APPL1 - expression binding and semantic analysis
 *	DERIVE & APPL2 - fetch handling
 */

/*
 * TODO
 *
 *	if pmRegister called _after_ context is open, then iterate
 *	over all contexts and expand the ctl_t struct and call
 *	bind_expr and check_expr to add new dm for each context
 *
 *	Need to handle
 *	  pmRegister before any context is open
 *	  new contexts after pmRegister is called (the initial implementation)
 *	  intermixed pmRegister and newContext calls
 *
 *	pmFetch pre- and post- callbacks
 *
 *      reverse name lookup (check pmResult dumps in QA/249)
 *
 * 	delta() support --  any other functions?
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "pmapi.h"
#include "impl.h"

/*
 * Derived Metrics support
 */

typedef struct {		/* one value in the expression tree */
    int		inst;
    pmAtomValue	value;
    int		vlen;		/* from vlen of pmValueBlock for string and aggregates */
} val_t;

typedef struct {		/* dynamic information for an expression node */
    pmID	pmid;
    int		numval;
    int		iv_alloc;	/* set if ivlist is allocated from this node */
    val_t	*ivlist;	/* instance-value pairs */
} info_t;

typedef struct node {		/* expression tree node */
    int		type;
    pmDesc	desc;
    struct node	*left;
    struct node	*right;
    char	*value;
    info_t	*info;
} node_t;

typedef struct {		/* one derived metric */
    char	*name;
    pmID	pmid;
    node_t	*expr;
} dm_t;

/*
 * Control structure for a set of derived metrics.
 * This is used for the static definitions (registered) and the dynamic
 * tree of expressions maintained per context.
 */
typedef struct {
    int		nmetric;	/* derived metrics */
    dm_t	*mlist;
    int		fetch_has_dm;	/* ==1 if pmResult rewrite needed */
    int		numpmid;	/* from pmFetch before rewrite */
} ctl_t;

static ctl_t	registered;

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

static char *type_dbg[] = { "ERROR", "EOF", "UNDEF", "NUMBER", "NAME", "EQUALS", "PLUS", "MINUS", "STAR", "SLASH", "LPAREN", "RPAREN", "DELTA" };
static char type_c[] = { '\0', '\0', '\0', '\0', '\0', '=', '+', '-', '*', '/', '(', ')', '\0' };

/* function table for lexer */
static struct {
    int		f_type;
    char	*f_name;
} func[] = {
    { L_DELTA,	"delta" },
    { L_UNDEF,	NULL }
};

/* parser states */
#define P_INIT		0
#define P_LEAF		1
#define P_LEAF_PAREN	2
#define P_BINOP		3
#define P_END		99

static char *state_dbg[] = { "INIT", "LEAF", "LEAF_PAREN", "BINOP" };

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
    c = *string;
    if (c == '\0') {
	return L_EOF;
	eof = 1;
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
	    if (isspace(c)) continue;
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

	if (ltype == L_UNDEF) {
	    if (isdigit(c))
		ltype = L_NUMBER;
	    else if (isalpha(c))
		ltype = L_NAME;
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
	    if (ltype == L_NUMBER) {
		if (!isdigit(c)) {
		    unget(c);
		    p[-1] = '\0';
		    return L_NUMBER;
		}
	    }
	    else if (ltype == L_NAME) {
		if (isalpha(c) || isdigit(c) || c == '_' || c == '.')
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
	__pmNoMem("pmRegisterDerived: newnode", sizeof(node_t), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    np->type = type;
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
	__pmNoMem("bind_expr: info block", sizeof(info_t), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    new->info->pmid = PM_ID_NULL;
    new->info->numval = 0;
    new->info->ivlist = NULL;

    /*
     * need info to be non-null to protect copy of value in free_expr
     */
    new->value = np->value;

    if (new->type == L_NAME) {
	int	sts;

	sts = pmLookupName(1, &new->value, &new->info->pmid);
	if (sts < 0) {
	    pmprintf("Error: derived metric %s: operand: %s: %s\n", registered.mlist[n].name, new->value, pmErrStr(sts));
	    pmflush();
	    free(new->info);
	    free(new);
	    return NULL;
	}
	sts = pmLookupDesc(new->info->pmid, &new->desc);
	if (sts < 0) {
	    pmprintf("Error: derived metric %s: operand (%s [%s]): %s\n", registered.mlist[n].name, new->value, pmIDStr(new->info->pmid), pmErrStr(sts));
	    pmflush();
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

/* type promotion */
static int promote[6][6] = {
    { PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE },
    { PM_TYPE_U32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE },
    { PM_TYPE_64, PM_TYPE_64, PM_TYPE_64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE },
    { PM_TYPE_U64, PM_TYPE_U64, PM_TYPE_U64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE },
    { PM_TYPE_FLOAT, PM_TYPE_FLOAT, PM_TYPE_FLOAT, PM_TYPE_FLOAT, PM_TYPE_FLOAT, PM_TYPE_DOUBLE },
    { PM_TYPE_DOUBLE, PM_TYPE_DOUBLE, PM_TYPE_DOUBLE, PM_TYPE_DOUBLE, PM_TYPE_DOUBLE, PM_TYPE_DOUBLE }
};

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
     * type promotion (similar to ANSI C)
     * PM_TYPE_STRING, PM_TYPE_AGGREGATE and PM_TYPE_AGGREGATE_STATIC are
     * illegal operands except for renaming (no operator involved)
     * for the integer type operands, division => PM_TYPE_FLOAT
     * else PM_TYPE_DOUBLE & any type => PM_TYPE_DOUBLE
     * else PM_TYPE_FLOAT & any type => PM_TYPE_FLOAT
     * else PM_TYPE_U64 & any type => PM_TYPE_U64
     * else PM_TYPE_64 & any type => PM_TYPE_64
     * else PM_TYPE_U32 & any type => PM_TYPE_U32
     * else PM_TYPE_32 & any type => PM_TYPE_32
     *
     * units mapping
     * operator			checks
     * +, -			identical
     *				(TODO relax to union compatible and scale)
     * *, /			if one is counter, non-counter must
     *				have pmUnits of "none"
     */
    pmDesc	*right = &np->right->desc;
    pmDesc	*left = &np->left->desc;
    char	*errmsg;

    if (left->sem == PM_SEM_COUNTER) {
	if (right->sem == PM_SEM_COUNTER) {
	    if (np->type != L_PLUS && np->type != L_MINUS) {
		errmsg = "Illegal operator for counters";
		goto bad;
	    }
	}
	else {
	    if (np->type != L_STAR && np->type != L_SLASH) {
		errmsg = "Illegal operator for counter and non-counter";
		goto bad;
	    }
	}
    }
    else {
	if (right->sem == PM_SEM_COUNTER) {
	    if (np->type != L_STAR) {
		errmsg = "Illegal operator for non-counter and counter";
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
		errmsg = "Illegal operator for non-counters";
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
	    errmsg = "Non-arithmetic type for left operand";
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
	    errmsg = "Non-arithmetic type for right operand";
	    goto bad;
    }
    np->desc.type = promote[left->type][right->type];
    if (np->type == L_SLASH && np->desc.type != PM_TYPE_FLOAT && np->desc.type != PM_TYPE_DOUBLE) {
	/* for division result is real number */
	np->desc.type = PM_TYPE_FLOAT;
    }

    /* TODO finish off ... sem checking */
    return 0;

bad:
    pmprintf("Semantic error: derived metric %s: ", registered.mlist[n].name);
    if (np->left->type == L_NUMBER || np->left->type == L_NAME)
	pmprintf("%s ", np->left->value);
    else
	pmprintf("<expr> ");
    pmprintf("%c ", type_c[np->type+2]);
    if (np->right->type == L_NUMBER || np->right->type == L_NAME)
	pmprintf("%s", np->right->value);
    else
	pmprintf("<expr>");
    pmprintf(": %s\n", errmsg);
    pmflush();
    return -1;
}

static int
check_expr(int n, node_t *np)
{
    int		sts;

    assert(np != NULL);
    if (np->type == L_NUMBER || np->type == L_NAME)
	return 0;
    if (np->left != NULL)
	if ((sts = check_expr(n, np->left)) < 0)
	    return sts;
    if (np->right != NULL)
	if ((sts = check_expr(n, np->right)) < 0)
	    return sts;
    if (np->left == NULL) {
	np->desc = np->right->desc;	/* struct copy */
    }
    else if (np->right == NULL) {
	np->desc = np->left->desc;	/* struct copy */
    }
    else {
	/* build pmDesc from pmDesc of both operands */
	if ((sts = map_desc(n, np)) < 0) {
	    return sts;
	}
    }
    return 0;
}

static void
dump_expr(node_t *np, int level)
{
    if (level == 0) fprintf(stderr, "Expr dump from %p...\n", np);
    if (np == NULL) return;
    fprintf(stderr, "%p node type=%d left=%p right=%p", np, np->type, np->left, np->right);
    if (np->type == L_NAME) fprintf(stderr, " master=%d [name=%s]", np->info == NULL ? 1 : 0, np->value);
    if (np->type == L_NUMBER) fprintf(stderr, " master=%d [number=%s]", np->info == NULL ? 1 : 0, np->value);
    if (np->type == L_PLUS) fprintf(stderr, " [+]");
    if (np->type == L_MINUS) fprintf(stderr, " [-]");
    if (np->type == L_STAR) fprintf(stderr, " [*]");
    if (np->type == L_SLASH) fprintf(stderr, " [/]");
    fputc('\n', stderr);
    /* TODO - include np->desc? */
    if (np->info) {
	fprintf(stderr, "  pmid=%s numval=%d\n", pmIDStr(np->info->pmid), np->info->numval);
	/* TODO - include ivlist[]? */
    }
    if (np->left != NULL) dump_expr(np->left, level+1);
    if (np->right != NULL) dump_expr(np->right, level+1);
}

/*
 * Parser FSA
 * state	lex		new state
 * P_INIT	L_NAME or	P_LEAF
 * 		L_NUMBER
 * P_INIT	L_LPAREN	if parse() != NULL then P_LEAF
 * P_LEAF	L_PLUS or	P_BINOP
 * 		L_MINUS or
 * 		L_STAR or
 * 		L_SLASH
 * P_BINOP	L_NAME or	P_LEAF
 * 		L_NUMBER
 * P_BINOP	L_LPAREN	if parse() != NULL then P_LEAF
 * P_LEAF_PAREN	same as P_LEAF, but no precedence rules at next operator
 */
static node_t *
parse(int level)
{
    int		state = P_INIT;
    int		type;
    node_t	*expr = NULL;
    node_t	*curr = NULL;
    node_t	*np;

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
		errmsg = "Illegal character";
		free_expr(expr);
		return NULL;
		break;
	    case L_EOF:
		if (state == P_LEAF || state == P_LEAF_PAREN)
		    return expr;
		errmsg = "End of input";
		free_expr(expr);
		return NULL;
		break;
	    case L_RPAREN:
		if (state == P_LEAF || state == P_LEAF_PAREN)
		    return expr;
		errmsg = "Unexpected ')'";
		free_expr(expr);
		return NULL;
		break;
	}

	switch (state) {
	    case P_INIT:
		if (type == L_NAME || type == L_NUMBER) {
		    expr = curr = newnode(type);
		    if ((curr->value = strdup(tokbuf)) == NULL) {
			__pmNoMem("pmRegisterDerived: leaf node", strlen(tokbuf)+1, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    if (type == L_NUMBER) {
			/* TODO check value is small enough to fit in a 32-bit int */
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
		else
		    return NULL;
		break;

	    case P_LEAF_PAREN:	/* fall through */
	    case P_LEAF:
		if (type == L_PLUS || type == L_MINUS || type == L_STAR || type == L_SLASH) {
		    np = newnode(type);
		    if (state == P_LEAF_PAREN ||
		        (curr->type == L_NAME || curr->type == L_NUMBER) ||
		        (type == L_PLUS || type == L_MINUS)) {
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
		    return NULL;
		}
		break;

	    case P_BINOP:
		if (type == L_NAME || type == L_NUMBER) {
		    np = newnode(type);
		    if ((np->value = strdup(tokbuf)) == NULL) {
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

static int
checkname(char *p)
{
    int	firstch = 1;

    for ( ; *p; p++) {
	if (firstch) {
	    firstch = 0;
	    if (isalpha(*p)) continue;
	    return -1;
	}
	else {
	    if (isalpha(*p) || isdigit(*p) || *p == '_') continue;
	    if (*p == '.') {
		firstch = 1;
		continue;
	    }
	    return -1;
	}
    }
    return 0;
}

char *
pmRegisterDerived(char *name, char *expr)
{
    node_t		*np;
    static __pmID_int	pmid;

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL0)) {
	fprintf(stderr, "pmRegisterDerived: name=\"%s\" expr=\"%s\"\n", name, expr);
    }
#endif

    /* TODO check for and reject duplicate names */

    errmsg = NULL;
    string = expr;
    np = parse(1);
    if (np == NULL) {
	/* parser error */
	return this;
    }

    registered.nmetric++;
    registered.mlist = (dm_t *)realloc(registered.mlist, registered.nmetric*sizeof(dm_t));
    if (registered.mlist == NULL) {
	__pmNoMem("pmRegisterDerived: registered mlist", registered.nmetric*sizeof(dm_t), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    if (registered.nmetric == 1) {
	pmid.flag = 0;
	pmid.domain = DYNAMIC_PMID;
	pmid.cluster = 0;
    }
    registered.mlist[registered.nmetric-1].name = strdup(name);
    pmid.item = registered.nmetric;
    registered.mlist[registered.nmetric-1].pmid = *((pmID *)&pmid);
    registered.mlist[registered.nmetric-1].expr = np;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DERIVE) {
	fprintf(stderr, "pmRegisterDerived: register metric[%d] %s = %s\n", registered.nmetric-1, name, expr);
	if (pmDebug & DBG_TRACE_APPL0)
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
	    if (buf[0] == '#') {
		/* comment line, skip it ... */
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
		    __pmNoMem("pmLoadDerivedConfig: dupname", strlen(buf), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		/* trim white space from tail of metric name */
		q = &np[eq-1];
		while (q >= np && isspace(*q))
		    *q-- = '\0';
		/* trim white space from head of metric name */
		q = np;
		while (*q && isspace(*q))
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
		while (*ep != '\0' && isspace(*ep))
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
			else if (!isspace(*q)) *q = ' ';
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

    for (i = 0; i < registered.nmetric; i++) {
	/*
	 * prefix match ... if name is "", then all names match
	 */
	if (matchlen == 0 ||
	    (strncmp(name, registered.mlist[i].name, matchlen) == 0 &&
	     (registered.mlist[i].name[matchlen] == '.' ||
	      registered.mlist[i].name[matchlen] == '\0'))) {
	    sts++;
	    if ((list = (char **)realloc(list, sts*sizeof(list[0]))) == NULL) {
		__pmNoMem("__dmtraverse: list", sts*sizeof(list[0]), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    list[sts-1] = registered.mlist[i].name;
	}
    }
    *namelist = list;

    return sts;
}

int
__dmgetpmid(const char *name, pmID *dp)
{
    int		i;

    for (i = 0; i < registered.nmetric; i++) {
	if (strcmp(name, registered.mlist[i].name) == 0) {
	    *dp = registered.mlist[i].pmid;
	    return 0;
	}
    }
    return PM_ERR_NAME;
}

void
__dmopencontext(__pmContext *ctxp)
{
    int		i;
    int		sts;
    ctl_t	*cp;

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL1)) {
	fprintf(stderr, "__dmopencontext() called\n");
    }
#endif
    if (registered.nmetric == 0) {
	ctxp->c_dm = NULL;
	return;
    }
    if ((cp = (void *)malloc(sizeof(ctl_t))) == NULL) {
	__pmNoMem("pmNewContext: derived metrics (ctl)", sizeof(ctl_t), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    ctxp->c_dm = (void *)cp;
    cp->nmetric = registered.nmetric;
    if ((cp->mlist = (dm_t *)malloc(cp->nmetric*sizeof(dm_t))) == NULL) {
	__pmNoMem("pmNewContext: derived metrics (mlist)", cp->nmetric*sizeof(dm_t), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    for (i = 0; i < cp->nmetric; i++) {
	cp->mlist[i].name = registered.mlist[i].name;
	cp->mlist[i].pmid = registered.mlist[i].pmid;
	if (registered.mlist[i].expr != NULL) {
	    /* failures must be reported in bind_expr() or below */
	    cp->mlist[i].expr = bind_expr(i, registered.mlist[i].expr);
	    if (cp->mlist[i].expr != NULL) {
		/* failures must be reported in check_expr() or below */
		sts = check_expr(i, cp->mlist[i].expr);
		if (sts < 0) {
		    free_expr(cp->mlist[i].expr);
		    cp->mlist[i].expr = NULL;
		}
	    }
	}
	else
	    cp->mlist[i].expr = NULL;
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_DERIVE) && cp->mlist[i].expr != NULL) {
	    fprintf(stderr, "__dmopencontext: bind metric[%d] %s\n", i, registered.mlist[i].name);
	    if (pmDebug & DBG_TRACE_APPL1)
		dump_expr(cp->mlist[i].expr, 0);
	}
#endif
    }
}

void
__dmclosecontext(__pmContext *ctxp)
{
    int		i;
    ctl_t	*cp = (ctl_t *)ctxp->c_dm;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DERIVE) {
	fprintf(stderr, "__dmclosecontext() called dm->%p %d metrics\n", cp, cp == NULL ? -1 : cp->nmetric);
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

static void
get_pmids(node_t *np, int *cnt, pmID **list)
{
    assert(np != NULL);
    if (np->left != NULL) get_pmids(np->left, cnt, list);
    if (np->right != NULL) get_pmids(np->right, cnt, list);
    if (np->type == L_NAME) {
	(*cnt)++;
	if ((*list = (pmID *)realloc(*list, (*cnt)*sizeof(pmID))) == NULL) {
	    __pmNoMem("__dmprefetch: realloc xtralist", (*cnt)*sizeof(pmID), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	(*list)[*cnt-1] = np->info->pmid;
    }
}

/*
 * Walk the pmidlist[] from pmFetch.
 * For each derived metric found in the list add all the operand metrics,
 * and build a combined pmID list (newlist).
 * Return the number of pmIDs in the combined list.
 *
 * The derived metric pmIDs are left in the combined list (they will
 * return PM_ERR_NOAGENT from the fetch) to simplify the post-processing
 * of the pmResult in __dmpostfetch()
 */
int
__dmprefetch(__pmContext *ctxp, int numpmid, pmID *pmidlist, pmID **newlist)
{
    int		i;
    int		j;
    int		m;
    int		xtracnt = 0;
    pmID	*xtralist = NULL;
    pmID	*list;
    ctl_t	*cp = (ctl_t *)ctxp->c_dm;

    if (cp == NULL) return numpmid;

    /*
     * save numpmid to be used in __dmpostfetch() ... works because calls
     * to pmFetch cannot be nested (at all, but certainly for the same
     * context).
     * Ditto for the fast path flag (fetch_has_dm).
     */
    cp->numpmid = numpmid;
    cp->fetch_has_dm = 0;

    for (m = 0; m < numpmid; m++) {
	if (pmid_domain(pmidlist[m]) != DYNAMIC_PMID)
	    continue;
	for (i = 0; i < cp->nmetric; i++) {
	    if (pmidlist[m] == cp->mlist[i].pmid) {
		if (cp->mlist[i].expr != NULL) {
		    get_pmids(cp->mlist[i].expr, &xtracnt, &xtralist);
		    cp->fetch_has_dm = 1;
		}
		break;
	    }
	}
    }
    if (xtracnt == 0) return numpmid;

    /*
     * Some of the "extra" ones, may already be in the caller's pmFetch 
     * list, or repeated in xtralist[] (if the same metric operand appears
     * more than once as a leaf node in the expression tree.
     * Remove these duplicates
     */
    j = 0;
    for (i = 0; i < xtracnt; i++) {
	for (m = 0; m < numpmid; m++) {
	    if (xtralist[i] == pmidlist[m])
		/* already in pmFetch list */
		break;
	}
	if (m < numpmid) continue;
	for (m = 0; m < j; m++) {
	    if (xtralist[i] == xtralist[m])
	    	/* already in xtralist[] */
		break;
	}
	if (m == j)
	    xtralist[j++] = xtralist[i];
    }
    xtracnt = j;
    if (xtracnt == 0) return numpmid;

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL2)) {
	fprintf(stderr, "derived metrics prefetch added %d metrics:", xtracnt);
	for (i = 0; i < xtracnt; i++)
	    fprintf(stderr, " %s", pmIDStr(xtralist[i]));
	fputc('\n', stderr);
    }
#endif
    if ((list = (pmID *)malloc((numpmid+xtracnt)*sizeof(pmID))) == NULL) {
	__pmNoMem("__dmprefetch: alloc list", (numpmid+xtracnt)*sizeof(pmID), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    for (m = 0; m < numpmid; m++) {
	list[m] = pmidlist[m];
    }
    for (i = 0; i < xtracnt; i++) {
	list[m++] = xtralist[i];
    }
    free(xtralist);
    *newlist = list;

    return m;
}

/*
 * Free the old ivlist[] (if any) ... may need to walk the list because
 * the pmAtomValues may have buffers attached in the type STRING and
 * type AGGREGATE* cases
 */
void static
free_ivlist(node_t *np)
{
    int		i;

    assert(np->info != NULL);

    if (np->info->ivlist != NULL) {
	if (np->desc.type == PM_TYPE_STRING ||
	    np->desc.type == PM_TYPE_AGGREGATE ||
	    np->desc.type == PM_TYPE_AGGREGATE_STATIC) {
	    for (i = 0; i < np->info->numval; i++) {
		if (np->info->ivlist[i].value.vp != NULL)
		    free(np->info->ivlist[i].value.vp);
	    }
	}
	free(np->info->ivlist);
	np->info->ivlist = NULL;
    }
}

/*
 * Walk an expression tree, filling in operand values from the
 * pmResult at the leaf nodes and propagating the computed values
 * towards the root node of the tree.
 *
 * The control variable iv_alloc determines if the ivlist[] entries
 * are allocated with the current node, or the current node points
 * to ivlist[] entries allocated in another node.
 */
static int
eval_expr(node_t *np, pmResult *rp, int level)
{
    int		sts;
    int		i;
    int		j;
    size_t	need;
    node_t	*src;

    assert(np != NULL);
    if (np->left != NULL) {
	sts = eval_expr(np->left, rp, level+1);
	if (sts <= 0) return sts;
    }
    if (np->right != NULL) {
	sts = eval_expr(np->right, rp, level+1);
	if (sts <= 0) return sts;
    }

    switch (np->type) {

	case L_NUMBER:
	    if (np->info->numval == 0) {
		/* initialize ivlist[] for singular instance first time through */
		np->info->numval = 1;
		if ((np->info->ivlist = (val_t *)malloc(sizeof(val_t))) == NULL) {
		    __pmNoMem("eval_expr: number ivlist", sizeof(val_t), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		np->info->iv_alloc = 1;
		np->info->ivlist[0].inst = PM_INDOM_NULL;
		/* don't need error checking, done in the lexical scanner */
		np->info->ivlist[0].value.l = atoi(np->value);
	    }
	    return 1;

	case L_NAME:
	    /*
	     * Extract instance-values from pmResult and store them in
	     * ivlist[] as <int, pmAtomValue> pairs
	     */
	    for (j = 0; j < rp->numpmid; j++) {
		if (np->info->pmid == rp->vset[j]->pmid) {
		    free_ivlist(np);
		    np->info->numval = rp->vset[j]->numval;
		    if ((np->info->ivlist = (val_t *)malloc(np->info->numval*sizeof(val_t))) == NULL) {
			__pmNoMem("eval_expr: metric ivlist", np->info->numval*sizeof(val_t), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    for (i = 0; i < np->info->numval; i++) {
			np->info->ivlist[i].inst = rp->vset[j]->vlist[i].inst;
			switch (np->desc.type) {
			    case PM_TYPE_32:
			    case PM_TYPE_U32:
				np->info->ivlist[i].value.l = rp->vset[j]->vlist[i].value.lval;
				break;

			    case PM_TYPE_64:
			    case PM_TYPE_U64:
				memcpy((void *)&np->info->ivlist[i].value.ll, (void *)rp->vset[j]->vlist[i].value.pval->vbuf, sizeof(__int64_t));
				break;

			    case PM_TYPE_FLOAT:
				if (rp->vset[j]->valfmt == PM_VAL_INSITU) {
				    /* old style insitu float */
				    np->info->ivlist[i].value.l = rp->vset[j]->vlist[i].value.lval;
				}
				else {
				    assert(rp->vset[j]->vlist[i].value.pval->vtype == PM_TYPE_FLOAT);
				    memcpy((void *)&np->info->ivlist[i].value.f, (void *)rp->vset[j]->vlist[i].value.pval->vbuf, sizeof(float));
				}
				break;

			    case PM_TYPE_DOUBLE:
				memcpy((void *)&np->info->ivlist[i].value.d, (void *)rp->vset[j]->vlist[i].value.pval->vbuf, sizeof(double));
				break;

			    case PM_TYPE_STRING:
				need = rp->vset[j]->vlist[i].value.pval->vlen-PM_VAL_HDR_SIZE;
				if ((np->info->ivlist[i].value.cp = (char *)malloc(need)) == NULL) {
				    __pmNoMem("eval_expr: string value", rp->vset[j]->vlist[i].value.pval->vlen, PM_FATAL_ERR);
				    /*NOTREACHED*/
				}
				memcpy((void *)np->info->ivlist[i].value.cp, (void *)rp->vset[j]->vlist[i].value.pval->vbuf, need);
				np->info->ivlist[i].vlen = need;
				break;

			    case PM_TYPE_AGGREGATE:
			    case PM_TYPE_AGGREGATE_STATIC:
				if ((np->info->ivlist[i].value.vp = (void *)malloc(rp->vset[j]->vlist[i].value.pval->vlen)) == NULL) {
				    __pmNoMem("eval_expr: aggregate value", rp->vset[j]->vlist[i].value.pval->vlen, PM_FATAL_ERR);
				    /*NOTREACHED*/
				}
				memcpy(np->info->ivlist[i].value.vp, (void *)rp->vset[j]->vlist[i].value.pval->vbuf, rp->vset[j]->vlist[i].value.pval->vlen-PM_VAL_HDR_SIZE);
				np->info->ivlist[i].vlen = rp->vset[j]->vlist[i].value.pval->vlen-PM_VAL_HDR_SIZE;
				break;

			    default:
				/*
				 * really only PM_TYPE_NOSUPPORT should
				 * end up here
				 */
				return PM_ERR_APPVERSION;
			}
		    }
		    return np->info->numval;
		}
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_DERIVE) {
		fprintf(stderr, "eval_expr: botch: operand %s not in the extended pmResult\n", pmIDStr(np->info->pmid));
		__pmDumpResult(stderr, rp);
	    }
#endif
	    return PM_ERR_PMID;

	default:
	    src = NULL;
	    if (np->left == NULL) {
		/* copy right pmValues */
		src = np->right;
	    }
	    else if (np->right == NULL) {
		/* copy left pmValues */
		src = np->left;
	    }
	    if (src != NULL) {
		np->info->numval = src->info->numval;
		np->info->iv_alloc = 0;
		np->info->ivlist = src->info->ivlist;
		return np->info->numval;
	    }
	    else {
		free_ivlist(np);
		/* TODO arithmetic ... */
		return PM_ERR_GENERIC;
	    }
    }
    /*NOTREACHED*/
}

/*
 * Algorithm here is complicated by trying to re-write the pmResult.
 *
 * On entry the pmResult is likely to be built over a pinned PDU buffer,
 * which means individual pmValueSets cannot be selectively replaced
 * (this would come to tears badly in pmFreeResult() where as soon as
 * one pmValueSet is found to be in a pinned PDU buffer it is assumed
 * they are all so ... leaving a memory leak for any ones we'd modified
 * here).
 *
 * So the only option is to COPY the pmResult, selectively replacing
 * the pmValueSets for the derived metrics, and then calling
 * pmFreeResult() to free the input structure and return the new one.
 *
 * In making the COPY it is critical that we reverse the algorithm
 * used in pmFreeResult() so that a later call to pmFreeResult() will
 * not cause a memory leak.
 * This means ...
 * - malloc() the pmResult (padded out to the right number of vset[]
 *   entries)
 * - if valfmt is not PM_VAL_INSITU use PM_VAL_DPTR (not PM_VAL_SPTR),
 *   so anything we point to is going to be released when our caller
 *   calls pmFreeResult()
 * - if numval == 1,  use __pmPoolAlloc() for the pmValueSet;
 *   otherwise use one malloc() for each pmValueSet with vlist[] sized
 *   to be 0 if numval < 0 else numval
 * - pmValueBlocks for 64-bit integers, doubles or anything with a
 *   length equal to the size of a 64-bit integer are from
 *   __pmPoolAlloc(); otherwise pmValueBlocks are from malloc()
 *
 * For reference, the same logic appears in __pmLogFetchInterp() to
 * sythesize a pmResult there.
 */
void
__dmpostfetch(__pmContext *ctxp, pmResult **result)
{
    int		i;
    int		j;
    int		m;
    int		numval;
    size_t	need;
    int		rewrite;
    ctl_t	*cp = (ctl_t *)ctxp->c_dm;
    pmResult	*rp = *result;
    pmResult	*newrp;

    if (cp == NULL || cp->fetch_has_dm == 0) return;

    newrp = (pmResult *)malloc(sizeof(pmResult)+(cp->numpmid-1)*sizeof(pmValueSet *));
    if (newrp == NULL) {
	__pmNoMem("__dmpostfetch: newrp", sizeof(pmResult)+(cp->numpmid-1)*sizeof(pmValueSet *), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    newrp->timestamp = rp->timestamp;
    newrp->numpmid = cp->numpmid;

    for (j = 0; j < newrp->numpmid; j++) {
	numval = rp->vset[j]->numval;
	rewrite = 0;
	if (pmid_domain(rp->vset[j]->pmid) == DYNAMIC_PMID) {
	    for (m = 0; m < cp->nmetric; m++) {
		if (rp->vset[j]->pmid == cp->mlist[m].pmid) {
		    rewrite = 1;
		    if (cp->mlist[m].expr == NULL) {
			numval = PM_ERR_PMID;
		    }
		    else {
			int	k;
			numval = eval_expr(cp->mlist[m].expr, rp, 1);
    fprintf(stderr, "root node: numval=%d", cp->mlist[m].expr->info->numval);
    for (k = 0; k < cp->mlist[m].expr->info->numval; k++) {
	fprintf(stderr, " vset[%d]: inst=%d", k, cp->mlist[m].expr->info->ivlist[k].inst);
	if (cp->mlist[m].expr->desc.type == PM_TYPE_32)
	    fprintf(stderr, " l=%d", cp->mlist[m].expr->info->ivlist[k].value.l);
	else if (cp->mlist[m].expr->desc.type == PM_TYPE_U32)
	    fprintf(stderr, " u=%u", cp->mlist[m].expr->info->ivlist[k].value.ul);
	else if (cp->mlist[m].expr->desc.type == PM_TYPE_64)
	    fprintf(stderr, " ll=%lld", cp->mlist[m].expr->info->ivlist[k].value.ll);
	else if (cp->mlist[m].expr->desc.type == PM_TYPE_U64)
	    fprintf(stderr, " ul=%llu", cp->mlist[m].expr->info->ivlist[k].value.ull);
	else if (cp->mlist[m].expr->desc.type == PM_TYPE_FLOAT)
	    fprintf(stderr, " f=%f", (double)cp->mlist[m].expr->info->ivlist[k].value.f);
	else if (cp->mlist[m].expr->desc.type == PM_TYPE_DOUBLE)
	    fprintf(stderr, " d=%f", cp->mlist[m].expr->info->ivlist[k].value.f);
	else if (cp->mlist[m].expr->desc.type == PM_TYPE_STRING) {
	    fprintf(stderr, " cp=%s (len=%d)", cp->mlist[m].expr->info->ivlist[k].value.cp, cp->mlist[m].expr->info->ivlist[k].vlen);
	}
	else {
	    fprintf(stderr, " vp=%p (len=%d)", cp->mlist[m].expr->info->ivlist[k].value.vp, cp->mlist[m].expr->info->ivlist[k].vlen);
	}
	fputc('\n', stderr);
    }
		    }
		    break;
		}
	    }
	}

	if (numval < 0) {
	    /* only need pmid and numval */
	    need = sizeof(pmValueSet) - sizeof(pmValue);
	}
	else if (numval == 1) {
	    /* special case for single value */
	    newrp->vset[j] = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	    need = 0;
	}
	else {
	    /* already one pmValue in a pmValueSet */
	    need = sizeof(pmValueSet) + (numval - 1)*sizeof(pmValue);
	}
	if (need > 0) {
	    if ((newrp->vset[j] = (pmValueSet *)malloc(need)) == NULL) {
		__pmNoMem("__dmpostfetch: vset", need, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	}
	newrp->vset[j]->pmid = rp->vset[j]->pmid;
	newrp->vset[j]->numval = numval;
	if (numval < 0)
	    continue;

	if (rewrite) {
	    if (cp->mlist[m].expr->desc.type == PM_TYPE_32 ||
		cp->mlist[m].expr->desc.type == PM_TYPE_U32)
		newrp->vset[j]->valfmt = PM_VAL_INSITU;
	    else
		newrp->vset[j]->valfmt = PM_VAL_DPTR;
	    }
	else {
	    newrp->vset[j]->valfmt = rp->vset[j]->valfmt;
	}

	for (i = 0; i < numval; i++) {
	    pmValueBlock	*vp;

	    if (!rewrite) {
		newrp->vset[j]->vlist[i].inst = rp->vset[j]->vlist[i].inst;
		if (newrp->vset[j]->valfmt == PM_VAL_INSITU) {
		    newrp->vset[j]->vlist[i].value.lval = rp->vset[j]->vlist[i].value.lval;
		}
		else {
		    need = rp->vset[j]->vlist[i].value.pval->vlen;
		    if (need == PM_VAL_HDR_SIZE + sizeof(__int64_t))
			vp = (pmValueBlock *)__pmPoolAlloc(need);
		    else
			vp = (pmValueBlock *)malloc(need);
		    if (vp == NULL) {
			__pmNoMem("__dmpostfetch: copy value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    memcpy((void *)vp, (void *)rp->vset[j]->vlist[i].value.pval, need);
		    newrp->vset[j]->vlist[i].value.pval = vp;
		}
		continue;
	    }

	    newrp->vset[j]->vlist[i].inst = cp->mlist[m].expr->info->ivlist[i].inst;
	    switch (cp->mlist[m].expr->desc.type) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    newrp->vset[j]->vlist[i].value.lval = cp->mlist[m].expr->info->ivlist[i].value.l;
		    break;

		case PM_TYPE_64:
		case PM_TYPE_U64:
		    need = PM_VAL_HDR_SIZE + sizeof(__int64_t);
		    if ((vp = (pmValueBlock *)__pmPoolAlloc(need)) == NULL) {
			__pmNoMem("__dmpostfetch: 64-bit int value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = cp->mlist[m].expr->desc.type;
		    memcpy((void *)vp->vbuf, (void *)&cp->mlist[m].expr->info->ivlist[i].value.ll, sizeof(__int64_t));
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_FLOAT:
		    need = PM_VAL_HDR_SIZE + sizeof(float);
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			__pmNoMem("__dmpostfetch: float value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = PM_TYPE_FLOAT;
		    memcpy((void *)vp->vbuf, (void *)&cp->mlist[m].expr->info->ivlist[i].value.f, sizeof(float));
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_DOUBLE:
		    need = PM_VAL_HDR_SIZE + sizeof(double);
		    if ((vp = (pmValueBlock *)__pmPoolAlloc(need)) == NULL) {
			__pmNoMem("__dmpostfetch: double value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = PM_TYPE_DOUBLE;
		    memcpy((void *)vp->vbuf, (void *)&cp->mlist[m].expr->info->ivlist[i].value.f, sizeof(double));
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_STRING:
		case PM_TYPE_AGGREGATE:
		case PM_TYPE_AGGREGATE_STATIC:
		    need = PM_VAL_HDR_SIZE + cp->mlist[m].expr->info->ivlist[i].vlen;
		    if (need == PM_VAL_HDR_SIZE + sizeof(__int64_t))
			vp = (pmValueBlock *)__pmPoolAlloc(need);
		    else
			vp = (pmValueBlock *)malloc(need);
		    if (vp == NULL) {
			__pmNoMem("__dmpostfetch: string or aggregate value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = cp->mlist[m].expr->desc.type;
		    memcpy((void *)vp->vbuf, cp->mlist[m].expr->info->ivlist[i].value.vp, cp->mlist[m].expr->info->ivlist[i].vlen);
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		default:
		    /*
		     * really nothing should end up here ...
		     * do nothing as numval should have been < 0
		     */
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_DERIVE) {
			fprintf(stderr, "__dmpostfetch: botch: drived metric[%d]: operand %s has odd type (%d)\n", m, pmIDStr(rp->vset[j]->pmid), cp->mlist[m].expr->desc.type);
		    }
#endif
		    break;
	    }

	}
    }

    /*
     * cull the original pmResult and return the rewritten one
     */
    pmFreeResult(rp);
    *result = newrp;

    return;
}
