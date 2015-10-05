/*
 * Copyright (C) 2008 Silicon Graphics, Inc. All Rights Reserved.
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
 *
 * IB part of the PMDA - initialization, fetching etc.
 */
#include "ibpmda.h"
#include <infiniband/umad.h>
#include <infiniband/mad.h>
#include <ctype.h>

#define IBPMDA_MAX_HCAS (16)

typedef struct local_port_s {
	/*
	 * Cache the ca_name and portnum to avoid a bug in libibumad that
	 * leaks memory when umad_port_get() is called over and over.
	 * With ca_name and portnum we can safely do umad_port_release()
	 * first and then umad_port_get() without fear that some future
	 * version of release() will deallocate port->ca_name and
	 * port->portnum.
	 */
	char ca_name[UMAD_CA_NAME_LEN];
	int portnum;
	umad_port_t *ump;
	void * hndl;
	int needsupdate;
} local_port_t;

/* umad_ca_t starts with a name which is good enough for us to use */
typedef struct hca_state_s {
    umad_ca_t ca;
    local_port_t lports[UMAD_CA_MAX_PORTS];
} hca_state_t;

/* IB Architecture rel 1.2 demands that performance counters
 * must plateau once they reach 2^32. This structure is used
 * to track the counters and reset them when they get close to
 * the magic boundary */
typedef struct mad_counter_s {
	uint64_t accum; /* Accumulated value */
	uint32_t prev; /* Previous value of the counter */
	uint32_t cur; /* Current value, only valid during iteration */
	uint32_t isvalid; /* Is current value valid? */
} mad_counter_t;

typedef struct mad_cnt_desc_s {
	enum MAD_FIELDS madid; /* ID for the counter */
	char *name;
	int resetmask; /* Reset mask for port_performance_reset */
	uint32_t hiwat; /* If current value is over hiwat mark, reset it */
	int multiplier;
} mad_cnt_desc_t;

