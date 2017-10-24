#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

/* === begin largely copied from samplepmda events.c === */

static int mydomain = 29;

static pmValueSet	vs;
static char		*ebuf;
static int		ebuflen;
static char		*eptr;
static char		*ebufend;
static pmEventArray	*eap;
static pmEventRecord	*erp;

static int
check_buf(int need)
{
    int		offset = eptr - ebuf;

    while (&eptr[need] >= ebufend) {
	ebuflen *= 2;
	if ((ebuf = (char *)realloc(ebuf, ebuflen)) == NULL)
	    return -errno;
	eptr = &ebuf[offset];
	ebufend = &ebuf[ebuflen-1];
	vs.vlist[0].value.pval = (pmValueBlock *)ebuf;
    }
    return 0;
}

static int
add_param(pmID pmid, int type, pmAtomValue *avp)
{
    int			need;		/* bytes in the buffer */
    int			vlen;		/* value only length */
    int			sts;
    pmEventParameter	*epp;
    void		*src;

    need = sizeof(pmEventParameter);
    switch (type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	    vlen = sizeof(avp->l);
	    need += vlen;
	    src = &avp->l;
	    break;
	case PM_TYPE_64:
	case PM_TYPE_U64:
	    vlen = sizeof(avp->ll);
	    need += vlen;
	    src = &avp->ll;
	    break;
	case PM_TYPE_FLOAT:
	    vlen = sizeof(avp->f);
	    need += vlen;
	    src = &avp->f;
	    break;
	case PM_TYPE_DOUBLE:
	    vlen = sizeof(avp->d);
	    need += vlen;
	    src = &avp->d;
	    break;
	case PM_TYPE_STRING:
	    vlen = strlen(avp->cp);
	    need += PM_PDU_SIZE_BYTES(vlen);
	    src = avp->cp;
	    break;
	case PM_TYPE_AGGREGATE:
	case PM_TYPE_AGGREGATE_STATIC:
	    vlen = avp->vbp->vlen - PM_VAL_HDR_SIZE;
	    need += PM_PDU_SIZE_BYTES(vlen);
	    src = avp->vbp->vbuf;
	    break;
	default:
	    fprintf(stderr, "add_parameter failed: bad type (%d)\n", type);
	    exit(1);
    }
    if ((sts = check_buf(need)) < 0) {
	fprintf(stderr, "add_parameter failed: %s\n", pmErrStr(sts));
	exit(1);
    }
    epp = (pmEventParameter *)eptr;
    epp->ep_pmid = pmid;
    epp->ep_len = PM_VAL_HDR_SIZE + vlen;
    epp->ep_type = type;
    memcpy((void *)(eptr + sizeof(pmEventParameter)), src, vlen);
    eptr += need;
    erp->er_nparams++;
    return 0;
}

static void
reset(void)
{
    eptr = ebuf;
    eap = (pmEventArray *)eptr;
    eap->ea_nrecords = 0;
    eptr += sizeof(pmEventArray) - sizeof(pmEventRecord);
    vs.numval = 1;
    vs.valfmt = PM_VAL_DPTR;
    vs.vlist[0].inst = PM_IN_NULL;
}

static int
add_record(struct timeval *tp, int flags)
{
    int				sts;

    if ((sts = check_buf(sizeof(pmEventRecord) - sizeof(pmEventParameter))) < 0) {
	fprintf(stderr, "add_record failed: %s\n", pmErrStr(sts));
	exit(1);
    }
    eap->ea_nrecords++;
    erp = (pmEventRecord *)eptr;
    erp->er_timestamp.tv_sec = (__int32_t)tp->tv_sec;
    erp->er_timestamp.tv_usec = (__int32_t)tp->tv_usec;
    erp->er_nparams = 0;
    erp->er_flags = flags;
    eptr += sizeof(pmEventRecord) - sizeof(pmEventParameter);
    return 0;
}
/* === end copied from samplepmda events.c === */

