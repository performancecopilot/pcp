/*
 * Check size of structs that are parts of a PDU, archive on-disk
 * records or bitfields.
 *
 * Looking for unexpected compiler padding or struct element alignment.
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017-2018 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

/* from p_attr.c */
typedef struct {
    __pmPDUHdr	hdr;
    int		attr;	/* PCP_ATTR code (optional, can be zero) */
    char	value[sizeof(int)];
} attr_t;

/* from p_creds.c */
typedef struct {
    __pmPDUHdr	hdr;
    int		numcreds;
    __pmCred	credlist[1];
} creds_t;

/* from p_desc.c */
typedef struct {
    __pmPDUHdr	hdr;
    pmID	pmid;
} desc_req_t;
typedef struct {
    __pmPDUHdr	hdr;
    pmDesc	desc;
} desc_t;

/* from p_error.c */
typedef struct {
    __pmPDUHdr	hdr;
    int		code;		/* error code */
} p_error_t;
typedef struct {
    __pmPDUHdr	hdr;
    int		code;		/* error code */
    int		datum;		/* additional information */
} x_error_t;

/* from p_fetch.c */
typedef struct {
    __pmPDUHdr		hdr;
    int			ctxid;		/* context slot index from the client */
    pmTimeval      	when;		/* desired time */
    int			numpmid;	/* no. PMIDs to follow */
    pmID		pmidlist[1];	/* one or more */
} fetch_t;

/* from p_instance.c */
typedef struct {
    __pmPDUHdr		hdr;
    pmInDom		indom;
    pmTimeval		when;			/* desired time */
    int			inst;			/* may be PM_IN_NULL */
    int			namelen;		/* chars in name[], may be 0 */
    char		name[sizeof(int)];	/* may be missing */
} instance_req_t;
typedef struct {
    int		inst;			/* internal instance id */
    int		namelen;		/* chars in name[], may be 0 */
    char	name[sizeof(int)];	/* may be missing */
} instlist_t;
typedef struct {
    __pmPDUHdr	hdr;
    pmInDom	indom;
    int		numinst;	/* no. of elts to follow */
    __pmPDU	rest[1];	/* array of instlist_t */
} instance_t;

/* from p_label.c */
typedef struct {
    __pmPDUHdr	hdr;
    int		ident;		/* domain, PMID or pmInDom identifier */
    int		type;		/* context/domain/indom/cluster/item/insts */
} label_req_t;
typedef struct {
    int		inst;		/* instance identifier or PM_IN_NULL */
    int		nlabels;	/* number of labels or an error code */
    int		json;		/* offset to start of the JSON string */
    int		jsonlen;	/* length in bytes of the JSON string */
    pmLabel	labels[0];	/* zero or more label indices + flags */
} labelset_t;
typedef struct {
    __pmPDUHdr	hdr;
    int		ident;		/* domain, PMID or pmInDom identifier */
    int		type;		/* context/domain/indom/cluster/item/insts */
    int		padding;
    int		nsets;
    labelset_t	sets[1];
} labels_t;

/* from p_lcontrol.c */
typedef struct {
    pmID		v_pmid;
    int			v_numval;	/* no. of vlist els to follow */
    __pmValue_PDU	v_list[1];	/* one or more */
} lc_vlist_t;
typedef struct {
    __pmPDUHdr		c_hdr;
    int			c_control;	/* mandatory or advisory */
    int			c_state;	/* off, maybe or on */
    int			c_delta;	/* requested logging interval (msec) */
    int			c_numpmid;	/* no. of vlist_ts to follow */
    __pmPDU		c_data[1];	/* one or more */
} control_req_t;

/* from p_lrequest.c */
typedef struct {
    __pmPDUHdr	hdr;
    int		type;		/* notification type */
} notify_t;

/* from p_lstatus.c */
typedef struct {
    __pmPDUHdr		hdr;
    int                 pad;            /* force status to be double word aligned */
    __pmLoggerStatus	status;
} logstatus_t;

/* from p_pmns.c */
typedef struct {
    __pmPDUHdr   hdr;
    int		sts;      /* to encode status of pmns op */
    int		numids;
    pmID        idlist[1];
} idlist_t;
typedef struct {
    int namelen;
    char name[sizeof(__pmPDU)]; /* variable length */
} name_t;
typedef struct {
    int status;
    int namelen;
    char name[sizeof(__pmPDU)]; /* variable length */
} name_status_t;
typedef struct {
    __pmPDUHdr	hdr;
    int		nstrbytes; /* number of str bytes including null terminators */
    int 	numstatus; /* = 0 if there is no status to be encoded */
    int		numnames;
    __pmPDU	names[1]; /* list of variable length name_t or name_status_t */
} namelist_t;
typedef struct {
    __pmPDUHdr	hdr;
    int		subtype; 
    int		namelen;
    char	name[sizeof(int)];
} namereq_t;

/* from p_profile.c */
typedef struct {
    pmInDom	indom;
    int		state;		/* include/exclude */
    int		numinst;	/* no. of instances to follow */
    int		pad;		/* protocol backward compatibility */
} instprof_t;
typedef struct {
    __pmPDUHdr	hdr;
    int		ctxid;		/* context slot index from the client */
    int		g_state;	/* global include/exclude */
    int		numprof;	/* no. of elts to follow */
    int		pad;		/* protocol backward compatibility */
} profile_t;

/* from p_result.c */
typedef struct {
    pmID		pmid;
    int			numval;		/* no. of vlist els to follow, or error */
    int			valfmt;		/* insitu or pointer */
    __pmValue_PDU	vlist[1];	/* zero or more */
} vlist_t;
typedef struct {
    __pmPDUHdr		hdr;
    pmTimeval		timestamp;	/* when returned */
    int			numpmid;	/* no. of PMIDs to follow */
    __pmPDU		data[1];	/* zero or more */
} result_t;

