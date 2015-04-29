/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * Mongo PDU conversion exerciser --
 * 	+ if run standalone uses Send and Recv down its own pipe.
 *	+ if run remotely uses pdu-server at other end to echo PDUs
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/trace.h>
#include <pcp/trace_dev.h>
#include <math.h>
#include "localconfig.h"

static int		fd[2];
static int		standalone = 1;
static int		e;
static pmID		pmidlist[6];

/*
 * warning:
 *	instlist[], namelist[] and statlist[] must have the same number
 *	of elements!
 */

static int		instlist[] = {
	1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024
};

static char		*namelist[] = {
    /* 1 */	"alpha",
    /* 2 */	"beta",
    /* 4 */	"gamma",
    /* 8 */	"long-non-greek-letter-name-in-the-middle",
    /* 16 */	"delta",
    /* 32 */	"epsilon",
    /* 64 */	"phi",
    /* 128 */	"mu",
    /* 256 */	"lambda",
    /* 512 */	"omega",
    /* 1024 */	"theta"
};

static int		statlist[] = {
	-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5
};

static struct {
    int		inst;
    char	*name;
} indomlist[] = {
    { 0xdead, NULL },
    { 0, "fisho" },
    { 0, NULL }
};

static pmDesc		desc = {
    0xdeadbeef, PM_TYPE_64, 0xface, PM_SEM_COUNTER,
	{ 1, -1 , 0, PM_SPACE_MBYTE, PM_TIME_HOUR, 0 }
};

__pmLoggerStatus	logstat;

static int		timeout = TIMEOUT_DEFAULT;
static int		iter = 5;
static int		pass;

static long
foorand(void)
{
    static long rn[] = {
	 851401618, 1804928587,  758783491,  959030623,  684387517, 1903590565, 
	  33463914, 1254324197,  342241519,  824023566, 1483920592,  126398554, 
	1932422408,  351212254,  341603450, 1144747756, 1297389768, 1251336619, 
	 579758561,  838545539,  630072926, 1594243340,  641078858,  162216788, 
	 869693583, 1841205112, 2022861571, 1423418003, 1817792895,    5916492, 
	 992951867, 1143741253, 1691952160,  570397931, 2110444415,  658816051, 
	1290326580, 1307206911,  456208845, 1902445306,  654246144,  326116574, 
	 725123414,  832100416, 1382141583, 1618243337, 1296255701, 1141662977, 
	 986468767, 1401208270,  702616199, 2032315143,  790359597, 2026989069, 
	  15952070, 1109389944,  585771160,   52182039, 1271212614,  440155785
    };
    static int cnum = sizeof(rn) / sizeof (long);
    static int cur=-1;


    cur = (cur+1) % cnum;
    return (rn[cur]);
}

