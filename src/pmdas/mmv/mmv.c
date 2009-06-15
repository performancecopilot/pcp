/*
 * Copyright (c) 1995-2000,2009 Silicon Graphics, Inc. All Rights Reserved.
 * Copyright (c) 2009 Aconex. All Rights Reserved.
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
 *
 * MMV PMDA
 *
 * This PMDA uses specially formatted files in either /var/tmp/mmv or 
 * some other directory, as specified on the command line. Each file represent
 * a separate "cluster" of values with flat name structure for each cluster.
 * names for the metrics are prepended with mmv and name of the file.
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "mmv_stats.h"
#include "./domain.h"
#include <sys/stat.h>

static pmdaInterface	dispatch;

static pmdaMetric * metrics;
static int mcnt;
static pmdaIndom * indoms;
static int incnt;
static int reload;

static time_t conf_ts;		/* last mmv.conf timestamp */
static time_t statsdir_ts;	/* last statsdir timestamp */

static char *pcptmpdir;		/* probably /var/tmp */
static char *pcpvardir;		/* probably /var/pcp */
static char *pcppmdasdir;	/* probably /var/pcp/pmdas */
static char pmnsdir[MAXPATHLEN];   /* pcpvardir/pmns */
static char statsdir[MAXPATHLEN];   /* pcptmpdir/mmv */
static char confpath[MAXPATHLEN];   /* pcppmdasdir/mmv/mmv.conf */

static struct stats_s {
    char *	name;		/* strdup client name */
    void *	addr;		/* mmap */
    mmv_stats_hdr_t *	hdr;	/* header in mmap */
    mmv_stats_value_t *	values;	/* values in mmap */
    __int64_t *	extra;		/* per value value. holds old DISCRETE value */
    int		vcnt;		/* number of values */
    int		fd;		/* mmap fd */
    __int64_t	len;		/* mmap region len */
    time_t	ts;		/* mmap file timestamp */
    int		moff;		/* Index of the first metric in the array */
    int		mcnt;		/* How many metrics have we got */
    int		cluster;	/* cluster id */
    __uint64_t	gen;		/* generation number on open */
} * slist;

static int scnt;

static int
update_names(void)
{
    char script[MAXPATHLEN];
    int sep = __pmPathSeparator();

    snprintf (script, sizeof(script),
		"%s%c" "lib" "%c" "ReplacePmnsSubtree mmv %s%c" "mmv.new",
		pmGetConfig("PCP_SHARE_DIR"), sep, sep, pmnsdir, sep);
    if (system (script) == -1) {
	__pmNotifyErr (LOG_ERR, "%s: cannot exec %s", pmProgname, script);
	return 1;
    }
    snprintf (script, sizeof(script),
		"%s%c" "pmsignal -a -s HUP pmcd",
		pmGetConfig("PCP_BINADM_DIR"), sep);
    if (system (script) == -1) {
	__pmNotifyErr (LOG_ERR, "%s: cannot exec %s", pmProgname, script);
	return 1;
    }
    return 0;
}