/* from p_text.c */
typedef struct {
    __pmPDUHdr	hdr;
    int		ident;
    int		type;		/* one line or help, PMID or InDom */
} text_req_t;
typedef struct {
    __pmPDUHdr	hdr;
    int		ident;
    int		buflen;			/* no. of chars following */
    char	buffer[sizeof(int)];	/* desired text */
} text_t;


void
check(char *s, size_t sz, int expect)
{
    printf("%s: ", s);
    if (sz == expect)
	printf("OK\n");
    else
	printf("BAD - sizeof()=%d expect=%d\n", (int)sz, expect);
}

int
main(int argc, char **argv)
{
    pmSetProgname(argv[0]);

    if (argc != 1) {
	fprintf(stderr, "Usage: %s\n", pmGetProgname());
	exit(1);
    }

#define INT 4

    printf("basic units\n");
    check("int", sizeof(int), INT);

    printf("\npmapi.h\n");
    check("pmUnits", sizeof(pmUnits), INT);
    check("pmDesc", sizeof(pmDesc), 5*INT);
    check("pmValueBlock", sizeof(pmValueBlock), 2*INT);
    check("pmLabel", sizeof(pmLabel), 2*INT);
    check("pmTimeval", sizeof(pmTimeval), 2*INT);
    check("pmTimespec", sizeof(pmTimespec), 4*INT);
    check("pmEventParameter", sizeof(pmEventParameter), 2*INT);
    check("pmEventRecord", sizeof(pmEventRecord), 6*INT);
    check("pmHighResEventRecord", sizeof(pmHighResEventRecord), 8*INT);
    check("pmEventArray", sizeof(pmEventArray), 8*INT);
    check("pmHighResEventArray", sizeof(pmHighResEventArray), 10*INT);

    printf("\nlibpcp.h\n");
    check("__pmID_int", sizeof(__pmID_int), INT);
    check("__pmInDom_int", sizeof(__pmInDom_int), INT);
    check("__pmPDUInfo", sizeof(__pmPDUInfo), INT);
    check("__pmPDUHdr", sizeof(__pmPDUHdr), 3*INT);
    check("__pmCred", sizeof(__pmCred), INT);
    check("__pmVersionCred", sizeof(__pmVersionCred), INT);
    check("__pmValue_PDU", sizeof(__pmValue_PDU), 2*INT);
    check("__pmValueSet_PDU", sizeof(__pmValueSet_PDU), 5*INT);
    check("__pmLogHdr", sizeof(__pmLogHdr), 2*INT);
    check("__pmLogLabel", sizeof(__pmLogLabel), 5*INT+PM_LOG_MAXHOSTLEN+PM_TZ_MAXLEN);
    check("__pmLogTI", sizeof(__pmLogTI), 5*INT);
    check("__pmLoggerStatus", sizeof(__pmLoggerStatus), 10*INT+PM_LOG_MAXHOSTLEN+PM_LOG_MAXHOSTLEN+PM_TZ_MAXLEN+PM_TZ_MAXLEN);

    printf("\np_*.c files in libpcp\n");
    /* 3*INT for __pmPDUHdr, the PDU payload */
    check("attr_t", sizeof(attr_t), 3*INT+2*INT);
    check("creds_t", sizeof(creds_t), 3*INT+2*INT);
    check("desc_req_t", sizeof(desc_req_t), 3*INT+1*INT);
    check("desc_t", sizeof(desc_t), 3*INT+5*INT);
    check("p_error_t", sizeof(p_error_t), 3*INT+1*INT);
    check("x_error_t", sizeof(x_error_t), 3*INT+2*INT);
    check("fetch_t", sizeof(fetch_t), 3*INT+5*INT);
    check("instance_req_t", sizeof(instance_req_t), 3*INT+6*INT);
    check("instlist_t", sizeof(instlist_t), 3*INT);
    check("instance_t", sizeof(instance_t), 3*INT+3*INT);
    check("label_req_t", sizeof(label_req_t), 3*INT+2*INT);
    check("labelset_t", sizeof(labelset_t), 4*INT);
    check("labels_t", sizeof(labels_t), 3*INT+8*INT);
    check("lc_vlist_t", sizeof(lc_vlist_t), 4*INT);
    check("control_req_t", sizeof(control_req_t), 3*INT+5*INT);
    check("notify_t", sizeof(notify_t), 3*INT+1*INT);
    check("logstatus_t", sizeof(logstatus_t), 3*INT+1*INT+10*INT+PM_LOG_MAXHOSTLEN+PM_LOG_MAXHOSTLEN+PM_TZ_MAXLEN+PM_TZ_MAXLEN);
    check("idlist_t", sizeof(idlist_t), 3*INT+3*INT);
    check("name_t", sizeof(name_t), 2*INT);
    check("name_status_t", sizeof(name_status_t), 3*INT);
    check("namelist_t", sizeof(namelist_t), 3*INT+4*INT);
    check("namereq_t", sizeof(namereq_t), 3*INT+3*INT);
    check("instprof_t", sizeof(instprof_t), 4*INT);
    check("profile_t", sizeof(profile_t), 3*INT+4*INT);
    check("vlist_t", sizeof(vlist_t), 5*INT);
    check("result_t", sizeof(result_t), 3*INT+4*INT);
    check("text_req_t", sizeof(text_req_t), 3*INT+2*INT);
    check("text_t", sizeof(text_t), 3*INT+3*INT);

    return 0;
}