static void
dump(char *xpect)
{
    pmResult	**res;
    int		nmissed;
    int		nrecords;
    int		r;
    int		k;
    static pmID	pmid_flags = 0;
    static pmID	pmid_missed;

    fprintf(stderr, "Expecting ... %s\n", xpect);

    for (k = 0; k < vs.numval || vs.numval == PM_ERR_GENERIC; k++) {
	if (vs.vlist[k].inst != PM_IN_NULL)
	    fprintf(stderr, "[instance %d]\n", vs.vlist[k].inst);
	nrecords = pmUnpackEventRecords(&vs, k, &res);

	if (nrecords < 0) {
	    fprintf(stderr, "pmUnpackEventRecords: %s\n", pmErrStr(nrecords));
	    return;
	}

	/* lifted from pminfo.c */
	if (pmid_flags == 0) {
	    /*
	     * get PMID for event.flags and event.missed
	     * note that pmUnpackEventRecords() will have called
	     * __pmRegisterAnon(), so the anon metrics
	     * should now be in the PMNS
	     */
	    char	*name_flags = "event.flags";
	    char	*name_missed = "event.missed";
	    int	sts;
	    sts = pmLookupName(1, &name_flags, &pmid_flags);
	    if (sts < 0) {
		/* should not happen! */
		fprintf(stderr, "Warning: cannot get PMID for %s: %s\n", name_flags, pmErrStr(sts));
		/* avoid subsequent warnings ... */
		__pmid_int(&pmid_flags)->item = 1;
	    }
	    sts = pmLookupName(1, &name_missed, &pmid_missed);
	    if (sts < 0) {
		/* should not happen! */
		fprintf(stderr, "Warning: cannot get PMID for %s: %s\n", name_missed, pmErrStr(sts));
		/* avoid subsequent warnings ... */
		__pmid_int(&pmid_missed)->item = 1;
	    }
	}

	nmissed = 0;
	for (r = 0; r < nrecords; r++) {
	    if (res[r]->numpmid == 2 && res[r]->vset[0]->pmid == pmid_flags &&
		(res[r]->vset[0]->vlist[0].value.lval & PM_EVENT_FLAG_MISSED) &&
		res[r]->vset[1]->pmid == pmid_missed) {
		nmissed += res[r]->vset[1]->vlist[0].value.lval;
	    }
	}

	fprintf(stderr, "Array contains %d records and %d missed records\n", nrecords, nmissed);
	if (nrecords == 0)
	    continue;

	for (r = 0; r < nrecords; r++) {
	    fprintf(stderr, "pmResult[%d]\n", r);
	    __pmDumpResult(stderr, res[r]);
	}
	pmFreeEventResult(res);
    }
}

