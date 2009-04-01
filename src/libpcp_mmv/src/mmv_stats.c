/*
 *  Copyright (C) 2001,2009 Silicon Graphics, Inc.  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Memory Mapped Values PMDA API
 *
 *    Implementation of simple API to export values via mmap-ed file.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include "mmv_stats.h"

#define ROUNDUP_64(i) ((((i) + 63) >> 6) << 6)

static char *pcpvardir = NULL;
static char *pcptmpdir = NULL;
static char statsdir[MAXPATHLEN];
static char confpath[MAXPATHLEN];

typedef struct {
    mmv_stats_inst_t * inst;
    int ninst;
} indom_t;

/* getconfig: get pcpvardir and pcptmpdir from environment or pcp.conf
 *	      We cannot rely on PCP libraries to be linked into the
 *	      application, MMV is desinged to work even if PCP is not
 *	      available.
 * Returns: 0 on success, 1 if cannot setup the environment
 */
static int 
getconfig(void)
{
    FILE *fp;
    char var[MAXPATHLEN];
    char *conf;
    
    /* check the environment */
    if (!pcpvardir) pcpvardir = getenv("PCP_VAR_DIR");
    if (!pcptmpdir) pcptmpdir = getenv("PCP_TMP_DIR");

    /* got what we need ? */
    if (pcpvardir && pcptmpdir) 
        return 0;
        
    /* no - read pcp.conf */
    if ((conf = getenv("PCP_CONF")) == NULL)
	conf="/etc/pcp.conf";
    
    if (!(fp = fopen(conf, "r")))
        return 1;

    while (fgets(var, sizeof(var), fp)) {
        char *val = NULL;
        char *p;
        
        /* get key & value, skipping comments */
        
	if (var[0] == '#' || (p = strchr(var, '=')) == NULL)
	    continue;
	*p = '\0';
	val = p+1;
	if ((p = strrchr(val, '\n')) != NULL)
	    *p = '\0';
        
        /* one of our keys? */
        
        if (!pcpvardir && !strcmp(var, "PCP_VAR_DIR")) {
            pcpvardir = strdup(val);
            /* got both? */
            if (pcptmpdir) break;
        }
        
        if (!pcptmpdir && !strcmp(var, "PCP_TMP_DIR")) {
            pcptmpdir = strdup(val);
            /* got both? */
            if (pcpvardir) break;
        }
        
    }
    fclose(fp);

    return !(pcpvardir && pcptmpdir);
}

static void *
memmap(int fd, int sz)
{
    void *addr;
#ifdef IS_MINGW
    HANDLE handle = CreateFileMapping((HANDLE)_get_osfhandle(fd),
				NULL, PAGE_READWRITE, 0, sz, NULL);
    if (handle == NULL)
	addr = NULL;
    else {
	addr = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, sz);
	CloseHandle(handle);
    }
#else
    addr = mmap(0, sz, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
#endif
    return addr;
}

