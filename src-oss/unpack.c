#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

/* === begin largely copied from samplepmda events.c === */

static int mydomain = 29;

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
	    vlen = 8;		/* hardcoded for aggr[] */
	    need += PM_PDU_SIZE_BYTES(vlen);
	    src = avp->cp;
	    break;
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
    eap->ea_nmissed = 0;
    eptr += sizeof(pmEventArray) - sizeof(pmEventRecord);
}

static int
add_record(struct timeval *tp)
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

    fprintf(stderr, "Expecting ... %s\n", xpect);

    nrecords = pmUnpackEventRecords((pmValueBlock *)eap, &res, &nmissed);

    if (nrecords < 0) {
	fprintf(stderr, "pmUnpackEventRecords: %s\n", pmErrStr(nrecords));
	return;
    }
    fprintf(stderr, "Array contains %d records and %d records were missed\n", nrecords, nmissed);
    if (nrecords == 0)
	return;

    for (r = 0; r < nrecords; r++) {
	fprintf(stderr, "pmResult[%d]\n", r);
	__pmDumpResult(stderr, res[r]);
    }
    pmFreeEventResult(res);
}

int
main(int argc, char **argv)
{
    int			errflag = 0;
    int			sts;
    char		c;
    struct timeval	stamp;
    pmAtomValue		atom;
    int			savelen;
/* === begin largely copied from samplepmda events.c === */
    static char		aggr[] = { '\01', '\03', '\07', '\017', '\037', '\077', '\177', '\377' };
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
    ((__pmID_int *)&pmid_type)->domain = mydomain;
    ((__pmID_int *)&pmid_32)->domain = mydomain;
    ((__pmID_int *)&pmid_u32)->domain = mydomain;
    ((__pmID_int *)&pmid_64)->domain = mydomain;
    ((__pmID_int *)&pmid_u64)->domain = mydomain;
    ((__pmID_int *)&pmid_float)->domain = mydomain;
    ((__pmID_int *)&pmid_double)->domain = mydomain;
    ((__pmID_int *)&pmid_string)->domain = mydomain;
    ((__pmID_int *)&pmid_aggregate)->domain = mydomain;
/* === end copied from samplepmda events.c === */

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
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

    eap->ea_nmissed = -1;
    dump("Error - ea_nmissed < 0");
    eap->ea_nmissed = 0;

    eap->ea_len = PM_VAL_HDR_SIZE;
    dump("Error - vlen way too small");

    eap->ea_len = eptr - ebuf;
    dump("No records");

    add_record(&stamp);
    stamp.tv_sec++;
    eap->ea_len = eptr - ebuf;
    dump("1 record, no params");

    reset();
    add_record(&stamp);
    stamp.tv_sec++;
    atom.ul = 1;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    eap->ea_len = eptr - ebuf;
    dump("1 record, u32 param = 1");

    reset();
    add_record(&stamp);
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
    add_record(&stamp);
    stamp.tv_sec++;
    atom.ul = 4;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    atom.ull = 5;
    add_param(pmid_u64, PM_TYPE_U64, &atom);
    atom.cp = "6";
    add_param(pmid_string, PM_TYPE_STRING, &atom);
    add_record(&stamp);
    stamp.tv_sec++;
    atom.ul = 7;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    atom.d = 8;
    add_param(pmid_double, PM_TYPE_DOUBLE, &atom);
    atom.d = -9;
    add_param(pmid_double, PM_TYPE_DOUBLE, &atom);
    add_record(&stamp);
    stamp.tv_sec++;
    atom.ul = 10;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    atom.ull = 11;
    add_param(pmid_u64, PM_TYPE_U64, &atom);
    atom.cp = "twelve";
    add_param(pmid_string, PM_TYPE_STRING, &atom);
    atom.cp = "thirteen";
    add_param(pmid_string, PM_TYPE_STRING, &atom);
    atom.l = -14;
    add_param(pmid_32, PM_TYPE_32, &atom);
    atom.ul = 15;
    add_param(pmid_u32, PM_TYPE_U32, &atom);
    savelen = eptr - ebuf;
    add_record(&stamp);
    stamp.tv_sec++;
    atom.ul = 16;
    add_param(pmid_type, PM_TYPE_U32, &atom);
    atom.f = -17;
    add_param(pmid_float, PM_TYPE_FLOAT, &atom);
    atom.vp = (void *)aggr;
    add_param(pmid_aggregate, PM_TYPE_AGGREGATE, &atom);
    eap->ea_len = eptr - ebuf;
    eap->ea_nmissed = 7;
    dump("4 records, 7 missed [u32=4 u64=5 str=\"6\"][u32=7 d=8 d=-9][u32=10 u64=11 str=\"twelve\" str=\"thirteen\" 32=-14 u32=15][u32=16 f=-17 aggr=...]");
    eap->ea_nmissed = 0;

    eap->ea_len = savelen;
    dump("Error - buffer overrun @ record");

    return 0;
}