#define MADDESC_INIT(id, mask, shft, mul) \
	{IB_PC_##id##_F, #id, (1<<mask), (1U<<shft), mul}

/* note: order must match ibpmd_cndid definition */
static mad_cnt_desc_t mad_cnt_descriptors[] = {
    MADDESC_INIT(ERR_SYM,         0, 15, 1),
    MADDESC_INIT(LINK_RECOVERS,   1,  7, 1),
    MADDESC_INIT(LINK_DOWNED,     2,  7, 1),
    MADDESC_INIT(ERR_RCV,         3, 15, 1),
    MADDESC_INIT(ERR_PHYSRCV,     4, 15, 1),
    MADDESC_INIT(ERR_SWITCH_REL,  5, 15, 1),
    MADDESC_INIT(XMT_DISCARDS,	  6, 15, 1),
    MADDESC_INIT(ERR_XMTCONSTR,	  7,  7, 1),
    MADDESC_INIT(ERR_RCVCONSTR,	  8,  7, 1),
    MADDESC_INIT(ERR_LOCALINTEG,  9,  3, 1),
    MADDESC_INIT(ERR_EXCESS_OVR, 10,  3, 1),
    MADDESC_INIT(VL15_DROPPED,   11, 15, 1),
    MADDESC_INIT(XMT_BYTES,      12, 31, 4),
    MADDESC_INIT(RCV_BYTES,      13, 31, 4),
    MADDESC_INIT(XMT_PKTS,       14, 31, 1),
    MADDESC_INIT(RCV_PKTS,       15, 31, 1)
};

#undef MADDESC_INIT

static char *node_types[] = {"Unknown", "CA", "Switch", "Router", "iWARP RNIC"};

static char *port_states[] = {
    "Unknown",
    "Down",
    "Initializing",
    "Armed",
    "Active"
};

static char *port_phystates[] = {
    "No change",
    "Sleep",
    "Polling",
    "Disabled",
    "Port Configuration Training",
    "Link Up",
    "Error Recovery",
    "PHY Test"
};

/* Size is arbitrary, currently need 285 bytes for all caps */
#define IB_ALLPORTCAPSTRLEN 320

typedef struct port_state_s {
	ib_portid_t portid;
	local_port_t *lport;
	int needupdate;
	int validstate;
	int resetmask;
	int timeout;
	uint64_t guid;
	int remport;
	unsigned char perfdata[IB_MAD_SIZE];
	unsigned char portinfo[IB_MAD_SIZE];
	uint8_t switchperfdata[1024];
        mad_counter_t madcnts[ARRAYSZ(mad_cnt_descriptors)]; 
	char pcap[IB_ALLPORTCAPSTRLEN];
} port_state_t;

static char confpath[MAXPATHLEN];
static int portcount;
/* Line number while parsing the config file */
static FILE *fconf;
static int lcnt;

#define print_parse_err(loglevel, fmt, args...) \
    if (fconf) { \
	__pmNotifyErr(loglevel, "%s(%d): " fmt, confpath, lcnt, args); \
    } else { \
	__pmNotifyErr(loglevel, fmt, args); \
    }

static void
monitor_guid(pmdaIndom *itab, char *name, long long guid, int rport,
	    char *local, int lport)
{
    int inst;
    hca_state_t *hca = NULL;
    port_state_t *ps;
    
    if (pmdaCacheLookupName(itab[IB_HCA_INDOM].it_indom, local, NULL, 
			   (void**)&hca) != PMDA_CACHE_ACTIVE) {
	print_parse_err(LOG_ERR, "unknown HCA '%s' in 'via' clause\n", local);
	return;
    }

    if ((lport >= UMAD_CA_MAX_PORTS) || (lport < 0)) {
	print_parse_err(LOG_ERR, 
		       "port number %d is out of bounds for HCA %s\n",
			lport, local);
	return;
    }
    
    if (hca->lports[lport].hndl == NULL) {
	print_parse_err(LOG_ERR, 
		       "port %s:%d has failed initialization\n",
			local, lport);
	return;
    }
    
    if ((ps = (port_state_t *)calloc(1, sizeof(port_state_t))) == NULL) {
	__pmNotifyErr (LOG_ERR, "Out of memory to save state for %s\n", name);
	return;
    }

    ps->guid = guid;
    ps->remport = rport;
    ps->lport = hca->lports + lport;
    ps->portid.lid = -1;
    ps->timeout = 1000;

    if ((inst = pmdaCacheStore(itab[IB_PORT_INDOM].it_indom,
			    PMDA_CACHE_ADD, name, ps)) < 0) {
	__pmNotifyErr(LOG_ERR, "Cannot add %s to the cache - %s\n",
			name, pmErrStr(inst));
	free (ps);
	return;
    }

    portcount++;
}


static int
foreachport(hca_state_t *hst, void (*cb)(hca_state_t *, umad_port_t *, void *),
	    void *closure)
{
    int pcnt = hst->ca.numports;
    int p;
    int nports = 0;

    for (p=0; (pcnt >0) && (p < UMAD_CA_MAX_PORTS); p++) {
	umad_port_t *port = hst->ca.ports[p];

	if (port ) {
	    pcnt--;
	    nports++;
	    if (cb) {
		cb (hst, port, closure);
	    }
	}
    }
    return (nports);
}

#ifdef HAVE_NETWORK_BYTEORDER
#define guid_htonll(a) do { } while (0) /* noop */
#define guid_ntohll(a) do { } while (0) /* noop */
#else
static void
guid_htonll(char *p)
{
    char        c;
    int         i;

    for (i = 0; i < 4; i++) {
        c = p[i];
        p[i] = p[7-i];
        p[7-i] = c;
    }
}
#define guid_ntohll(v) guid_htonll(v)
#endif

static void
printportconfig (hca_state_t *hst, umad_port_t *port, void *arg)
{
    uint64_t hguid = port->port_guid;

    guid_ntohll((char *)&hguid);

    fprintf (fconf, "%s:%d 0x%llx %d via %s:%d\n",
	     port->ca_name, port->portnum, (unsigned long long)hguid,
	     port->portnum, hst->ca.ca_name, port->portnum);
}

static void
monitorport(hca_state_t *hst, umad_port_t *port, void *arg)
{
    pmdaIndom *itab = arg;
    uint64_t hguid = port->port_guid;
    char name[128];

    guid_ntohll((char *)&hguid);
    sprintf(name, "%s:%d", port->ca_name, port->portnum);

    monitor_guid(itab, name, hguid, port->portnum, port->ca_name, port->portnum);
}


static int mgmt_classes[] = {IB_SMI_CLASS, IB_SMI_DIRECT_CLASS, 
                             IB_SA_CLASS, IB_PERFORMANCE_CLASS};
static void
openumadport (hca_state_t *hst, umad_port_t *port, void *arg)
{
    void *hndl = arg;
    local_port_t *lp;

    if ((hndl = mad_rpc_open_port(port->ca_name, port->portnum, mgmt_classes,
                                  ARRAYSZ(mgmt_classes))) == NULL) {
	__pmNotifyErr(LOG_ERR, "Cannot open port handle for %s:%d\n",
			port->ca_name, port->portnum);
    }
    lp = &hst->lports[port->portnum];
    strcpy(lp->ca_name, port->ca_name);
    lp->portnum = port->portnum;
    lp->ump = port;
    lp->hndl = hndl;
}

static void
parse_config(pmdaIndom *itab)
{
    char buffer[2048];

    while ((fgets(buffer, sizeof(buffer)-1, fconf)) != NULL) {
	char *p;

	lcnt++;

	/* strip comments */
	if ((p = strchr(buffer,'#')))
	    *p='\0';

	for (p = buffer; *p; p++) {
	    if (!isspace (*p))
		break;
	}

	if (*p != '\0') {
	    char name[128];
	    long long guid;
	    int rport;
	    char local[128];
	    int lport;

	    if (sscanf(p, "%[^ \t]%llx%d via %[^:]:%d",
                       name, &guid, &rport, local, &lport) != 5) {
		__pmNotifyErr (LOG_ERR, "%s(%d): cannot parse the line\n",
			       confpath, lcnt);
		continue;
	    }
  
	    monitor_guid(itab, name, guid, rport, local, lport);
	}
    }
}

int
ib_load_config(const char *cp, int writeconf, pmdaIndom *itab, unsigned int nindoms)
{
    char hcas[IBPMDA_MAX_HCAS][UMAD_CA_NAME_LEN];
    hca_state_t *st = NULL;
    int i, n;
    int (*closef)(FILE *) = fclose;

    if (nindoms <= IB_CNT_INDOM)
	return -EINVAL;

    if (umad_init()) {
	__pmNotifyErr(LOG_ERR,
		"umad_init() failed.  No IB kernel support or incorrect ABI version\n");
	return -EIO;
    }

    if ((n = umad_get_cas_names(hcas, ARRAYSZ(hcas)))) {
	if ((st = calloc (n, sizeof(hca_state_t))) == NULL)
	    return -ENOMEM;
    } else
	/* No HCAs */
	return 0;

    /* Open config file - if the executable bit is set then assume that
     * user wants it to be a script and run it, otherwise try loading it.
     */
    strcpy(confpath, cp);
    if (access(confpath, F_OK) == 0) {
	if (writeconf) {
	    __pmNotifyErr(LOG_ERR,
		    "Config file exists and writeconf arg was given to pmdaib.  Aborting.");
	    exit(1);
	}

	if (access(confpath, X_OK)) {
	    /* Not an executable, just read it */
	    fconf = fopen (confpath, "r");
	} else {
	    fconf = popen(confpath, "r");
	    closef = pclose;
	} 
    } else if (writeconf) {
	fconf = fopen(confpath, "w");
    }
    /* else no config file: Just monitor local ports */

    for (i=0; i < n; i++) {
	if (umad_get_ca(hcas[i], &st[i].ca) == 0) {
	    int e = pmdaCacheStore(itab[IB_HCA_INDOM].it_indom, PMDA_CACHE_ADD,
				    st[i].ca.ca_name, &st[i].ca);

	    if (e < 0) {
		__pmNotifyErr(LOG_ERR, 
			"Cannot add instance for %s to the cache - %s\n",
			 st[i].ca.ca_name, pmErrStr(e));
		continue;
	    }

	    foreachport(st+i, openumadport, NULL);
	    if (fconf == NULL)
		/* No config file - monitor local ports */
		foreachport(st+i, monitorport, itab);
	    if (writeconf)
		foreachport(st+i, printportconfig, fconf);
	}
    }

    if (fconf) {
	parse_config(itab);
	(*closef)(fconf);
    }

    if (writeconf)
	/* Config file is now written.  Exit. */
	exit(0);

    if (!portcount) {
	__pmNotifyErr(LOG_INFO, "No IB ports found to monitor");
    }

    itab[IB_CNT_INDOM].it_set = (pmdaInstid *)calloc(ARRAYSZ(mad_cnt_descriptors), 
						     sizeof(pmdaInstid));

    if (itab[IB_CNT_INDOM].it_set == NULL) {
	return -ENOMEM;
    }

    itab[IB_CNT_INDOM].it_numinst = ARRAYSZ(mad_cnt_descriptors);
    for (i=0; i < ARRAYSZ(mad_cnt_descriptors); i++) {
	itab[IB_CNT_INDOM].it_set[i].i_inst = i;
	itab[IB_CNT_INDOM].it_set[i].i_name = mad_cnt_descriptors[i].name;

    }

    return 0;
}

static char *
ib_portcap_to_string(port_state_t *pst)
{
	static struct {
		int bit;
		const char *cap;
	} capdest [] = {
		{1, "SM"},
        	{2, "Notice"},
        	{3, "Trap"},
        	{5, "AutomaticMigration"},
        	{6, "SLMapping"},
        	{7, "MKeyNVRAM"},
        	{8, "PKeyNVRAM"},
        	{9, "LedInfo"},
        	{10, "SMdisabled"},
        	{11, "SystemImageGUID"},
        	{12, "PkeySwitchExternalPortTrap"},
        	{16, "CommunicatonManagement"},
        	{17, "SNMPTunneling"},
        	{18, "Reinit"},
        	{19, "DeviceManagement"},
        	{20, "VendorClass"},
        	{21, "DRNotice"},
        	{22, "CapabilityMaskNotice"},
        	{23, "BootManagement"},
        	{24, "IsLinkRoundTripLatency"},
        	{25, "ClientRegistration"}
	};
	char *comma = "";
	int commalen = 0;
	int i;
	char *ptr = pst->pcap;
	uint32_t bsiz = sizeof(pst->pcap);
	int pcap = mad_get_field(pst->portinfo, 0, IB_PORT_CAPMASK_F);

	*ptr ='\0';

	for (i=0; i < ARRAYSZ(capdest); i++) {
		if (pcap & (1<<capdest[i].bit)) {
			int sl = strlen(capdest[i].cap) + commalen;
			if (sl < bsiz) {
				sprintf (ptr, "%s%s", comma, capdest[i].cap);
				comma = ","; commalen=1;
				bsiz -= sl;
				ptr += sl;
			}
		}
	}

	return (pst->pcap);
}


/* This function can be called multiple times during single
 * fetch operation so take care to avoid side effects, for example,
 * if the "previous" value of the counter is above the high 
 * watermark and must be reset, don't change the previous value here - 
 * it  could lead to double counting on the second call */
static uint64_t
ib_update_perfcnt (port_state_t *pst, int udata, int *rv )
{
    mad_cnt_desc_t * md = mad_cnt_descriptors + udata;
    mad_counter_t *mcnt = pst->madcnts + udata;

    if (!mcnt->isvalid) {
    	uint32_t delta;

	mcnt->cur = mad_get_field(pst->perfdata, 0, md->madid);
	mcnt->isvalid = 1;

	/* If someone resets the counters, then don't update the the
	 * accumulated value because we don't know what was the value before it
	 * was reset. And if the difference between current and previous value
	 * is larger then the high watermark then don't update the accumulated
	 * value either - current value could've pegged because we didn't 
	 * fetch often enough */
	delta = mcnt->cur - mcnt->prev;
	if ((mcnt->cur < mcnt->prev) || (delta > md->hiwat)) { 
	    mcnt->isvalid = PM_ERR_VALUE;
	} else {
	    mcnt->accum += delta;
	}

	if (mcnt->cur > md->hiwat) {
	    pst->resetmask |= md->resetmask;
	}
    }

    *rv = mcnt->isvalid;
    return (mcnt->accum * md->multiplier);
}

static int
ib_linkwidth (port_state_t *pst)
{
    int w = mad_get_field(pst->portinfo, 0, IB_PORT_LINK_WIDTH_ACTIVE_F);

    switch (w) {
    case 1:
	return (1);
    case 2:
        return (4);
    case 4:
        return (8);
    case 8:
        return (12);
    }
    return (0);
}

int
ib_fetch_val(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmInDom_int *ind = (__pmInDom_int *)&(mdesc->m_desc.indom);
    __pmID_int	*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int	rv = 1; 
    port_state_t *pst = NULL;
    hca_state_t *hca = NULL;
    int umask = 1<<idp->cluster;
    int udata = (int)((__psint_t)mdesc->m_user);
    void *closure = NULL;
    int st;
    char *name = NULL;

    if (inst == PM_INDOM_NULL) {
	return PM_ERR_INST;
    }

    if (ind->serial != IB_CNT_INDOM) {
	if ((st = pmdaCacheLookup (mdesc->m_desc.indom, inst, &name,
				   &closure)) != PMDA_CACHE_ACTIVE) {
	    if (st == PMDA_CACHE_INACTIVE)
		st = PM_ERR_INST;
	    __pmNotifyErr (LOG_ERR, "Cannot find instance %d in indom %s: %s\n",
			   inst, pmInDomStr(mdesc->m_desc.indom), pmErrStr(st));
	    return st;
	}
    }

    /* If fetching from HCA indom, then no refreshing is necessary for the
     * lifetime of a pmda. Ports could change state, so some update could be
     * necessary */
    switch (ind->serial) {
    case IB_PORT_INDOM:
	if (idp->cluster > 3) {
	    return PM_ERR_INST;
	}

	pst = closure;

	if (pst->needupdate & umask) {
	    local_port_t *lp = pst->lport;

	    /* A port state is considered up-to date regardless of any
	     * errors which could happen later - this is used to implement
	     * one shot updates */
	    pst->needupdate ^= umask;

	    /* The state of the local port used for queries is checked
	     * once per fetch request */
	    if (lp->needsupdate) {
		umad_release_port(lp->ump);
		if (umad_get_port(lp->ca_name, lp->portnum, lp->ump) != 0) {
		    __pmNotifyErr (LOG_ERR, 
				   "Cannot get state of the port %s:%d\n",
				   lp->ump->ca_name, lp->ump->portnum);
		    return 0;
		}
		lp->needsupdate = 0;
	    }

	    /* If the port which we're supposed to use to query the data
	     * does not have a LID then we don't even try to query anything,
	     * is it going to fail anyway */
	    if (lp->ump->base_lid == 0) {
		return 0; /* No values available */
	    }

	    if (pst->portid.lid < 0) {
		ib_portid_t sm = {0};
		sm.lid = lp->ump->sm_lid;

		memset (&pst->portid, 0, sizeof (pst->portid));
		if (ib_resolve_guid_via (&pst->portid, &pst->guid, &sm,
					 pst->timeout, lp->hndl) < 0) {
			__pmNotifyErr (LOG_ERR, 
				       "Cannot resolve GUID 0x%llx for %s "
				       "via  %s:%d\n", 
					(unsigned long long)pst->guid,
					name, lp->ump->ca_name,
					lp->ump->portnum);
			pst->portid.lid = -1;
			return 0;
		}
	    }

	    switch (idp->cluster) {
	    case 0: /* port attributes */
		memset (pst->portinfo, 0, sizeof(pst->portinfo));
		if (!smp_query_via (pst->portinfo, &pst->portid,
				    IB_ATTR_PORT_INFO, 0, pst->timeout,
				    lp->hndl)) {
		    __pmNotifyErr (LOG_ERR,
				   "Cannot get port info for %s via %s:%d\n",
				   name, lp->ump->ca_name, lp->ump->portnum);
		    return 0;
		}
		break;

	    case 1: /* performance counters */
		/* I thought about updating all accumulating counters
		 * in case port_performance_query() succeeds but
		 * decided not to do it right now - updating all counters
		 * could mean more resets even in case when nobody is
		 * actually looking at the particular counter and I'm
		 * trying to minimize resets. */
		memset (pst->perfdata, 0, sizeof (pst->perfdata));
		if (!port_perf_query(pst->perfdata, &pst->portid,
				 pst->remport, pst->timeout, lp->hndl)) {
		    __pmNotifyErr (LOG_ERR,
				   "Cannot get performance counters for %s "
				   "via %s:%d\n",
				   name, lp->ump->ca_name, lp->ump->portnum);
		    return 0; 
		}
		break;

	    case 3: { /* switch performance counters */

#ifdef HAVE_PMA_QUERY_VIA

		// To find the LID of the switch the HCA is connected to,
		// send an SMP on the directed route 0,1 and ask the port
		// to identify itself.
		ib_portid_t sw_port_id = {
		    .drpath = {
			.cnt = 1,
			.p = { 0, 1, },
		    },
		};

		uint8_t sw_info[64];
		memset(sw_info, 0, sizeof(sw_info));
		if (!smp_query_via(sw_info, &sw_port_id, IB_ATTR_PORT_INFO, 0,
			pst->timeout, lp->hndl)) {
		    __pmNotifyErr(LOG_ERR, 
			    "Cannot get switch port info for %s via %s:%d.\n",
			    name, lp->ump->ca_name, lp->ump->portnum);
		    return 0;
		}

		int sw_lid, sw_port;
		mad_decode_field(sw_info, IB_PORT_LID_F, &sw_lid);
		mad_decode_field(sw_info, IB_PORT_LOCAL_PORT_F, &sw_port);

		sw_port_id.lid = sw_lid;

		// Query for the switch's performance counters' values.
		memset(pst->switchperfdata, 0, sizeof(pst->switchperfdata));
		if (!pma_query_via(pst->switchperfdata, &sw_port_id, sw_port,
			pst->timeout, IB_GSI_PORT_COUNTERS_EXT, lp->hndl)) {
		    __pmNotifyErr(LOG_ERR, 
			    "Cannot query performance counters of switch LID %d, port %d.\n",
			    sw_lid, sw_port);
		    return 0;
		}
#endif
		break;
	    }

	    }
	    pst->validstate ^= umask;
	} else if (!(pst->validstate & umask)) {
		/* We've hit an error on the previous update - continue
	         * reporting no data for this instance */
		return (0);
	}
	break;

    case IB_HCA_INDOM:
	hca = closure;
	break;

    case IB_CNT_INDOM:
	break;

    default:
	return (PM_ERR_INST);
    }

    switch (idp->cluster) {
    case 0: /* UMAD data - hca name, fw_version, number of ports etc */
	switch(idp->item) {
	case METRIC_ib_hca_hw_ver:
	    atom->cp = hca->ca.hw_ver;
	    break;

	case METRIC_ib_hca_system_guid:
	    atom->ull = hca->ca.system_guid;
	    break;

	case METRIC_ib_hca_node_guid:
	    atom->ull = hca->ca.node_guid;
	    break;

	case METRIC_ib_hca_numports:
	    atom->l = hca->ca.numports;
	    break;

	case METRIC_ib_hca_type:
	    if (hca->ca.node_type < ARRAYSZ(node_types)) {
	    	atom->cp = node_types[hca->ca.node_type];
	    } else {
		__pmNotifyErr (LOG_INFO, "Unknown node type %d for %s\n", 
			 hca->ca.node_type, hca->ca.ca_name);
		atom->cp = "Unknown";
	    }
	    break;

	case METRIC_ib_hca_fw_ver:
	    atom->cp = hca->ca.fw_ver;
	    break;

	case METRIC_ib_port_gid_prefix:
	    atom->ull = mad_get_field64(pst->portinfo, 0, IB_PORT_GID_PREFIX_F);
	    break;

	case METRIC_ib_port_rate:
	    atom->l = ib_linkwidth(pst) * 
		      (5 * mad_get_field (pst->portinfo, 0, 
					  IB_PORT_LINK_SPEED_ACTIVE_F))/2;
	    break;

	case METRIC_ib_port_lid:
	    atom->l = pst->portid.lid;
	    break;

	case METRIC_ib_port_capabilities:
	    atom->cp = ib_portcap_to_string(pst);
	    break;

	case METRIC_ib_port_phystate:
	    st = mad_get_field (pst->portinfo, 0, IB_PORT_PHYS_STATE_F);
	    if (st < ARRAYSZ(port_phystates)) {
	    	atom->cp = port_phystates[st];
	    } else {
		__pmNotifyErr (LOG_INFO, "Unknown port PHY state %d on %s\n",
			       st, name);
	        atom->cp = "Unknown";
	    }
	    break;

	case METRIC_ib_port_guid:
	    atom->ull = pst->guid;
	    break;

	case METRIC_ib_hca_ca_type:
	    atom->cp = hca->ca.ca_type;
	    break;

	case METRIC_ib_port_state:
	    st = mad_get_field (pst->portinfo, 0, IB_PORT_STATE_F);
	    if (st < ARRAYSZ(port_states)) {
	    	atom->cp = port_states[st];
	    } else {
		__pmNotifyErr (LOG_INFO, "Unknown port state %d on %s\n",
			       st, name);
	        atom->cp = "Unknown";
	    }
	    break;

	case METRIC_ib_port_linkspeed:
	    switch ((st = mad_get_field(pst->portinfo, 0,
				        IB_PORT_LINK_SPEED_ACTIVE_F))) {
	    case 1:
		atom->cp = "2.5 Gpbs";
		break;
	    case 2:
        	atom->cp = "5.0 Gbps";
		break;
	    case 4:
                atom->cp = "10.0 Gbps";
                break;
	    default:
		__pmNotifyErr (LOG_INFO, "Unknown link speed %d on %s\n",
				st, name);
                atom->cp  = "Unknown";
                break;
	    }
	    break;

	case METRIC_ib_port_linkwidth:
	    atom->l = ib_linkwidth(pst);
	    break;

	default:
	    rv = PM_ERR_PMID;
	    break;
	}
	break;

    case 1: /* Fetch values from mad rpc response */
        if ((udata >= 0) && (udata < ARRAYSZ(mad_cnt_descriptors))) {
	    /* If a metric has udata set then it's one of the "direct" 
	     * metrics - just update the accumulated counter
	     * and stuff its value into pmAtomValue */
	    switch (mdesc->m_desc.type) {
	    case PM_TYPE_32:
		atom->l = (int32_t)ib_update_perfcnt (pst, udata, &rv);
		break;
	    case PM_TYPE_64:
		atom->ll = ib_update_perfcnt (pst, udata, &rv);
		break;
	    default:
		rv = PM_ERR_INST;
		break;
	    }
	} else {
	    int rv1=0, rv2=0;
	    /* Synthetic metrics */
	    switch (idp->item) {
	    case METRIC_ib_port_total_bytes:
		atom->ll = ib_update_perfcnt (pst, IBPMDA_XMT_BYTES, &rv1)
			 + ib_update_perfcnt (pst, IBPMDA_RCV_BYTES, &rv2);
		break;

	    case METRIC_ib_port_total_packets:
		atom->ll = ib_update_perfcnt (pst, IBPMDA_XMT_PKTS, &rv1)
			 + ib_update_perfcnt (pst, IBPMDA_RCV_PKTS, &rv2);
		break;

	    case METRIC_ib_port_total_errors_drop:
		atom->l = (int)(ib_update_perfcnt (pst, IBPMDA_ERR_SWITCH_REL, &rv1) 
			+ ib_update_perfcnt (pst, IBPMDA_XMT_DISCARDS, &rv2));
		break;

	    case METRIC_ib_port_total_errors_filter:
		atom->l = (int)(ib_update_perfcnt (pst, IBPMDA_ERR_XMTCONSTR, &rv1) 
			+ ib_update_perfcnt (pst, IBPMDA_ERR_RCVCONSTR, &rv2));
		break;

	    default:
		rv = PM_ERR_PMID;
		break;
	    }

	    if ((rv1 < 0) || (rv2 < 0)) {
		rv = (rv1 < 0) ? rv1 : rv2;
	    }
	}
	break;

    case 2: /* Control structures */
	switch (idp->item) {
	case METRIC_ib_control_query_timeout:
	    atom->l = pst->timeout;
	    break;

	case METRIC_ib_control_hiwat:
	    if (inst < ARRAYSZ(mad_cnt_descriptors)) {
		atom->ul = mad_cnt_descriptors[inst].hiwat;
	    } else {
		rv = PM_ERR_INST;
	    }
	    break;

	default:
	    rv = PM_ERR_PMID;
	    break;
	}
	break;

    case 3: /* Fetch values from switch response */

#ifdef HAVE_PMA_QUERY_VIA

	// (The values are "swapped" because what the port receives is what the
	// switch sends, and vice versa.)
	switch (idp->item) {
	    case METRIC_ib_port_switch_in_bytes: {
		mad_decode_field(pst->switchperfdata, 
		    	IB_PC_EXT_XMT_BYTES_F, &atom->ull);
    		atom->ull *= 4; // TODO: programmatically determine link width
		break;
	    }
	    case METRIC_ib_port_switch_in_packets: {
		mad_decode_field(pst->switchperfdata, 
		    	IB_PC_EXT_XMT_PKTS_F, &atom->ull);
		break;
	    }
	    case METRIC_ib_port_switch_out_bytes: {
		mad_decode_field(pst->switchperfdata, 
		    	IB_PC_EXT_RCV_BYTES_F, &atom->ull);
    		atom->ull *= 4; // TODO: programmatically determine link width
		break;
	    }
	    case METRIC_ib_port_switch_out_packets: {
		mad_decode_field(pst->switchperfdata, 
		    	IB_PC_EXT_RCV_PKTS_F, &atom->ull);
		break;
	    }
	    case METRIC_ib_port_switch_total_bytes: {
	    	uint64_t sw_rx_bytes, sw_tx_bytes;
	    	int ib_lw;
		mad_decode_field(pst->switchperfdata, 
			IB_PC_EXT_RCV_BYTES_F, &sw_rx_bytes);
		mad_decode_field(pst->switchperfdata, 
			IB_PC_EXT_XMT_BYTES_F, &sw_tx_bytes);
		ib_lw = 4; // TODO: programmatically determine link width
		atom->ull = (sw_rx_bytes * ib_lw) + (sw_tx_bytes * ib_lw);
	    	break;
	    }
	    case METRIC_ib_port_switch_total_packets: {
	    	uint64_t sw_rx_packets, sw_tx_packets;
		mad_decode_field(pst->switchperfdata,
			IB_PC_EXT_RCV_PKTS_F, &sw_rx_packets);
		mad_decode_field(pst->switchperfdata,
			IB_PC_EXT_XMT_PKTS_F, &sw_tx_packets);
		atom->ull = sw_rx_packets + sw_tx_packets;
	    	break;
	    }
	    default: {
		rv = PM_ERR_PMID;
		break;
	    }
	}
#else

	return PM_ERR_VALUE;

#endif
	break;

    default:
	rv = PM_ERR_PMID;
	break;
    }

    return rv;
}

/* Walk the instances and arm needupdate flag in each instance's
 * state. The actuall updating is done in the fetch function */
void
ib_rearm_for_update(void *state)
{
    port_state_t *pst = state;

    pst->lport->needsupdate = 1;

    pst->needupdate = IB_PORTINFO_UPDATE | IB_HCA_PERF_UPDATE | IB_SWITCH_PERF_UPDATE;
    pst->validstate = 4; /* 0x4 for timeout which is always valid */
}

void
ib_reset_perfcounters (void *state)
{
    int m;
    port_state_t *pst = state;

    if (pst->resetmask && (pst->portid.lid != 0)) {
	memset (pst->perfdata, 0, sizeof (pst->perfdata));

	if (port_perf_reset(pst->perfdata, &pst->portid, pst->remport,
			pst->resetmask, pst->timeout, pst->lport->hndl)) {
	    int j;

	    for (j=0; j < ARRAYSZ(mad_cnt_descriptors); j++) {
		if (pst->resetmask & (1<<j)) {
		    pst->madcnts[j].prev = 0;
		    pst->madcnts[j].isvalid = 0;
		}
	    }
	}
    }
    pst->resetmask = 0;

    for (m=0; m < ARRAYSZ(mad_cnt_descriptors); m++) {
	if (pst->madcnts[m].isvalid) {
	    pst->madcnts[m].prev = pst->madcnts[m].cur;
	    pst->madcnts[m].isvalid = 0;
	}
    }
}

int
ib_store(pmResult *result, pmdaExt *pmda)
{
    int i;

    for (i = 0; i < result->numpmid ; i++) {
	pmValueSet *vs = result->vset[i];
	__pmID_int *pmidp = (__pmID_int *)&vs->pmid;
	int inst;

	if (pmidp->cluster != 2) {
	    return (-EACCES);
	}

	if (vs->valfmt != PM_VAL_INSITU) {
	    return (-EINVAL);
	}

	for (inst=0; inst < vs->numval; inst++) {
	    int id = vs->vlist[inst].inst;
	    void *closure = NULL;

	    switch (pmidp->item) {
	    case METRIC_ib_control_query_timeout:
		if (pmdaCacheLookup (pmda->e_indoms[IB_PORT_INDOM].it_indom,
				     id, NULL, &closure) == PMDA_CACHE_ACTIVE) {
		    port_state_t *pst = closure;
		    pst->timeout = vs->vlist[inst].value.lval;
		} else {
		    return (PM_ERR_INST);
		}
		break;

	    case METRIC_ib_control_hiwat:
		if ((id < 0) ||
		    (id > pmda->e_indoms[IB_CNT_INDOM].it_numinst)) {
		    return (PM_ERR_INST);
		} 

		mad_cnt_descriptors[id].hiwat = (uint32_t)vs->vlist[inst].value.lval;
		break;

	    default:
		return (-EACCES);
	    }
        }
    }
    return 0;
}