static void
map_stats(void)
{
    char path[MAXPATHLEN], tmppath[MAXPATHLEN];
    char * fname;
    FILE *f = NULL;
    FILE * conf;
    int sep = __pmPathSeparator();
    int i;
 
    putenv("TMPDIR=");	/* temp file must be in pmnsdir, for rename */

#if HAVE_MKSTEMP
    sprintf(tmppath, "%s%cmmv-XXXXXX", pmnsdir, __pmPathSeparator());
    fname = tmppath;
    i = mkstemp(fname);
    if (i != -1)
	f = fdopen(i, "w");
#else
    fname = tempnam (pmnsdir, "mmv");
    if (fname != NULL) {
	strncpy (tmppath, fname, sizeof(tmppath));
	free (fname);
	fname = tmppath;
	f = fopen(fname, "w");
    }
#endif
    if (f == NULL ) {
	__pmNotifyErr(LOG_ERR, "%s: failed to generate temporary file %s: %s",
			  pmProgname, fname, strerror(errno));
	return;
    }

    fputs ("mmv {\n", f);

    if ( metrics != NULL ) {
	/* Re-alloc metrics to include only mmv.reload... */
	mcnt = 1;
	if ((metrics = realloc (metrics, sizeof (pmdaMetric)*mcnt)) == NULL) {
	    mcnt = 0;
	    goto close_and_return;
	} else {
	    fprintf (f, "\treload\t%d:0:0\n", MMV);
	}
    }

    if ( indoms != NULL ) {
	for (i=0; i < incnt; i++ ) {
	    free (indoms[i].it_set);
	}
	free (indoms);
	indoms = NULL;
	incnt = 0;
    }

    if ( slist != NULL ) {
	for ( i=0; i < scnt; i++ ) {
	    free (slist[i].name);
	    free (slist[i].extra);
	    __pmMemoryUnmap(slist[i].addr, slist[i].len);
	    close (slist[i].fd);
	}
	free (slist);
	slist = NULL;
	scnt = 0;
    }
  
    if ( (conf = fopen (confpath, "r")) != NULL) {
	char buffer[MAXPATHLEN];
	int cluster = 0;

	while ( (fgets(buffer, sizeof(buffer)-1, conf)) != NULL ) {
	    struct stat statbuf;
	    char client[MAXPATHLEN];
	    char *p;
	    int ecluster;

	    /* strip comments */
	    p=strchr(buffer,'#');
	    if (p)
		*p='\0';

	    if ( (sscanf(buffer, "%[^ \t\n]", client) != 1) || !*client )
		continue;

	    if ( sscanf(buffer+strlen(client), "%d", &ecluster) != 1 ) {
		cluster++;
	    } else {
		cluster = ecluster;
	    }

	    __pmNotifyErr(LOG_INFO, "%s: mmv client %d - \"%s\"",
			  pmProgname, cluster, client);

	    if ( strchr (client, sep) == NULL ) {
	        sprintf (path, "%s%c%s", statsdir, sep, client);
	    } else {
		strcpy (path, client);
	    }

	    if (stat(path, &statbuf) >= 0 && S_ISREG(statbuf.st_mode) ) {
		int fd;

		if ((fd = open(path, O_RDONLY)) >= 0) {
		    void *m = __pmMemoryMap(fd, statbuf.st_size, 0);
		    if (m == NULL) {
		        __pmNotifyErr(LOG_ERR, 
				      "%s: failed to memory map \"%s\" - %s",
				      pmProgname, client,
				      strerror(errno));
			close (fd);
		    } else {
			mmv_stats_hdr_t * hdr = (mmv_stats_hdr_t *)m;
			int s;

			if ( strcmp (hdr->magic, "MMV") ) {
			    __pmMemoryUnmap(m, statbuf.st_size);
			    close (fd);
			    continue;
			}

			if ( hdr->version != MMV_VERSION ) {
				close (fd);
				__pmNotifyErr(LOG_ERR, 
					      "%s: mmv client version %d "
					      "not supported (current is %d)",
					      pmProgname, hdr->version, MMV_VERSION);
				continue;
			}

			if ( !hdr->g1 || hdr->g1 != hdr->g2 ) {
			    /* Daemon is messing with us - give it a
			     * second to finish */
			    int w;

			    for (w=0; (w<10)&&(!hdr->g1 || hdr->g1 != hdr->g2); w++) {
				struct timespec rem, req = {0, 100000000};
				nanosleep(&req, &rem);
			    }

			    if ( !hdr->g1 || hdr->g1 != hdr->g2 ) {
				/* Daemon takes too long - ingore it */
				__pmMemoryUnmap(m, statbuf.st_size);
				close (fd);
				__pmNotifyErr(LOG_ERR, 
					      "%s: waited too long for"
					      " generation sync in %s",
					      pmProgname, path);
				continue;

			    }
			}

			for (s=0; s < scnt; s++ ) {
			    if (!strcmp(slist[s].name, client)) {
				__pmNotifyErr(LOG_ERR, 
					      "%s: duplicate client %s"
					      " - skipping",
					      pmProgname, client);
				break;
			    }
			}

			if ( s == scnt ) {
			    slist = realloc (slist, 
					     sizeof (struct stats_s)*(scnt+1));
			    if ( slist != NULL ) {
				slist[scnt].name = strdup (client);
				slist[scnt].addr = m;
				slist[scnt].hdr = hdr;
				slist[scnt].fd = fd;
				slist[scnt].ts = statbuf.st_ctime;
				slist[scnt].cluster = cluster;
				slist[scnt].mcnt = 0;
				slist[scnt].moff = -1;
				slist[scnt].gen = hdr->g1;
				slist[scnt++].len = statbuf.st_size;

				fprintf (f, "\t%s\n", client);
			    }
			} else {
			    __pmMemoryUnmap(m, statbuf.st_size);
			    close (fd);
			}
		    }
		} else {
		    __pmNotifyErr(LOG_ERR, 
				  "%s: failed to open client file \"%s\" - %s",
			          pmProgname, client, strerror(errno));
		}
	    } else {
		__pmNotifyErr(LOG_ERR, 
			      "%s: failed to stat client file \"%s\" - %s",
			      pmProgname, client, strerror(errno));
	    }
	}

	fputs ("}\n", f);
	fclose (conf);

	for ( i=0; i < scnt; i++ ) {
	    int j;
	    struct stats_s * s = slist+i;
	    mmv_stats_hdr_t * hdr = (mmv_stats_hdr_t *)s->addr;
	    mmv_stats_toc_t * toc = 
		(mmv_stats_toc_t *)((char *)s->addr+sizeof (mmv_stats_hdr_t));

	    fprintf (f, "mmv.%s {\n", s->name);

	    for (j=0; j < hdr->tocs; j++ ) {
		switch ( toc[j].typ ) {
		case MMV_TOC_METRICS:
		    metrics = realloc (metrics, 
				       sizeof (pmdaMetric)*(mcnt+toc[j].cnt));
		    if ( metrics != NULL ) {
			int k;
			mmv_stats_metric_t * ml = 
			    (mmv_stats_metric_t *)((char *)s->addr + 
						  toc[j].offset);

			if ( s->moff < 0 ) {
			    s->moff = mcnt;
			}
			s->mcnt += toc[j].cnt;

			for ( k=0; k < toc[j].cnt; k++ ) {
			    metrics[mcnt].m_user = ml+k;
			    metrics[mcnt].m_desc.pmid = 
				PMDA_PMID(s->cluster,k);

			    if ( ml[k].type == MMV_ENTRY_INTEGRAL ) {
    			        metrics[mcnt].m_desc.sem = PM_SEM_COUNTER;
				metrics[mcnt].m_desc.type = MMV_ENTRY_I64;
			    } else if ( ml[k].type == MMV_ENTRY_DISCRETE ) {
			        metrics[mcnt].m_desc.sem = PM_SEM_DISCRETE;
				metrics[mcnt].m_desc.type = MMV_ENTRY_I64;
			    } else {
    			        metrics[mcnt].m_desc.sem = ml[k].semantics;
				metrics[mcnt].m_desc.type = ml[k].type;
			    }
			    metrics[mcnt].m_desc.indom = 
				(s->cluster << 11) | ml[k].indom;
			    memcpy (&metrics[mcnt].m_desc.units,
				    &ml[k].dimension, sizeof (pmUnits));

			    fprintf (f, "\t%s\t%d:%d:%d\n",
				     ml[k].name, MMV, s->cluster, k);
			    mcnt++;
			}
		    } else {
			__pmNotifyErr(LOG_ERR, "%s: cannot grow metric list",
				      pmProgname);
			exit (1);
		    }
		    break;

		case MMV_TOC_INDOM:
		    indoms = realloc (indoms,(incnt+1)*sizeof (pmdaIndom));

		    if ( indoms != NULL ) {
			mmv_stats_inst_t * id =
			    (mmv_stats_inst_t *)((char *)s->addr + 
						 toc[j].offset);

			indoms[incnt].it_indom = (s->cluster<< 11) | j;
			indoms[incnt].it_numinst = toc[j].cnt;
			indoms[incnt].it_set = 
			    (pmdaInstid *) calloc (toc[j].cnt, 
						   sizeof (pmdaInstid));

			if ( indoms[incnt].it_set != NULL ) {
			    int k;

			    for ( k=0; k < indoms[incnt].it_numinst; k++ ) {
				indoms[incnt].it_set[k].i_inst= id[k].internal;
				indoms[incnt].it_set[k].i_name= id[k].external;
			    }
			} else {
			    __pmNotifyErr(LOG_ERR, 
					  "%s: cannot get memory for "
					  "instance list",
					  pmProgname);
			    exit (1);
			}
			incnt++;
		    } else {
			__pmNotifyErr(LOG_ERR, "%s: cannot grow indom list",
				      pmProgname);
			exit (1);
		    }

		    break;

		case MMV_TOC_VALUES: 
		    s->vcnt = toc[j].cnt;
		    s->values = (mmv_stats_value_t *)((char *)s->addr + 
						     toc[j].offset);
		    s->extra = (__int64_t*)malloc(s->vcnt*sizeof(__int64_t));
		    if (!s->extra) {
			__pmNotifyErr(LOG_ERR, 
				      "%s: cannot get memory for values",
				      pmProgname);
			exit (1);
		    } else {
			int k;
			for (k=0; k<s->vcnt; k++)
			    s->extra[k] = -1;
		    }
		    break;
		}
	    }

	    fputs ("}\n", f);
	}

	fclose (f);
	sprintf (path, "%s%c" "mmv.new", pmnsdir, sep);
	if ( rename2 (fname, path) < 0 ) {
	    __pmNotifyErr(LOG_ERR, "%s: cannot rename %s to %s - %s",
			  pmProgname, fname, path, strerror (errno));
	}
    } else {
	__pmNotifyErr(LOG_ERR, "%s: cannot open %s - %s",
		      pmProgname, confpath, strerror (errno));
    }

    reload = 0;
    return;

 close_and_return:
    fclose (f);
    return;
}