void * 
mmv_stats_init (const char * fname, const mmv_stats_t * st, int nstats)
{
    int fd;
    void * addr = NULL;
    char fullpath[MAXPATHLEN];
    char buffer[MAXPATHLEN];
    mmv_stats_metric_t * mlist = NULL;
    indom_t * indoms;
    FILE *conf;
    int activate = 0;
    int nindoms = 0;
    int i, sz;
    int vcnt = 0;
    
    if (getconfig())
        return NULL;

    sprintf(confpath, "%s/pmdas/mmv/mmv.conf", pcpvardir);
    sprintf(statsdir, "%s/mmv", pcptmpdir);
    
    /* first see if we should activate */
    
    conf = fopen (confpath, "r");
    if (!conf) 
        return NULL;
    
    while ( (fgets(buffer, sizeof(buffer)-1, conf)) != NULL ) {
        char client[MAXPATHLEN];
        char *p =strchr(buffer,'#'); /* strip comments */

        if (p)
            *p='\0';

        if ( (sscanf(buffer, "%[^ \t\n]", client)!=1) || !* client )
            continue;
        
        if (!strcmp(client, fname)) {
            activate++;
            break;
        }
    }
    fclose(conf);
    
    if (!activate) 
        return NULL;
    
    /* ok we're in business */

    mlist = (mmv_stats_metric_t *) calloc(nstats, sizeof (mmv_stats_metric_t));
    if ( mlist == NULL ) {
	return NULL;
    } else if ((indoms = (indom_t *) calloc (nstats,sizeof(indom_t*)))==NULL) {
	free (mlist);
	return NULL;
    }

    /* Get some idea about the size of the file we need */
    for ( i=0; i < nstats; i++ ) {
	if ((st[i].type < MMV_ENTRY_NOSUPPORT) || 
	    (st[i].type > MMV_ENTRY_DISCRETE) ||
	    (strlen (st[i].name) == 0)) {
	    free (mlist);
	    free (indoms);
	    return NULL;
	}

	strcpy (mlist[i].name, st[i].name);
	mlist[i].type = st[i].type;
	mlist[i].dimension = st[i].dimension;

	if ( st[i].indom != NULL ) {
	    /* Lookup an indom */
	    int j;

	    for ( j=0; j < nindoms; j++ ) {
		if ( indoms[j].inst == st[i].indom ) {
		    vcnt += indoms[j].ninst;
		    mlist[i].indom = j;
		    break;
		}
	    }

	    if ( j == nindoms ) {
		indoms[nindoms].inst = st[i].indom;
		indoms[nindoms].ninst = 0;

		for ( j=0; st[i].indom[j].internal != -1; j++ )
		    indoms[nindoms].ninst++;

		vcnt += indoms[nindoms].ninst;
		mlist[i].indom = nindoms++;
	    }
	} else {
	    mlist[i].indom = -1;
	    vcnt++;
	}
    }

    if (vcnt == 0) {
	free (mlist);
	free (indoms);
	return NULL;
    }

    /* Size of of the header + TOC with enough instances to
     * accomodate nindoms instance lists plus metric list and value
     * list */
    sz = ROUNDUP_64(sizeof (mmv_stats_hdr_t) + 
		    sizeof (mmv_stats_toc_t) * (nindoms+2));

    /* Size of all indoms */
    for (i=0; i < nindoms; i++ ) {
	sz += ROUNDUP_64(indoms[i].ninst * sizeof (mmv_stats_inst_t));
    }

    /* Size of metrics list*/
    sz += ROUNDUP_64(nstats * sizeof (mmv_stats_metric_t));

    /* Size of values list */
    sz += vcnt * sizeof (mmv_stats_value_t);

    sprintf (fullpath, "%s/%s", statsdir, fname);
    
    /* unlink will cause the pmda to reload on next fetch */
    unlink(fullpath);

    /* creat will cause the pmda to reload on next fetch */
    if ( (fd = open (fullpath, O_RDWR | O_CREAT | O_EXCL, 0644)) >= 0 ) {
	if (lseek(fd, sz - 1, SEEK_SET) < 0 ||
	    write(fd, " ", 1) <= 0) {
	    free (mlist);
	    free (indoms);
	    close (fd);
	    return NULL;
	}

	if ( (addr = memmap(fd, sz)) != MAP_FAILED ) {
	    mmv_stats_hdr_t * hdr = (mmv_stats_hdr_t *) addr;
	    mmv_stats_value_t * val = NULL;
	    mmv_stats_toc_t * toc = 
		(mmv_stats_toc_t *)((char *)addr + sizeof (mmv_stats_hdr_t));
	    int offset = ROUNDUP_64(sizeof (mmv_stats_hdr_t) + 
				    (nindoms+2)*sizeof(mmv_stats_toc_t));

	    /* We clobber stat file uncondtionally on each restart -
	     * it's easier this way and pcp can deal with counter
	     * wraps by itself */
	    memset (hdr, 0, sizeof (mmv_stats_hdr_t));
	    strcpy (hdr->magic, "MMV");
            hdr->version = MMV_VERSION_0;
	    hdr->g1 = time(NULL);
	    hdr->tocs = nindoms+2;

	    for ( i=0; i < nindoms; i++ ) {
		toc[i].typ = MMV_TOC_INDOM;
		toc[i].cnt = indoms[i].ninst;
		toc[i].offset = offset;

		memcpy ((char *)addr + offset, indoms[i].inst, 
			sizeof (mmv_stats_inst_t) * indoms[i].ninst);

		offset += sizeof (mmv_stats_inst_t) * indoms[i].ninst;
		offset = ROUNDUP_64(offset);
	    }

	    toc[i].typ = MMV_TOC_METRICS;
	    toc[i].cnt = nstats;
	    toc[i].offset = offset;

	    memcpy ((char *)addr + offset, mlist,  
		    sizeof (mmv_stats_metric_t) * (nstats));

	    offset += sizeof (mmv_stats_metric_t) * (nstats);
	    offset = ROUNDUP_64(offset);

	    i++;

	    toc[i].typ = MMV_TOC_VALUES;
	    toc[i].cnt = vcnt;
	    toc[i].offset = offset;

	    val = (mmv_stats_value_t *)((char *)addr + offset);

	    for ( --vcnt, i=0; i < nstats; i++ ) {
		if ( st[i].indom == NULL ) {
		    memset (val+vcnt, 0, sizeof (mmv_stats_value_t));

		    val[vcnt].metric = 
			toc[nindoms].offset + i*sizeof (mmv_stats_metric_t);
		    val[vcnt--].instance = -1;
		} else {
		    int j, idx = mlist[i].indom;

		    for (j=indoms[idx].ninst - 1; j >= 0; j-- ) {
			memset (val+vcnt, 0, sizeof (mmv_stats_value_t));

			val[vcnt].metric = 
			    toc[nindoms].offset+i*sizeof(mmv_stats_metric_t);
			val[vcnt--].instance = 
			    toc[idx].offset + j*sizeof (mmv_stats_inst_t);
		    }
		}
	    }

	    hdr->g2 = hdr->g1; /* Unlock the header - PMDA can read now */
	} else {
	    addr = NULL;
	}
    }

    free (mlist);
    free (indoms);
    if (fd >= 0)
	close (fd);
 
    return (addr);
}