static void
_z(void)
{
    int			fatal = 0;	/* exit if set */
    __pmPDU		*pb;
    __pmTracePDU	*tpb;
    int			i;
    int			j;
    int			k;
    int			n;
    pmID		pmid;
    pmResult		*rp = NULL;
    pmValueBlock	myvb;
    pmValueBlock	*gvbp;
    pmValueBlock	*xvbp;
    pmAtomValue		av;
    int			ident;
    int			type;
    char		*buffer;
    int			control;
    int			attr;
    int			rate;
    int			state;
    int			num;
    pmID		*pmidp;
    __pmInResult	inres;
    __pmInResult	*inresp;
    pmInDom		indom;
    int			inst;
    pmDesc		result_desc;
    pmDesc		*descp = &result_desc;
    int			ctxnum;
    __pmTimeval		now;
    __pmProfile		curprof;
    __pmInDomProfile	idp[2];
    __pmProfile		*profp;
    pmResult		*resp;
    int			code;
    int			nv;
    int			sav_nv;
    int			sav_np;
    int			sender;
    int			count;
    char		*vp;
    __pmCred		increds[1];
    __pmCred		*outcreds;
    char		*resname;
    char		**resnamelist;
    int			*resstatlist;
    __pmLoggerStatus	*lsp;
    double		pi = M_PI;
    char		mytag[10];
    /*
     * use pid as "from" context for backwards compatibility to
     * keep QA tests happy, rather than FROM_ANON which would be
     * the more normal value for this usage.
     */
    pid_t		mypid = getpid();

/* PDU_ERROR */
    for (i = -1; i < 2; i += 2) {
	if ((e = __pmSendError(fd[1], mypid, i * PM_ERR_GENERIC)) < 0) {
	    fprintf(stderr, "Error: SendError: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
		fprintf(stderr, "Error: RecvError: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else if (e == 0) {
		fprintf(stderr, "Error: RecvError: end-of-file!\n");
		fatal = 1;
		goto cleanup;
	    }
	    else if (e != PDU_ERROR) {
		fprintf(stderr, "Error: RecvError: %s wrong type PDU!\n", __pmPDUTypeStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		__pmOverrideLastFd(PDU_OVERRIDE2);
		if ((e = __pmDecodeError(pb, &code)) < 0) {
		    fprintf(stderr, "Error: DecodeError: %s\n", pmErrStr(e));
		    fatal = 1;
		    goto cleanup;
		}
		else {
		    if (code != i * PM_ERR_GENERIC)
			fprintf(stderr, "Botch: Error: code: got: 0x%x expect: 0x%x\n",
			    code, i * PM_ERR_GENERIC);
		}
	    }
	}
    }

/* PDU_RESULT */
    resp = NULL;
    num = 7;		/* the _maximum_ number of merics we have */
    rp = (pmResult *)malloc(sizeof(*rp) + (num -1)*sizeof(pmValueSet *));
    rp->numpmid = 0;
    rp->timestamp.tv_sec = 30 * 60 * 60;	/* 30 hrs after the epoch */
    rp->timestamp.tv_usec = 123456;		/* plus a gnat */
    /* singular instance, insitu value */
    rp->vset[0] = (pmValueSet *)malloc(sizeof(*rp->vset[0]));
    rp->numpmid = 1;
    rp->vset[0]->pmid = 0xdead;
    rp->vset[0]->numval = 1;
    rp->vset[0]->valfmt = PM_VAL_INSITU;
    rp->vset[0]->vlist[0].inst = PM_IN_NULL;
    rp->vset[0]->vlist[0].value.lval = 1234;
    /* 3 instances, all values insitu */
    rp->vset[1] = (pmValueSet *)malloc(sizeof(*rp->vset[1])+2*sizeof(pmValue));
    rp->numpmid = 2;
    rp->vset[1]->pmid = 0xbeef;
    rp->vset[1]->numval = 3;
    rp->vset[1]->valfmt = PM_VAL_INSITU;
    rp->vset[1]->vlist[0].inst = 2;
    rp->vset[1]->vlist[0].value.lval = 2345;
    rp->vset[1]->vlist[1].inst = 4;
    rp->vset[1]->vlist[1].value.lval = 3456;
    rp->vset[1]->vlist[2].inst = 8;
    rp->vset[1]->vlist[2].value.lval = 4567;
    /* singular instance, STRING value in pmValueBlock */
    rp->vset[2] = (pmValueSet *)malloc(sizeof(*rp->vset[0]));
    rp->numpmid = 3;
    rp->vset[2]->pmid = pmidlist[0];
    rp->vset[2]->numval = 1;
    rp->vset[2]->valfmt = PM_VAL_DPTR;
    rp->vset[2]->vlist[0].inst = PM_IN_NULL;
    rp->vset[2]->vlist[0].value.pval = &myvb;
    rp->vset[2]->vlist[0].value.pval->vtype = PM_TYPE_STRING;
    rp->vset[2]->vlist[0].value.pval->vlen = PM_VAL_HDR_SIZE + 2;
    av.cp = "0";
    rp->vset[2]->vlist[0].value.pval = NULL;
    if ((e = __pmStuffValue(&av, &rp->vset[2]->vlist[0], PM_TYPE_STRING)) < 0) {
	fprintf(stderr, "Error: __pmStuffValue vset[%d] PM_TYPE_STRING: %s\n", i, pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    rp->vset[2]->vlist[0].value.pval->vbuf[0] = '0' + pass;
    /* singular instance, U64 value in pmValueBlock */
    rp->vset[3] = (pmValueSet *)malloc(sizeof(*rp->vset[0]));
    rp->numpmid = 4;
    rp->vset[3]->pmid = pmidlist[1];
    rp->vset[3]->numval = 1;
    rp->vset[3]->vlist[0].inst = PM_IN_NULL;
    av.ull = 0x8765432112345678LL;
    if ((e = __pmStuffValue(&av, &rp->vset[3]->vlist[0], PM_TYPE_U64)) < 0) {
	fprintf(stderr, "Error: __pmStuffValue vset[%d] PM_TYPE_U64: %s\n", i, pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    rp->vset[3]->valfmt = e;
    /* singular instance, FLOAT value in pmValueBlock */
    rp->vset[4] = (pmValueSet *)malloc(sizeof(*rp->vset[0]));
    rp->numpmid = 5;
    rp->vset[4]->pmid = pmidlist[2];
    rp->vset[4]->numval = 1;
    rp->vset[4]->vlist[0].inst = PM_IN_NULL;
    av.f = 4.3E+21;
    if ((e = __pmStuffValue(&av, &rp->vset[4]->vlist[0], PM_TYPE_FLOAT)) < 0) {
	fprintf(stderr, "Error: __pmStuffValue vset[%d] PM_TYPE_FLOAT: %s\n", i, pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    rp->vset[4]->valfmt = e;
    /* singular instance, DOUBLE value in pmValueBlock */
    rp->vset[5] = (pmValueSet *)malloc(sizeof(*rp->vset[0]));
    rp->numpmid = 6;
    rp->vset[5]->pmid = pmidlist[3];
    rp->vset[5]->numval = 1;
    rp->vset[5]->vlist[0].inst = PM_IN_NULL;
    av.d = 4.56E+123;
    if ((e = __pmStuffValue(&av, &rp->vset[5]->vlist[0], PM_TYPE_DOUBLE)) < 0) {
	fprintf(stderr, "Error: __pmStuffValue vset[%d] PM_TYPE_DOUBLE: %s\n", i, pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    rp->vset[5]->valfmt = e;
    /* no values */
    rp->vset[6] = (pmValueSet *)malloc(sizeof(*rp->vset[0]));
    rp->numpmid = 7;
    rp->vset[6]->pmid = 0xdeadcafe;
    rp->vset[6]->numval = PM_ERR_GENERIC;

    /* done with setup, do it! */
    if ((e = __pmSendResult(fd[1], mypid, rp)) < 0) {
	fprintf(stderr, "Error: SendResult: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvResult: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvResult: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_RESULT) {
	    fprintf(stderr, "Error: RecvResult: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmDecodeResult(pb, &resp)) < 0) {
		fprintf(stderr, "Error: DecodeResult: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	}
    }
    if (resp->timestamp.tv_sec != rp->timestamp.tv_sec)
	fprintf(stderr, "Botch: Result: tv_sec: got: %d expect: %d\n",
	    (int)resp->timestamp.tv_sec, (int)rp->timestamp.tv_sec);
    if (resp->timestamp.tv_usec != rp->timestamp.tv_usec)
	fprintf(stderr, "Botch: Result: tv_usec: got: %d expect: %d\n",
	    (int)resp->timestamp.tv_usec, (int)rp->timestamp.tv_usec);
    if (resp->numpmid != rp->numpmid)
	fprintf(stderr, "Botch: Result: numpmid: got: %d expect: %d\n",
	    resp->numpmid, rp->numpmid);
    for (i = 0; i < rp->numpmid; i++) {
	if (resp->vset[i]->pmid != rp->vset[i]->pmid)
	    fprintf(stderr, "Botch: Result: vset[%d].pmid: got: 0x%x expect: 0x%x\n",
		i, resp->vset[i]->pmid, rp->vset[i]->pmid);
	if (resp->vset[i]->numval != rp->vset[i]->numval) {
	    fprintf(stderr, "Botch: Result: vset[%d].numval: got: %d expect: %d\n",
		i, resp->vset[i]->numval, rp->vset[i]->numval);
	    continue;
	}
	if (resp->vset[i]->numval < 0)
	    continue;
	if (resp->vset[i]->valfmt != rp->vset[i]->valfmt)
	    fprintf(stderr, "Botch: Result: vset[%d].valfmt: got: %d expect: %d\n",
		i, resp->vset[i]->valfmt, rp->vset[i]->valfmt);
	for (j = 0; j < rp->vset[i]->numval; j++) {
	    if (resp->vset[i]->vlist[j].inst != rp->vset[i]->vlist[j].inst)
		fprintf(stderr, "Botch: Result: vset[%d][%d].inst: got: %d expect: %d\n",
		    i, j, resp->vset[i]->vlist[j].inst,
		    rp->vset[i]->vlist[j].inst);
	    if (resp->vset[i]->valfmt != rp->vset[i]->valfmt)
		continue;
	    if (resp->vset[i]->valfmt == PM_VAL_INSITU) {
		if (resp->vset[i]->vlist[j].value.lval != rp->vset[i]->vlist[j].value.lval)
		    fprintf(stderr, "Botch: Result: vset[%d][%d].value.lval: got: %d expect: %d\n",
			i, j, resp->vset[i]->vlist[j].value.lval,
			rp->vset[i]->vlist[j].value.lval);
		continue;
	    }
	    /* NOT insitu */
	    gvbp = resp->vset[i]->vlist[j].value.pval;
	    xvbp = rp->vset[i]->vlist[j].value.pval;
	    if (gvbp->vlen != xvbp->vlen)
		fprintf(stderr, "Botch: Result: vset[%d][%d].value.pval->vlen: got %d expect %d\n",
		    i, j, gvbp->vlen, xvbp->vlen);
	    if (gvbp->vtype != xvbp->vtype) {
		fprintf(stderr, "Botch: Result: vset[%d][%d].value.pval->vtype: got %d expect %d\n",
		    i, j, gvbp->vtype, xvbp->vtype);
		continue;
	    }
	    switch (gvbp->vtype) {
		pmAtomValue	gav;
		pmAtomValue	xav;
		case PM_TYPE_STRING:
		    if (strncmp(gvbp->vbuf, xvbp->vbuf, gvbp->vlen - sizeof(int)) != 0)
			fprintf(stderr, "Botch: Result: vset[%d][%d].value.pval->vbuf: got \"%*.*s\" expect \"%*.*s\"\n",
			    i, j, gvbp->vlen, gvbp->vlen, gvbp->vbuf,
			    gvbp->vlen, gvbp->vlen, xvbp->vbuf);
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    memcpy(&gav.ull, gvbp->vbuf, sizeof(__uint64_t));
		    memcpy(&xav.ull, xvbp->vbuf, sizeof(__uint64_t));
		    if (gav.ull != xav.ull)
			fprintf(stderr, "Botch: Result: vset[%d][%d].value.pval->ull: got %lld expect %lld\n",
			    i, j, (long long)gav.ull, (long long)xav.ull);
		    break;
		case PM_TYPE_FLOAT:
		    memcpy(&gav.f, gvbp->vbuf, sizeof(float));
		    memcpy(&xav.f, xvbp->vbuf, sizeof(float));
		    if (gav.f != xav.f)
			fprintf(stderr, "Botch: Result: vset[%d][%d].value.pval->ull: got %e expect %e\n",
			    i, j, (double)gav.f, (double)xav.f);
		    break;
		case PM_TYPE_DOUBLE:
		    memcpy(&gav.d, gvbp->vbuf, sizeof(double));
		    memcpy(&xav.d, xvbp->vbuf, sizeof(double));
		    if (gav.d != xav.d)
			fprintf(stderr, "Botch: Result: vset[%d][%d].value.pval->ull: got %e expect %e\n",
			    i, j, gav.d, xav.d);
		    break;
	    }
	}
    }
    if (resp != NULL)
	pmFreeResult(resp);

/* PDU_PROFILE */
    n = sizeof(instlist) / sizeof(instlist[0]);
    curprof.state = PM_PROFILE_EXCLUDE;
    curprof.profile_len = 2;
    curprof.profile = idp;
    idp[0].indom = 0xdeadcafe;
    idp[0].state = PM_PROFILE_INCLUDE;
    idp[0].instances_len = 1 + (foorand() % n);
    idp[0].instances = instlist;
    idp[1].indom = 0xface;
    idp[1].state = PM_PROFILE_EXCLUDE;
    idp[1].instances_len = 1 + (foorand() % n);
    idp[1].instances = &instlist[n - idp[1].instances_len];
    /* context no == 42 ... hack */
    if ((e = __pmSendProfile(fd[1], mypid, 42, &curprof)) < 0) {
	fprintf(stderr, "Error: SendProfile: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvProfile: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvProfile: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_PROFILE) {
	    fprintf(stderr, "Error: RecvProfile: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    profp = NULL;
	    if ((e = __pmDecodeProfile(pb, &ctxnum, &profp)) < 0) {
		fprintf(stderr, "Error: DecodeProfile: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (ctxnum != 42)
		    fprintf(stderr, "Botch: Profile: ctxnum: got: %d expect: %d\n",
			ctxnum, 42);
		if (profp == NULL)
		    fprintf(stderr, "Botch: Profile: profp is NULL!\n");
		else {
		    if (profp->state != curprof.state)
			fprintf(stderr, "Botch: Profile: global state: got: %d expect: %d\n",
			    profp->state, curprof.state);
		    if (profp->profile_len != curprof.profile_len)
			fprintf(stderr, "Botch: Profile: profile_len: got: %d expect: %d\n",
			    profp->profile_len, curprof.profile_len);
		    if (profp->profile == NULL)
			fprintf(stderr, "Botch: Profile: profp->profile is NULL!\n");
		    else {
			for (i = 0; i < curprof.profile_len; i++) {
			    if (profp->profile[i].indom != curprof.profile[i].indom)
				fprintf(stderr, "Botch: Profile: [%d]indom: got: 0x%x expect: 0x%x\n",
				    i, profp->profile[i].indom, curprof.profile[i].indom);
			    if (profp->profile[i].state != curprof.profile[i].state)
				fprintf(stderr, "Botch: Profile: [%d]state: got: %d expect: %d\n",
				    i, profp->profile[i].state, curprof.profile[i].state);
			    if (profp->profile[i].instances_len != curprof.profile[i].instances_len)
				fprintf(stderr, "Botch: Profile: [%d]instances_len: got: %d expect: %d\n",
				    i, profp->profile[i].instances_len, curprof.profile[i].instances_len);
			    else if (profp->profile[i].instances == NULL)
				fprintf(stderr, "Botch: Profile: profp->profile[%d].instances is NULL!\n", i);
			    else {
				for (k = 0; k <curprof.profile[i].instances_len; k++) {
				    if (profp->profile[i].instances[k] != curprof.profile[i].instances[k])
					fprintf(stderr, "Botch: Profile: [%d]instances[%d]: got: %d expect: %d\n",
					    i, k, profp->profile[i].instances[k], curprof.profile[i].instances[k]);
				}
				free(profp->profile[i].instances);
			    }
			}
			free(profp->profile);
		    }
		    free(profp);
		}
	    }
	}
    }

/* PDU_FETCH */
    n = sizeof(pmidlist) / sizeof(pmidlist[0]);
    if (pass != 0)
	n = 1 + (foorand() % n);
    if ((e = __pmSendFetch(fd[1], mypid, 43, (__pmTimeval *)0, n, pmidlist)) < 0) {
	fprintf(stderr, "Error: SendFetch: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvFetch: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvFetch: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_FETCH) {
	    fprintf(stderr, "Error: RecvFetch: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmDecodeFetch(pb, &ctxnum, &now, &num, &pmidp)) < 0) {
		fprintf(stderr, "Error: DecodeFetch: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (ctxnum != 43)
		    fprintf(stderr, "Botch: Fetch: ctxnum: got: %d expect: %d\n",
			ctxnum, 43);
		if (num != n)
		    fprintf(stderr, "Botch: Fetch: num: got: %d expect: %d\n",
			num, n);
		else {
		    for (i = 0; i < num; i++) {
			if (pmidlist[i] != pmidp[i])
			    fprintf(stderr, "Botch: Fetch: pmidlist[%d]: got: 0x%x expect: 0x%x\n",
				i, pmidp[i], pmidlist[i]);
		    }
		}
		__pmUnpinPDUBuf(pmidp);
	    }
	}
    }

/* PDU_DESC_REQ */
    if ((e = __pmSendDescReq(fd[1], mypid, 0xdeadbeef)) < 0) {
	fprintf(stderr, "Error: SendDescReq: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvDescReq: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvDescReq: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_DESC_REQ) {
	    fprintf(stderr, "Error: RecvDescReq: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmDecodeDescReq(pb, &pmid)) < 0) {
		fprintf(stderr, "Error: DecodeDescReq: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (pmid != 0xdeadbeef)
		    fprintf(stderr, "Botch: DescReq: pmid: got: 0x%x expect: 0x%x\n",
			pmid, 0xdeadbeef);
	    }
	}
    }

/* PDU_DESC */
    if ((e = __pmSendDesc(fd[1], mypid, &desc)) < 0) {
	fprintf(stderr, "Error: SendDesc: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvDesc: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvDesc: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_DESC) {
	    fprintf(stderr, "Error: RecvDesc: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmDecodeDesc(pb, descp)) < 0) {
		fprintf(stderr, "Error: DecodeDesc: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (descp->pmid != desc.pmid)
		    fprintf(stderr, "Botch: Desc: pmid: got: 0x%x expect: 0x%x\n",
			descp->pmid, desc.pmid);
		if (descp->type != desc.type)
		    fprintf(stderr, "Botch: Desc: type: got: %d expect: %d\n",
			descp->type, desc.type);
		if (descp->indom != desc.indom)
		    fprintf(stderr, "Botch: Desc: indom: got: 0x%x expect: 0x%x\n",
			descp->indom, desc.indom);
		if (descp->sem != desc.sem)
		    fprintf(stderr, "Botch: Desc: sem: got: %d expect: %d\n",
			descp->sem, desc.sem);
		if (descp->units.dimSpace != desc.units.dimSpace)
		    fprintf(stderr, "Botch: Desc: dimSpace: got: %d expect: %d\n",
			descp->units.dimSpace, desc.units.dimSpace);
		if (descp->units.dimTime != desc.units.dimTime)
		    fprintf(stderr, "Botch: Desc: dimTime: got: %d expect: %d\n",
			descp->units.dimTime, desc.units.dimTime);
		if (descp->units.dimCount != desc.units.dimCount)
		    fprintf(stderr, "Botch: Desc: dimCount: got: %d expect: %d\n",
			descp->units.dimCount, desc.units.dimCount);
		if (descp->units.scaleSpace != desc.units.scaleSpace)
		    fprintf(stderr, "Botch: Desc: scaleSpace: got: %d expect: %d\n",
			descp->units.scaleSpace, desc.units.scaleSpace);
		if (descp->units.scaleTime != desc.units.scaleTime)
		    fprintf(stderr, "Botch: Desc: scaleTime: got: %d expect: %d\n",
			descp->units.scaleTime, desc.units.scaleTime);
		if (descp->units.scaleCount != desc.units.scaleCount)
		    fprintf(stderr, "Botch: Desc: scaleCount: got: %d expect: %d\n",
			descp->units.scaleCount, desc.units.scaleCount);
	    }
	}
    }

/* PDU_INSTANCE_REQ */
    n = sizeof(indomlist) / sizeof(indomlist[0]);
    if (pass != 0)
	n = 1 + (foorand() % n);
    now.tv_sec = 60 * 60 * 60;		/* 60 hrs after the epoch */
    now.tv_usec = 654321;		/* plus a gnat */
    for (i = 0; i < n; i++) {
	__pmTimeval	tmp;
	if ((e = __pmSendInstanceReq(fd[1], mypid, &now, 0xface, indomlist[i].inst, indomlist[i].name)) < 0) {
	    fprintf(stderr, "Error: SendInstanceReq: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
		fprintf(stderr, "Error: RecvInstanceReq: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else if (e == 0) {
		fprintf(stderr, "Error: RecvInstanceReq: end-of-file!\n");
		fatal = 1;
		goto cleanup;
	    }
	    else if (e != PDU_INSTANCE_REQ) {
		fprintf(stderr, "Error: RecvInstanceReq: %s wrong type PDU!\n", __pmPDUTypeStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if ((e = __pmDecodeInstanceReq(pb, &tmp, &indom, &inst, &resname)) < 0) {
		    fprintf(stderr, "Error: DecodeInstanceReq: %s\n", pmErrStr(e));
		    fatal = 1;
		    goto cleanup;
		}
		else {
		    if (tmp.tv_sec != now.tv_sec || tmp.tv_usec != now.tv_usec)
			fprintf(stderr, "Botch: InstanceReq: when: got: %d,%d expect: %d,%d\n",
			    tmp.tv_sec, tmp.tv_usec, now.tv_sec, now.tv_usec);
		    if (indom != 0xface)
			fprintf(stderr, "Botch: InstanceReq: indom: got: 0x%x expect: 0x%x\n",
			    indom, 0xface);
		    if (inst != indomlist[i].inst)
			fprintf(stderr, "Botch: InstanceReq: inst: got: %d expect: %d\n",
			    inst, indomlist[i].inst);
		    if (resname != (char *)0 && indomlist[i].name != (char *)0 &&
			strcmp(resname, indomlist[i].name) != 0)
			    fprintf(stderr, "Botch: InstanceReq: name: got: \"%s\" expect: \"%s\"\n",
				resname, indomlist[i].name);
		    if ((resname == (char *)0 || indomlist[i].name == 0) &&
			resname != indomlist[i].name)
			    fprintf(stderr, "Botch: InstanceReq: name: got: " PRINTF_P_PFX "%p expect: " PRINTF_P_PFX "%p\n",
				resname, indomlist[i].name);
		    if (resname != (char *)0)
			free(resname);
		}
	    }
	}
    }

/* PDU_INSTANCE */
    n = sizeof(instlist) / sizeof(instlist[0]);
    if (pass != 0)
	n = (foorand() % n);	/* zero is ok here */
    inres.indom = 0x1234;
    inres.numinst = n;
    for (k = 0; k < 3; k++) {

	if (k == 0) {
	    inres.instlist = instlist;
	    inres.namelist = namelist;
	}
	else if (k == 1) {
	    inres.instlist = NULL;
	    inres.namelist = namelist;
	}
	else {
	    inres.instlist = instlist;
	    inres.namelist = NULL;
	}
	inresp = NULL;
	if ((e = __pmSendInstance(fd[1], mypid, &inres)) < 0) {
	    fprintf(stderr, "Error: SendInstance: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
		fprintf(stderr, "Error: RecvInstance: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else if (e == 0) {
		fprintf(stderr, "Error: RecvInstance: end-of-file!\n");
		fatal = 1;
		goto cleanup;
	    }
	    else if (e != PDU_INSTANCE) {
		fprintf(stderr, "Error: RecvInstance: %s wrong type PDU!\n", __pmPDUTypeStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if ((e = __pmDecodeInstance(pb, &inresp)) < 0) {
		    fprintf(stderr, "Error: DecodeInstance: %s\n", pmErrStr(e));
		    fatal = 1;
		    goto cleanup;
		}
		else {
		    if (inresp->indom != inres.indom)
			fprintf(stderr, "Botch: Instance: indom: got: 0x%x expect: 0x%x\n",
			    inresp->indom, inres.indom);
		    if (inresp->numinst != inres.numinst)
			fprintf(stderr, "Botch: Instance: numinst: got: %d expect: %d\n",
			    inresp->numinst, inres.numinst);
		    else {
			for (i = 0; i < inres.numinst; i++) {
			    if (inres.instlist != NULL &&
				inresp->instlist[i] != inres.instlist[i])
				fprintf(stderr, "Botch: Instance: instlist[%d]: got: %d expect: %d\n",
				    i, inresp->instlist[i], inres.instlist[i]);
			    if (inres.namelist != NULL &&
				strcmp(inresp->namelist[i], inres.namelist[i]) != 0)
				fprintf(stderr, "Botch: Instance: namelist[%d]: got: \"%s\" expect: \"%s\"\n",
				    i, inresp->namelist[i], inres.namelist[i]);
			}
		    }
		}
	    }
	}
	if (inresp != NULL)
	    __pmFreeInResult(inresp);
    }

/* PDU_TEXT_REQ */
    if ((e = __pmSendTextReq(fd[1], mypid, 0x12341234, PM_TEXT_PMID|PM_TEXT_ONELINE)) < 0) {
	fprintf(stderr, "Error: SendTextReq: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvTextReq: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvTextReq: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_TEXT_REQ) {
	    fprintf(stderr, "Error: RecvTextReq: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmDecodeTextReq(pb, &ident, &type)) < 0) {
		fprintf(stderr, "Error: DecodeTextReq: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (ident != 0x12341234)
		    fprintf(stderr, "Botch: TextReq: ident: got: 0x%x expect: 0x%x\n",
			ident, 0x12341234);
		if (type != (PM_TEXT_PMID|PM_TEXT_ONELINE))
		    fprintf(stderr, "Botch: TextReq: type: got: 0x%x expect: 0x%x\n",
			type, PM_TEXT_PMID|PM_TEXT_ONELINE);
	    }
	}
    }

/* PDU_TEXT */
#define MARY "mary had a little lamb\nits fleece was white as snow\n"
    if ((e = __pmSendText(fd[1], mypid, 0x43214321, MARY)) < 0) {
	fprintf(stderr, "Error: SendText: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvText: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvText: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_TEXT) {
	    fprintf(stderr, "Error: RecvText: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    buffer = NULL;
	    if ((e = __pmDecodeText(pb, &ident, &buffer)) < 0) {
		fprintf(stderr, "Error: DecodeText: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (ident != 0x43214321)
		    fprintf(stderr, "Botch: TextReq: ident: got: 0x%x expect: 0x%x\n",
			ident, 0x43214321);
		if (buffer == NULL)
		    fprintf(stderr, "Botch: TextReq: buffer is NULL!\n");
		else {
		    if (strcmp(buffer, MARY) != 0)
			fprintf(stderr, "Botch: Text: buffer: got: \"%s\" expect: \"%s\"\n",
			    buffer, MARY);
		    free(buffer);
		}
	    }
	}
    }

#if PCP_VER >= 3800
/* PDU_AUTH */
#define USERNAME "pcpqa"
    if ((e = __pmSendAuth(fd[1], mypid, PCP_ATTR_USERNAME, USERNAME, sizeof(USERNAME))) < 0) {
	fprintf(stderr, "Error: SendAuth: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvAuth: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvAuth: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_AUTH) {
	    fprintf(stderr, "Error: RecvAuth: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    buffer = NULL;
	    if ((e = __pmDecodeAuth(pb, &attr, &buffer, &count)) < 0) {
		fprintf(stderr, "Error: DecodeAuth: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (attr != PCP_ATTR_USERNAME)
		    fprintf(stderr, "Botch: AuthAttr: attr: got: 0x%x expect: 0x%x\n",
			attr, PCP_ATTR_USERNAME);
		if (count != sizeof(USERNAME))
		    fprintf(stderr, "Botch: AuthAttr: length: got: 0x%x expect: 0x%x\n",
			count, (int)sizeof(USERNAME));
		if (buffer == NULL)
		    fprintf(stderr, "Botch: AuthAttr: payload is NULL!\n");
		else {
		    if (strncmp(buffer, USERNAME, sizeof(USERNAME)) != 0)
			fprintf(stderr, "Botch: AuthAttr: payload: got: \"%s\" expect: \"%s\"\n",
			    buffer, USERNAME);
		}
	    }
	}
    }
#endif

/* PDU_CREDS */
    sender = 0;
    count = -1;
    outcreds = NULL;
    increds[0].c_type = CVERSION;
    increds[0].c_vala = (unsigned char)PDU_VERSION;
    increds[0].c_valb = (unsigned char)10;
    increds[0].c_valc = (unsigned char)11;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "0 = %x\n", *(unsigned int*)&(increds[0]));
    }
#endif
    if ((e = __pmSendCreds(fd[1], mypid, 1, increds)) < 0) {
	fprintf(stderr, "Error: SendCreds: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvCreds: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvCreds: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_CREDS) {
	    fprintf(stderr, "Error: RecvCreds: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    outcreds = NULL;
	    if ((e = __pmDecodeCreds(pb, &sender, &count, &outcreds)) < 0) {
		fprintf(stderr, "Error: DecodeCreds: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else if (outcreds == NULL)
		fprintf(stderr, "Botch: DecodeCreds: outcreds is NULL!\n");
	    else {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0) {
		    fprintf(stderr, "0 = %x\n", *(unsigned int*)&(outcreds[0]));
		}
#endif
		if (outcreds[0].c_type != CVERSION)
		    fprintf(stderr, "Botch: Creds: type: got: %x expect: %x\n",
			    (unsigned int)outcreds[0].c_type, (unsigned int)CVERSION);
		if ((outcreds[0].c_vala != (unsigned char)PDU_VERSION) ||
		    (outcreds[0].c_valb != (unsigned char)10) ||
		    (outcreds[0].c_valc != (unsigned char)11))
		    fprintf(stderr, "Botch: Creds: value mismatch (cred #0)\n");
		if (standalone && sender != mypid)
		    fprintf(stderr, "Botch: Creds: sender pid mismatch: got:%d expect:%" FMT_PID "\n",
			    sender, mypid);
		if (count != 1)
		    fprintf(stderr, "Botch: Creds: PDU count: got:%d expect:%d\n", count, 1);
		if (outcreds != NULL)
		    free(outcreds);
	    }
	}
    }

/* PDU_PMNS_IDS */
    n = sizeof(pmidlist) / sizeof(pmidlist[0]);
    if (pass != 0)
	n = 1 + (foorand() % n);
    if ((e = __pmSendIDList(fd[1], mypid, n, pmidlist, 43)) < 0) {
	fprintf(stderr, "Error: SendIDList: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvIDList: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvIDList: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_PMNS_IDS) {
	    fprintf(stderr, "Error: RecvIDList: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    pmID	mylist[6];
	    if ((e = __pmDecodeIDList(pb, n, mylist, &k)) < 0) {
		fprintf(stderr, "Error: DecodeIDList: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		for (i = 0; i < n; i++) {
		    if (pmidlist[i] != mylist[i])
			fprintf(stderr, "Botch: IDList: pmidlist[%d]: got: 0x%x expect: 0x%x\n",
			    i, mylist[i], pmidlist[i]);
		}
		if (k != 43)
		    fprintf(stderr, "Botch: IDList: sts: got: %d expect: %d\n",
			k, 43);
	    }
	}
    }

/* PDU_PMNS_NAMES */
    n = sizeof(namelist) / sizeof(namelist[0]);
    if (pass != 0)
	n = 1 + (foorand() % n);
    if ((e = __pmSendNameList(fd[1], mypid, n, namelist, statlist)) < 0) {
	fprintf(stderr, "Error: SendNameList: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvNameList: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvNameList: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_PMNS_NAMES) {
	    fprintf(stderr, "Error: RecvNameList: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    resnamelist = NULL;
	    resstatlist = NULL;
	    if ((e = __pmDecodeNameList(pb, &num, &resnamelist, &resstatlist)) < 0) {
		fprintf(stderr, "Error: DecodeNameList: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (num != n)
		    fprintf(stderr, "Botch: NameList: num: got: %d expect: %d\n",
			num, n);
		if (resnamelist == NULL)
		    fprintf(stderr, "Botch: NameList: resnamelist is NULL!\n");
		else if (resstatlist == NULL)
		    fprintf(stderr, "Botch: NameList: resstatlist is NULL!\n");
		else {
		    for (i = 0; i < num; i++) {
			if (strcmp(resnamelist[i], namelist[i]) != 0)
			    fprintf(stderr, "Botch: NameList: name[%d]: got: \"%s\" expect: \"%s\"\n",
				i, resnamelist[i], namelist[i]);
			if (resstatlist[i] != statlist[i])
			    fprintf(stderr, "Botch: NameList: stat[%d]: got: %d expect: %d\n",
				i, resstatlist[i], statlist[i]);
		    }
		    free(resnamelist);
		    free(resstatlist);
		}
	    }
	}
    }
    /* and again, with NULL statlist */
    if ((e = __pmSendNameList(fd[1], mypid, n, namelist, NULL)) < 0) {
	fprintf(stderr, "Error: SendNameList-2: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvNameList-2: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvNameList-2: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_PMNS_NAMES) {
	    fprintf(stderr, "Error: RecvNameList-2: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    resnamelist = NULL;
	    resstatlist = NULL;
	    if ((e = __pmDecodeNameList(pb, &num, &resnamelist, &resstatlist)) < 0) {
		fprintf(stderr, "Error: DecodeNameList-2: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (num != n)
		    fprintf(stderr, "Botch: NameList-2: num: got: %d expect: %d\n",
			num, n);
		if (resstatlist != NULL) {
		    fprintf(stderr, "Botch: NameList-2: resstatlist is NOT NULL!\n");
		    free(resstatlist);
		}
		if (resnamelist == NULL)
		    fprintf(stderr, "Botch: NameList-2: resnamelist is NULL!\n");
		else {
		    for (i = 0; i < num; i++) {
			if (strcmp(resnamelist[i], namelist[i]) != 0)
			    fprintf(stderr, "Botch: NameList-2: name[%d]: got: \"%s\" expect: \"%s\"\n",
				i, resnamelist[i], namelist[i]);
		    }
		    free(resnamelist);
		}
	    }
	}
    }

/* PDU_PMNS_CHILD */
    if ((e = __pmSendChildReq(fd[1], mypid, "mumble.fumble", 1)) < 0) {
	fprintf(stderr, "Error: SendChildReq: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvChildReq: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvChildReq: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_PMNS_CHILD) {
	    fprintf(stderr, "Error: RecvChildReq: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    resname = NULL;
	    if ((e = __pmDecodeChildReq(pb, &resname, &k)) < 0) {
		fprintf(stderr, "Error: DecodeChildReq: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (resname == NULL)
		    fprintf(stderr, "Botch: DecodeChildReq: resname is NULL!\n");
		else {
		    if (strcmp(resname, "mumble.fumble") != 0)
			fprintf(stderr, "Botch: DecodeChildReq: name: got: \"%s\" expect: \"%s\"\n",
			    resname, "mumble.fumble");
		    if (k != 1)
			fprintf(stderr, "Botch: DecodeChildReq: subtype: got: %d expect: %d\n",
			    k, 1);
		    free(resname);
		}
	    }
	}
    }

/* PDU_PMNS_TRAVERSE */
    if ((e = __pmSendTraversePMNSReq(fd[1], mypid, "foo.bar.snort")) < 0) {
	fprintf(stderr, "Error: SendTraversePMNSReq: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvTraversePMNSReq: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvTraversePMNSReq: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_PMNS_TRAVERSE) {
	    fprintf(stderr, "Error: RecvTraversePMNSReq: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    resname = NULL;
	    if ((e = __pmDecodeTraversePMNSReq(pb, &resname)) < 0) {
		fprintf(stderr, "Error: DecodeTraversePMNSReq: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (resname == NULL)
		    fprintf(stderr, "Botch: DecodeTraversePMNSReq: resname is NULL!\n");
		else {
		    if (strcmp(resname, "foo.bar.snort") != 0)
			fprintf(stderr, "Botch: DecodeTraversePMNSReq: name: got: \"%s\" expect: \"%s\"\n",
			    resname, "mumble.fumble");
		    free(resname);
		}
	    }
	}
    }

/* PDU_LOG_CONTROL */
    sav_nv = rp->vset[0]->numval;
    sav_np = rp->numpmid;
    for (nv = -1; nv < 2; nv++) {
	rp->numpmid = 2;	/* use only first 2 from PDU_RESULT above */
	rp->vset[0]->numval  = nv;
	if ((e = __pmSendLogControl(fd[1], rp, PM_LOG_MANDATORY, PM_LOG_MAYBE, 1000)) < 0) {
	    fprintf(stderr, "Error: SendLogControl: numval=%d %s\n", nv, pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
		fprintf(stderr, "Error: RecvLogControl: numval=%d %s\n", nv, pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else if (e == 0) {
		fprintf(stderr, "Error: RecvLogControl: end-of-file!\n");
		fatal = 1;
		goto cleanup;
	    }
	    else if (e != PDU_LOG_CONTROL) {
		fprintf(stderr, "Error: RecvLogControl: numval=%d %s wrong type PDU!\n", nv, __pmPDUTypeStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if ((e = __pmDecodeLogControl(pb, &resp, &control, &state, &rate)) < 0) {
		    fprintf(stderr, "Error: DecodeLogControl: numval=%d %s\n", nv, pmErrStr(e));
		    fatal = 1;
		    goto cleanup;
		}
		else {
		    if (state != PM_LOG_MAYBE)
			fprintf(stderr, "Botch: LogControl: numval=%d state: got: %d expect: %d\n",
			    nv, state, PM_LOG_MAYBE);
		    if (control != PM_LOG_MANDATORY)
			fprintf(stderr, "Botch: LogControl: numval=%d control: got: %d expect: %d\n",
			    nv, control, PM_LOG_MANDATORY);
		    if (rate != 1000)
			fprintf(stderr, "Botch: LogControl: numval=%d rate: got: %d expect: %d\n",
			    nv, rate, 1000);
		    if (resp->numpmid != rp->numpmid)
			fprintf(stderr, "Botch: LogControl: numval=%d numpmid: got: %d expect: %d\n",
			    nv, resp->numpmid, rp->numpmid);
		    else {
			for (i = 0; i < rp->numpmid; i++) {
			    if (resp->vset[i]->pmid != rp->vset[i]->pmid)
				fprintf(stderr, "Botch: LogControl: numval=%d vset[%d].pmid: got: 0x%x expect: 0x%x\n",
				    nv, i, resp->vset[i]->pmid, rp->vset[i]->pmid);
			    if (resp->vset[i]->valfmt != rp->vset[i]->valfmt)
				fprintf(stderr, "Botch: LogControl: numval=%d vset[%d].valfmt: got: %d expect: %d\n",
				    nv, i, resp->vset[i]->valfmt, rp->vset[i]->valfmt);
			    if (resp->vset[i]->numval != rp->vset[i]->numval)
				fprintf(stderr, "Botch: LogControl: numval=%d vset[%d].numval: got: %d expect: %d\n",
				    nv, i, resp->vset[i]->numval, rp->vset[i]->numval);
			    else {
				for (j = 0; j < rp->vset[i]->numval; j++) {
				    if (resp->vset[i]->vlist[j].inst != rp->vset[i]->vlist[j].inst)
					fprintf(stderr, "Botch: LogControl: numval=%d vset[%d][%d].inst: got: %d expect: %d\n",
					    nv, i, j, resp->vset[i]->vlist[j].inst, rp->vset[i]->vlist[j].inst);
				    if (resp->vset[i]->vlist[j].value.lval != rp->vset[i]->vlist[j].value.lval)
					fprintf(stderr, "Botch: LogControl: numval=%d vset[%d][%d].value.lval: got: %d expect: %d\n",
					    nv, i, j, resp->vset[i]->vlist[j].value.lval, rp->vset[i]->vlist[j].value.lval);
				}
			    }
			}
		    }
		    pmFreeResult(resp);
		}
	    }
	}
    }
    rp->vset[0]->numval = sav_nv;
    rp->numpmid = sav_np;

/* PDU_LOG_STATUS */
    logstat.ls_start.tv_sec = 13 * 60 * 60;	/* 13 hrs after the epoch */
    logstat.ls_start.tv_usec = 12345;		/* and a bit */
    logstat.ls_last.tv_sec = 13 * 60 * 60 + 10;
    logstat.ls_last.tv_usec = 23456;
    logstat.ls_timenow.tv_sec = 13 * 60 * 60 + 20;
    logstat.ls_timenow.tv_usec = 34567;
    logstat.ls_state = PM_LOG_MAYBE;
    logstat.ls_vol = 1;
    logstat.ls_size = 2048;
    strcpy(logstat.ls_hostname, "foo");
    strcpy(logstat.ls_fqdn, "foo.bar.com");
    strcpy(logstat.ls_tz, "TZ-THERE");
    strcpy(logstat.ls_tzlogger, "TZ-HERE");
    if ((e = __pmSendLogStatus(fd[1], &logstat)) < 0) {
	fprintf(stderr, "Error: SendLogStatus: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvLogStatus: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvLogStatus: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_LOG_STATUS) {
	    fprintf(stderr, "Error: RecvLogStatus: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmDecodeLogStatus(pb, &lsp)) < 0) {
		fprintf(stderr, "Error: DecodeLogStatus: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (lsp->ls_start.tv_sec != logstat.ls_start.tv_sec)
		    fprintf(stderr, "Botch: LogStatus: ls_staty.tv_sec: got: %d expect: %d\n",
			lsp->ls_start.tv_sec, logstat.ls_start.tv_sec);
		if (lsp->ls_start.tv_usec != logstat.ls_start.tv_usec)
		    fprintf(stderr, "Botch: LogStatus: ls_staty.tv_usec: got: %d expect: %d\n",
			lsp->ls_start.tv_usec, logstat.ls_start.tv_usec);
		if (lsp->ls_last.tv_sec != logstat.ls_last.tv_sec)
		    fprintf(stderr, "Botch: LogStatus: ls_staty.tv_sec: got: %d expect: %d\n",
			lsp->ls_last.tv_sec, logstat.ls_last.tv_sec);
		if (lsp->ls_last.tv_usec != logstat.ls_last.tv_usec)
		    fprintf(stderr, "Botch: LogStatus: ls_staty.tv_usec: got: %d expect: %d\n",
			lsp->ls_last.tv_usec, logstat.ls_last.tv_usec);
		if (lsp->ls_timenow.tv_sec != logstat.ls_timenow.tv_sec)
		    fprintf(stderr, "Botch: LogStatus: ls_staty.tv_sec: got: %d expect: %d\n",
			lsp->ls_timenow.tv_sec, logstat.ls_timenow.tv_sec);
		if (lsp->ls_timenow.tv_usec != logstat.ls_timenow.tv_usec)
		    fprintf(stderr, "Botch: LogStatus: ls_staty.tv_usec: got: %d expect: %d\n",
			lsp->ls_timenow.tv_usec, logstat.ls_timenow.tv_usec);
		if (lsp->ls_state != logstat.ls_state)
		    fprintf(stderr, "Botch: LogStatus: ls_state: got: 0x%x expect: 0x%x\n",
			lsp->ls_state, logstat.ls_state);
		if (lsp->ls_vol != logstat.ls_vol)
		    fprintf(stderr, "Botch: LogStatus: ls_vol: got: 0x%x expect: 0x%x\n",
			lsp->ls_vol, logstat.ls_vol);
		if (lsp->ls_size != logstat.ls_size)
		    fprintf(stderr, "Botch: LogStatus: ls_size: got: 0x%x expect: 0x%x\n",
			(int)lsp->ls_size, (int)logstat.ls_size);
		if (strcmp(lsp->ls_hostname, logstat.ls_hostname) != 0)
		    fprintf(stderr, "Botch: LogStatus: ls_hostname: got: \"%s\" expect: \"%s\"\n",
			lsp->ls_hostname, logstat.ls_hostname);
		if (strcmp(lsp->ls_fqdn, logstat.ls_fqdn) != 0)
		    fprintf(stderr, "Botch: LogStatus: ls_fqdn: got: \"%s\" expect: \"%s\"\n",
			lsp->ls_fqdn, logstat.ls_fqdn);
		if (strcmp(lsp->ls_tz, logstat.ls_tz) != 0)
		    fprintf(stderr, "Botch: LogStatus: ls_tz: got: \"%s\" expect: \"%s\"\n",
			lsp->ls_tz, logstat.ls_tz);
		if (strcmp(lsp->ls_tzlogger, logstat.ls_tzlogger) != 0)
		    fprintf(stderr, "Botch: LogStatus: ls_tzlogger: got: \"%s\" expect: \"%s\"\n",
			lsp->ls_tzlogger, logstat.ls_tzlogger);
		__pmUnpinPDUBuf(pb);
	    }
	}
    }

/* PDU_LOG_REQUEST */
    if ((e = __pmSendLogRequest(fd[1], LOG_REQUEST_SYNC)) < 0) {
	fprintf(stderr, "Error: SendLogRequest: %s\n", pmErrStr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmGetPDU(fd[0], ANY_SIZE, timeout, &pb)) < 0) {
	    fprintf(stderr, "Error: RecvLogRequest: %s\n", pmErrStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: RecvLogRequest: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != PDU_LOG_REQUEST) {
	    fprintf(stderr, "Error: RecvLogRequest: %s wrong type PDU!\n", __pmPDUTypeStr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmDecodeLogRequest(pb, &k)) < 0) {
		fprintf(stderr, "Error: DecodeLogRequest: %s\n", pmErrStr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (k != LOG_REQUEST_SYNC)
		    fprintf(stderr, "Botch: LogRequest: request: got: 0x%x expect: 0x%x\n",
			k, LOG_REQUEST_SYNC);
	    }
	}
    }

/* TRACE_PDU_ACK */
    if ((e = __pmtracesendack(fd[1], 0x41)) < 0) {
	fprintf(stderr, "Error: tracesendack: %s\n", pmtraceerrstr(e));
	fatal = 1;
	goto cleanup;
    }
    else {
	if ((e = __pmtracegetPDU(fd[0], TRACE_TIMEOUT_DEFAULT, &tpb)) < 0) {
	    fprintf(stderr, "Error: tracegetPDU: %s\n", pmtraceerrstr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else if (e == 0) {
	    fprintf(stderr, "Error: tracegetPDU: end-of-file!\n");
	    fatal = 1;
	    goto cleanup;
	}
	else if (e != TRACE_PDU_ACK) {
	    fprintf(stderr, "Error: tracegetPDU: 0x%x wrong type PDU!\n", e);
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmtracedecodeack(tpb, &k)) < 0) {
		fprintf(stderr, "Error: tracedecodeack: %s\n", pmtraceerrstr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if (k != 0x41)
		    fprintf(stderr, "Botch: traceack: request: got: 0x%x expect: 0x%x\n",
			k, 0x41);
	    }
	}
    }

/* TRACE_PDU_DATA */
    strcpy(mytag, "b1+b2");
    for (i = 5; i > 0; i--) {
	int proto;
	double	d;
	mytag[i] = '\0';
	if ((e = __pmtracesenddata(fd[1], mytag, i+1, TRACE_TYPE_OBSERVE, i * pi)) < 0) {
	    fprintf(stderr, "Error: tracesenddata: %s\n", pmtraceerrstr(e));
	    fatal = 1;
	    goto cleanup;
	}
	else {
	    if ((e = __pmtracegetPDU(fd[0], TRACE_TIMEOUT_DEFAULT, &tpb)) < 0) {
		fprintf(stderr, "Error: tracegetPDU: %s\n", pmtraceerrstr(e));
		fatal = 1;
		goto cleanup;
	    }
	    else if (e == 0) {
		fprintf(stderr, "Error: tracegetPDU: end-of-file!\n");
		fatal = 1;
		goto cleanup;
	    }
	    else if (e != TRACE_PDU_DATA) {
		fprintf(stderr, "Error: tracegetPDU: 0x%x wrong type PDU!\n", e);
		fatal = 1;
		goto cleanup;
	    }
	    else {
		if ((e = __pmtracedecodedata(tpb, &vp, &j, &k, &proto, &d)) < 0) {
		    fprintf(stderr, "Error: tracedecodedata: %s\n", pmtraceerrstr(e));
		    fatal = 1;
		    goto cleanup;
		}
		else {
		    if (strcmp(vp, mytag) != 0)
			fprintf(stderr, "Botch: tracedata: tag: got: \"%s\" expect: \"%s\"\n",
			    vp, mytag);
		    if (j != i+1)
			fprintf(stderr, "Botch: tracedata: taglen: got: 0x%x expect: 0x%x\n",
			    j, i);
		    if (k != TRACE_TYPE_OBSERVE)
			fprintf(stderr, "Botch: tracedata: type: got: 0x%x expect: 0x%x\n",
			    k, TRACE_TYPE_OBSERVE);
		    if (d != i * pi)
			fprintf(stderr, "Botch: tracedata: value: got: %g expect: %g\n",
			    d, i * pi);
		    free(vp);
		}
	    }
	}
    }

cleanup:

    /* done with rp by now */
    if (rp != NULL) {
	for (i = 0; i < rp->numpmid; i++) {
	    if (rp->vset[i]->numval > 0 && rp->vset[i]->valfmt == PM_VAL_DPTR) {
		for (j = 0; j < rp->vset[i]->numval; j++) {
		    if (rp->vset[i]->vlist[j].value.pval != NULL)
			free(rp->vset[i]->vlist[j].value.pval);
		}
	    }
	    free(rp->vset[i]);
	}
	free(rp);
    }

    if (fatal)
	exit(1);

}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		port = 4323;	/* default port for remote connection */
    char	*endnum;
    __pmID_int	*pmidp;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:i:Np:?")) != EOF) {
	switch (c) {
	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'i':	/* iterations */
	    iter = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -i requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 'N':	/* TIMEOUT_NEVER */
	    timeout = TIMEOUT_NEVER;
	    fprintf(stderr, "+ Using TIMEOUT_NEVER instead of TIMEOUT_DEFAULT +\n");
	    break;

	case 'p':	/* port */
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -p requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind < argc-1) {
	fprintf(stderr, "Usage: %s [-N] [-D n] [-i iter] [-p port] [host]\n", pmProgname);
	exit(1);
    }

    if ((e = pmLoadNameSpace(PM_NS_DEFAULT)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(e));
	exit(1);
    }

    if (optind == argc) {
	/* standalone, use a pipe */
	if (pipe(fd) < 0) {
	    perror("pipe");
	    exit(1);
	}
    }
    else {
	/* remote connection, use a TCP/IP socket */
	char *host = argv[optind];
	if ((e = __pmAuxConnectPMCDPort(host, port)) < 0) {
	    fprintf(stderr, "__pmAuxConnectPMCDPort(%s,%d): %s\n", host, port, pmErrStr(e));
	    exit(1);
	}
	fd[0] = fd[1] = e;
	standalone = 0;
    }

    if (__pmSetVersionIPC(fd[0], PDU_VERSION) < 0 ||
	__pmSetVersionIPC(fd[1], PDU_VERSION) < 0) {
	fprintf(stderr, "Error: __pmSetVersionIPC: %s\n", pmErrStr(-errno));
	exit(1);
    }

    pmidlist[0] = (pmID)0;
    pmidlist[1] = (pmID)0;
    pmidp = (__pmID_int *)&pmidlist[1];
    pmidp->domain = 123;
    pmidp->cluster = 456;
    pmidp->item = 789;
    pmidlist[2] = (pmID)0;
    pmidp = (__pmID_int *)&pmidlist[2];
    pmidp->domain = 255;
    pmidlist[3] = (pmID)0;
    pmidp = (__pmID_int *)&pmidlist[3];
    pmidp->cluster = 4095;
    pmidlist[4] = (pmID)0;
    pmidp = (__pmID_int *)&pmidlist[4];
    pmidp->item = 1023;
    pmidlist[5] = PM_ID_NULL;

    for (pass = 0; pass < iter; pass++) {
	fprintf(stderr, "+++++++++++++++++++++++++++\n");
	fprintf(stderr, "+ Mode: PDU_BINARY Pass %d +\n", pass);
	fprintf(stderr, "+++++++++++++++++++++++++++\n");

	_z();
    }

    if (standalone) {
	__pmCloseSocket(fd[0]);
	__pmCloseSocket(fd[1]);
    }

    pmUnloadNameSpace();

    exit(0);
}