/*
 * callback provided to pmdaFetch
 */
static int
mmv_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int * id = (__pmID_int *)&(mdesc->m_desc.pmid);

    if ( id->cluster == 0 ) { /* Control cluster */
	int i = 0;

	for ( i=0; i < mcnt; i++ ) {
	    __pmID_int * mid = (__pmID_int *)&(metrics[i].m_desc.pmid);

	    if ( mid->cluster == 0 ) {
		if ( mid->item == id->item ) {
		    switch (metrics[i].m_desc.type ) {
		    case PM_TYPE_32:
			atom->l = *((int *) metrics[i].m_user);
			break;
		    case PM_TYPE_U32:
			atom->ul = *((unsigned int *) metrics[i].m_user);
			break;
		    case PM_TYPE_64:
			atom->ll = *((__int64_t *) metrics[i].m_user);
			break;
		    case PM_TYPE_U64:
			atom->ull = *((__uint64_t *) metrics[i].m_user);
			break;
		    case PM_TYPE_FLOAT:
			atom->f = *((float *) metrics[i].m_user);
			break;
		    case PM_TYPE_DOUBLE:
			atom->d =  *((double *) metrics[i].m_user);
			break;
		    }

		    return 1;
		}
	    } else {
		break;
	    }
	}
    } else if ( scnt > 0 ) { /* We have a least one source of metrics */
	int c;

	for ( c=0; c < scnt; c++ ) {
	    struct stats_s * s = slist + c;

	    if ( s->cluster == id->cluster ) {
		if ( id->item < s->mcnt ) {
		    mmv_stats_value_t * val = s->values;
		    mmv_stats_metric_t * m =
			(mmv_stats_metric_t *)metrics[s->moff+id->item].m_user;
		    int i;

		    /* There is no direct mapped instances, so we have
		     * to search */
		    for ( i=0; i < s->vcnt; i++ ) {
			mmv_stats_metric_t * mt = 
			    (mmv_stats_metric_t *)((char *)s->addr + 
						  val[i].metric);
			mmv_stats_inst_t * is = 
			    (mmv_stats_inst_t *)((char *)s->addr + 
						 val[i].instance);

			if ( (mt == m) && 
			     ((mt->indom < 0) || (is->internal == inst)) ) {
			    struct timeval tv; 

			    switch (m->type ) {
			    case MMV_ENTRY_I32:
				atom->l = val[i].val.i32;
				break;
			    case MMV_ENTRY_U32:
				atom->ul = val[i].val.u32;
				break;
			    case MMV_ENTRY_I64:
				atom->ll = val[i].val.i64;
				break;
			    case MMV_ENTRY_U64:
				atom->ull = val[i].val.u64;
				break;
			    case MMV_ENTRY_FLOAT:
				atom->f = val[i].val.f;
				break;
			    case MMV_ENTRY_DOUBLE:
				atom->d = val[i].val.d;
				break;
			    case MMV_ENTRY_INTEGRAL:
				gettimeofday (&tv, NULL); 
				atom->ll = val[i].val.i64 + 
				    val[i].extra*(tv.tv_sec*1e6 + tv.tv_usec);
				break;
			    case MMV_ENTRY_DISCRETE: {
				__int64_t new = val[i].val.i64; 
				__int64_t old = s->extra[i];

				s->extra[i] = new;

				if (old < 0) {
				    return PM_ERR_AGAIN;
				} else {
				    atom->ll = new - old;
				    if (atom->ll < 0) 
					return PM_ERR_CONV;
				}
				break;
			    }
			    case MMV_ENTRY_NOSUPPORT:
				return PM_ERR_APPVERSION;

			    }
			    return 1;
			}
		    }
		} else {
		    __pmNotifyErr(LOG_ERR, 
				  "%s: item %d is too big for cluster %s "
				  "with %d item",
				  pmProgname, id->item, s->name, s->mcnt);

		    return PM_ERR_PMID;
		}
	    }
	}

	/* If we're here, it means there is no cluster for the metric
	 * so we could as well return an error */
	return PM_ERR_PMID;
    }

    return 0;
}