mmv_stats_value_t *
mmv_lookup_value_desc (void * addr, const char * metric, const char * inst)
{
    if ( addr != NULL && metric != NULL ) {
	int i;
	mmv_stats_hdr_t * hdr = (mmv_stats_hdr_t *) addr;
	mmv_stats_toc_t * toc = 
	    (mmv_stats_toc_t *)((char *)addr + sizeof (mmv_stats_hdr_t));

	for ( i=0; i < hdr->tocs; i++ ) {
	    if ( toc[i].typ ==  MMV_TOC_VALUES ) {
		int j;
		mmv_stats_value_t * v = 
		    (mmv_stats_value_t *)((char *)addr + toc[i].offset);

		for ( j=0; j < toc[i].cnt; j++ ) {
		    mmv_stats_metric_t * m = 
			(mmv_stats_metric_t *)((char *)addr + v[j].metric);
		    if ( ! strcmp (m->name, metric) ) {
			if ( m->indom < 0 ) {  /* Singular metric */
			    return v+j;
 			} else {
			    if ( inst == NULL ) {
				/* Metric has multiple instances, but
				 * we don't know which one to return,
				 * so return an error */
				return (NULL);
			    } else {
				mmv_stats_inst_t * in = 
				    (mmv_stats_inst_t *)((char *)addr + 
							 v[j].instance);

				if ( ! strcmp (in->external, inst) ) {
				    return v+j;
				}
			    }
			}
		    }
		}
	    }
	}
    }

    return (NULL);
}

void
mmv_inc_value (void * addr, mmv_stats_value_t * v, double inc)
{
    if ( v != NULL && addr != NULL ) {
	mmv_stats_metric_t * m = 
	    (mmv_stats_metric_t *)((char *)addr + v->metric);
    
	switch (m->type ) {
	case MMV_ENTRY_I32:
	    v->val.i32 += (__int32_t)inc;
	    break;
	case MMV_ENTRY_U32:
	    v->val.u32 += (__uint32_t)inc;
	    break;
	case MMV_ENTRY_DISCRETE:
            /* fall-through */
	case MMV_ENTRY_I64:
	    v->val.i64 += (__int64_t)inc;
	    break;
	case MMV_ENTRY_U64:
	    v->val.i32 += (__uint64_t)inc;
	    break;
	case MMV_ENTRY_FLOAT:
	    v->val.f += (float)inc;
	    break;
	case MMV_ENTRY_DOUBLE:
	    v->val.d += inc;
	    break;
	case MMV_ENTRY_INTEGRAL:
	    v->val.i64 +=  (__int64_t)inc;
	    if ( inc < 0 ) {
		v->extra++;
	    } else {
		v->extra--;
	    }
	    break;
	default:
	    break;
	}
    }
}