int
main(int argc, char **argv)
{
    int			errflag = 0;
    int			sts;
    int			c;
    struct timeval	stamp;
    pmAtomValue		atom;
    int			savelen;
    pmEventParameter	*epp;
/* === begin largely copied from samplepmda events.c === */
    static pmValueBlock	*aggr;
    static char		aggrval[] = { '\01', '\03', '\07', '\017', '\037', '\077', '\177', '\377' };
    pmID		pmid_array = PMDA_PMID(4095,0);	/* event.records */
    pmID		pmid_type = PMDA_PMID(0,127);	/* event.type */
    pmID		pmid_32 = PMDA_PMID(0,128);	/* event.param_32 */
    pmID		pmid_u32 = PMDA_PMID(0,129);	/* event.param_u32 */
    pmID		pmid_64 = PMDA_PMID(0,130);	/* event.param_64 */
    pmID		pmid_u64 = PMDA_PMID(0,131);	/* event.param_u64 */
    pmID		pmid_float = PMDA_PMID(0,132);	/* event.param_float */
    pmID		pmid_double = PMDA_PMID(0,133);	/* event.param_double */
    pmID		pmid_string = PMDA_PMID(0,134);	/* event.param_string */
    pmID		pmid_aggregate = PMDA_PMID(0,135);	/* event.param_string */

    /* first time, punt on a 512 byte buffer */
    ebuflen = 512;
    ebuf = eptr = (char *)malloc(ebuflen);
    if (ebuf == NULL) {
	fprintf(stderr, "initial ebuf malloc failed: %s\n", strerror(errno));
	exit(1);
    }
    ebufend = &ebuf[ebuflen-1];
    /*
     * also, fix the domain field in the event parameter PMIDs ...
     * note these PMIDs must match the corresponding metrics in
     * desctab[] and this cannot easily be done automatically
     */
    ((__pmID_int *)&pmid_array)->domain = mydomain;
    ((__pmID_int *)&pmid_type)->domain = mydomain;
    ((__pmID_int *)&pmid_32)->domain = mydomain;
    ((__pmID_int *)&pmid_u32)->domain = mydomain;
    ((__pmID_int *)&pmid_64)->domain = mydomain;
    ((__pmID_int *)&pmid_u64)->domain = mydomain;
    ((__pmID_int *)&pmid_float)->domain = mydomain;
    ((__pmID_int *)&pmid_double)->domain = mydomain;
    ((__pmID_int *)&pmid_string)->domain = mydomain;
    ((__pmID_int *)&pmid_aggregate)->domain = mydomain;
    /* build pmValueBlock for aggregate value */
    aggr = (pmValueBlock *)malloc(PM_VAL_HDR_SIZE + sizeof(aggrval));
    aggr->vtype = PM_TYPE_AGGREGATE;
    aggr->vlen = PM_VAL_HDR_SIZE + sizeof(aggrval);
    memcpy(aggr->vbuf, (void *)aggrval, sizeof(aggrval));

/* === end copied from samplepmda events.c === */

    vs.pmid = pmid_array;
    vs.numval = 1;
    vs.valfmt = PM_VAL_DPTR;
    vs.vlist[0].inst = PM_IN_NULL;
    vs.vlist[0].value.pval = (pmValueBlock *)ebuf;

    __pmSetProgname(argv[0]);


    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s ...\n", pmProgname);
	fprintf(stderr, "options:\n");
	fprintf(stderr, "-D debug\n");
	exit(1);
    }

    reset();
    gettimeofday(&stamp, NULL);
    /* rebase event records 10 secs in past, add 1 sec for each new record */
    stamp.tv_sec -= 10;

    eap->ea_type = 0;
    eap->ea_len = 0;
    dump("Unknown or illegal metric type");
    eap->ea_type = PM_TYPE_EVENT;

    eap->ea_nrecords = -1;
    eap->ea_len = eptr - ebuf;
    dump("Error - ea_nrecords < 0");
    eap->ea_nrecords = 0;

    eap->ea_len = PM_VAL_HDR_SIZE;
    dump("Error - vlen way too small");

    eap->ea_len = eptr - ebuf;
    dump("No records");

    add_record(&stamp, 0);
    stamp.tv_sec++;
    eap->ea_len = eptr - ebuf;
    dump("1 record, no params");

    reset();
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.ul = 1;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    eap->ea_len = eptr - ebuf;
    dump("1 record, u32 param = 1");

    reset();
    add_record(&stamp, 1);
    stamp.tv_sec++;
    atom.ul = 2;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    atom.ll = -3;
    add_param(pmid_64, PM_TYPE_64, &atom);
    eap->ea_len = eptr - ebuf;
    dump("1 record, u32 param = 2, u64 param = -3");
    eap->ea_len--;
    dump("Error - buffer overrun @ parameter");

    reset();
    add_record(&stamp, PM_EVENT_FLAG_MISSED);
    stamp.tv_sec++;
    erp->er_nparams = 3;
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.ul = 4;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    atom.ull = 5;
    add_param(pmid_u64, PM_TYPE_U64, &atom);
    atom.cp = "6";
    add_param(pmid_string, PM_TYPE_STRING, &atom);
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.ul = 7;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    atom.d = 8;
    add_param(pmid_double, PM_TYPE_DOUBLE, &atom);
    atom.d = -9;
    add_param(pmid_double, PM_TYPE_DOUBLE, &atom);
    add_record(&stamp, 2);
    stamp.tv_sec++;
    atom.ul = 10;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    atom.ull = 11;
    add_param(pmid_u64, PM_TYPE_U64, &atom);
    atom.cp = "twelve";
    add_param(pmid_string, PM_TYPE_STRING, &atom);
    atom.cp = "thirteen";
    add_param(pmid_string, PM_TYPE_STRING, &atom);
    atom.l = 14;
    add_param(pmid_32, PM_TYPE_32, &atom);
    atom.ul = 15;
    add_param(pmid_u32, PM_TYPE_U32, &atom);
    add_record(&stamp, PM_EVENT_FLAG_MISSED);
    stamp.tv_sec++;
    erp->er_nparams = 4;
    savelen = eptr - ebuf;
    add_record(&stamp,0);
    stamp.tv_sec++;
    atom.ul = 16;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    atom.f = -17;
    add_param(pmid_float, PM_TYPE_FLOAT, &atom);
    atom.vbp = aggr;
    add_param(pmid_aggregate, PM_TYPE_AGGREGATE, &atom);
    eap->ea_len = eptr - ebuf;
    dump("6 records, 7 missed [u32=4 u64=5 str=\"6\"][u32=7 d=8 d=-9][u32=10 u64=11 str=\"twelve\" str=\"thirteen\" 32=-14 u32=15][u32=16 f=-17 aggr=...]");

    eap->ea_len = savelen;
    dump("Error - buffer overrun @ record");

    reset();
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.ul = 1;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    eap->ea_len = eptr - ebuf;
    vs.numval = PM_ERR_GENERIC;
    dump("bad numval");

    reset();
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.ul = 1;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    eap->ea_len = eptr - ebuf;
    vs.valfmt = 42;
    dump("bad valfmt");

    reset();
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.ul = 1;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    eap->ea_len = eptr - ebuf;
    vs.vlist[0].inst = 42;
    dump("odd instance");

    reset();
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.vbp = aggr;
    epp = (pmEventParameter *)eptr;
    add_param(pmid_aggregate, PM_TYPE_AGGREGATE, &atom);
    epp->ep_type = PM_TYPE_EVENT;
    eap->ea_len = eptr - ebuf;
    dump("1 record, nested event type @ 1st parameter");

    reset();
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.ul = 18;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.ul = 19;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    atom.vbp = aggr;
    epp = (pmEventParameter *)eptr;
    add_param(pmid_aggregate, PM_TYPE_AGGREGATE, &atom);
    epp->ep_type = PM_TYPE_EVENT;
    atom.ul = 20;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.ul = 21;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    eap->ea_len = eptr - ebuf;
    dump("3 records, nested event type @ 2nd parameter of 2nd record");

    printf("\n");
    printf("__pmDumpEventRecords coverage test ...\n");
    reset();
    add_record(&stamp, 0);
    stamp.tv_sec++;
    atom.l = 22;
    add_param(pmid_type, PM_TYPE_32, &atom);
    atom.ul = 23;
    add_param(pmid_u32, PM_TYPE_U32, &atom);
    atom.ll = -24;
    add_param(pmid_64, PM_TYPE_64, &atom);
    atom.ull = 25;
    add_param(pmid_u64, PM_TYPE_U64, &atom);
    atom.f = -26;
    add_param(pmid_float, PM_TYPE_FLOAT, &atom);
    atom.d = 27;
    add_param(pmid_double, PM_TYPE_DOUBLE, &atom);
    atom.cp = "minus twenty-eight";
    add_param(pmid_string, PM_TYPE_STRING, &atom);
    atom.vbp = aggr;
    add_param(pmid_aggregate, PM_TYPE_AGGREGATE, &atom);
    add_param(pmid_aggregate, PM_TYPE_AGGREGATE_STATIC, &atom);
    epp = (pmEventParameter *)eptr;
    atom.l = 29;
    add_param(pmid_32, PM_TYPE_32, &atom);
    eap->ea_len = eptr - ebuf;
    pmSetDebug("fetch");
    dump("all good");
    epp->ep_type = PM_TYPE_UNKNOWN;
    __pmDumpEventRecords(stdout, &vs, 0);

    return 0;
}