static int
mmv_reload_maybe (int notready)
{
    int i;
    struct stat s;
    int need_reload = reload;
    
    /* check if any of the generation numbers have changed (unexpected) */
    for ( i=0; i < scnt; i++ ) {
	if (slist[i].hdr->g1!=slist[i].gen || slist[i].hdr->g2!=slist[i].gen) {
	    need_reload++;
	    break;
	}
    }
    
    /* check if the mmv.conf has been modified */
    if ( stat (confpath, &s) >= 0 && s.st_ctime != conf_ts ) {
	need_reload++;
	conf_ts = s.st_ctime;
    }
    
    /* check if the directory has been modified */
    if ( stat (statsdir, &s) >= 0 && s.st_ctime != statsdir_ts ) {
	need_reload++;
	statsdir_ts = s.st_ctime;
    }

    if ( need_reload ) {
	/* something changed - reload */
	int m;
	pmdaExt * pmda = dispatch.version.two.ext; /* I know it V.2 */

	if (notready)
	    __pmSendError(pmda->e_outfd, PDU_BINARY, PM_ERR_PMDANOTREADY);
	__pmNotifyErr(LOG_INFO, "%s: reloading", pmProgname);

	map_stats();

	pmda->e_indoms = indoms;
	pmda->e_nindoms = incnt;
	pmda->e_metrics = metrics;
	pmda->e_nmetrics = mcnt;
	pmda->e_direct = 0;

	/* fix bit fields in indom for all instance domains */
	for (m = 0; m < pmda->e_nindoms; m++) {
	    int serial = pmda->e_indoms[m].it_indom;
	    __pmInDom_int * indomp = 
		(__pmInDom_int *)&(pmda->e_indoms[m].it_indom);
	    indomp->serial = serial;
	    indomp->pad = 0;
	    indomp->domain = dispatch.domain;
	}

	/* fix bitfields in metrics */
	for (m = 0; m < pmda->e_nmetrics; m++) {
	    __pmID_int * pmidp = 
		(__pmID_int *)&pmda->e_metrics[m].m_desc.pmid;
    
	    if (pmda->e_metrics[m].m_desc.indom != PM_INDOM_NULL) {
		int serial = pmda->e_metrics[m].m_desc.indom;
		__pmInDom_int * indomp = 
		    (__pmInDom_int *)&(pmda->e_metrics[m].m_desc.indom);
		indomp->serial = serial;
		indomp->pad = 0;
		indomp->domain = dispatch.domain;
	    }
    
	    pmidp->domain = dispatch.domain;
	}

	__pmNotifyErr(LOG_INFO, 
		      "%s: %d metrics and %d indoms after reload", 
		      pmProgname, mcnt, incnt);

	reload = update_names();
    }

    return need_reload;
}

/* Intercept request for descriptor and check if we'd have to reload */
static int
mmv_desc (pmID pmid, pmDesc *desc, pmdaExt *ep)
{
    if (pmid != metrics[0].m_desc.pmid)		/* mmv.reload is special */
	mmv_reload_maybe (0);
    return pmdaDesc (pmid, desc, ep);
}

/* Same as mmv_desc - intercept and reload_maybe */
static int
mmv_text(int ident, int type, char **buffer, pmdaExt *ep)
{
    mmv_reload_maybe (0);
    return pmdaText (ident, type, buffer, ep);
}

static int
mmv_instance(pmInDom indom, int inst, char *name, 
	     __pmInResult **result, pmdaExt *ep)
{
    mmv_reload_maybe (0);
    return pmdaInstance (indom, inst, name, result, ep);
}

static int
mmv_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    mmv_reload_maybe (0);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
mmv_store(pmResult *result, pmdaExt *ep)
{
    if ( mmv_reload_maybe (1) ) {
	return PM_ERR_PMDAREADY;
    } else {
	int i;

	for (i = 0; i < result->numpmid; i++) {
	    pmValueSet * vsp = result->vset[i];
	    __pmID_int * id = (__pmID_int *)&vsp->pmid;

	    if ( id->cluster == 0 ) {
		/* We only have values which could be modified in cluster 0 */
		int m = 0;

		for ( m=0; m < mcnt; m++ ) {
		    __pmID_int * mid = (__pmID_int *)&(metrics[m].m_desc.pmid);

		    if ( mid->cluster == 0 ) {
			if ( mid->item == id->item ) {
			    int sts = 0;
			    int type = metrics[m].m_desc.type;
			    pmAtomValue atom;

			    if (vsp->numval != 1 )
				return (PM_ERR_CONV);

			    if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
						      type, &atom, type)) < 0)
				return sts;

			    switch (metrics[m].m_desc.type ) {
			    case PM_TYPE_32:
				*((int *) metrics[i].m_user) = atom.l;
				break;
			    case PM_TYPE_U32:
				*((unsigned int *) metrics[i].m_user)=atom.ul;
				break;
			    case PM_TYPE_64:
				*((__int64_t *) metrics[i].m_user) = atom.ll;
				break;
			    case PM_TYPE_U64:
				*((__uint64_t *) metrics[i].m_user)=atom.ull;
				break;
			    case PM_TYPE_FLOAT:
				*((float *) metrics[i].m_user) = atom.f;
				break;
			    case PM_TYPE_DOUBLE:
				*((double *) metrics[i].m_user) = atom.d;
				break;
			    }
			    break;
			}
		    } else {
			return (PM_ERR_PMID);
		    }
		}
	    }
	}
    }

    return 0;
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default "
	  "log name\n",
	  stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int		err = 0;
    int		sep = __pmPathSeparator();
    char	mypath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    snprintf(mypath, sizeof(mypath), "%s%c" "mmv" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, MMV,
		"mmv.log", mypath);

    if ( (pmdaGetOpt(argc, argv, "D:d:l:?", &dispatch, &err) != EOF) || 
	err || argc != optind ) {
	usage();
    }

    pcptmpdir = pmGetConfig ("PCP_TMP_DIR");
    pcpvardir = pmGetConfig ("PCP_VAR_DIR");
    pcppmdasdir = pmGetConfig ("PCP_PMDAS_DIR");

    sprintf(confpath, "%s%c" "mmv" "%c" "mmv.conf", pcppmdasdir, sep, sep);
    sprintf(statsdir, "%s%c" "mmv", pcptmpdir, sep);
    sprintf(pmnsdir, "%s%c" "pmns", pcpvardir, sep);

    pmdaOpenLog(&dispatch);

    /* Initialize internal dispatch table */
    if (dispatch.status == 0) {
	if ( (metrics = malloc (sizeof(pmdaMetric))) != NULL ) {
	    metrics[mcnt].m_user = & reload;
	    metrics[mcnt].m_desc.pmid = PMDA_PMID(0, 0);
	    metrics[mcnt].m_desc.type = PM_TYPE_32;
	    metrics[mcnt].m_desc.indom = PM_INDOM_NULL;
	    metrics[mcnt].m_desc.sem = PM_SEM_INSTANT;
	    memset (&metrics[mcnt].m_desc.units, 0, sizeof (pmUnits));
	    mcnt = 1;
	} else {
	    __pmNotifyErr(LOG_ERR, "%s: pmdaInit - out of memory\n", pmProgname);
	    exit (0);
	}

	dispatch.version.two.fetch = mmv_fetch;
	dispatch.version.two.store = mmv_store;
	dispatch.version.two.desc = mmv_desc;
	dispatch.version.two.text = mmv_text;
	dispatch.version.two.instance = mmv_instance;

	pmdaSetFetchCallBack(&dispatch, mmv_fetchCallBack);

	__pmNotifyErr(LOG_INFO, "%s: pmdaInit - %d metrics and %d indoms", 
		      pmProgname, mcnt, incnt);

	pmdaInit(&dispatch, indoms, incnt, metrics, mcnt);
    }

    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
