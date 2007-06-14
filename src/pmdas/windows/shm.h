/*
 * shared memory region description
 *
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#define SHM_MAGIC	0x70615243
#define SHM_ROUND(x) (((x)+7)&(~7))

/*
 * the only safe place to be able to open this file in both the PMDA and
 * shim.exe is a relative pathname, or in the current directory (wherever
 * that might be hiding) ... /tmp does not work for shim.exe, and there
 * is no windows pathname that will apparently work for SFU and Cygwin
 * both
 */
#define SHM_FILENAME	"pcp.windows.shm"

typedef struct {
    int		base;
    int		elt_size;
    int		nelt;
} shm_seg_t;

#define MAX_UNAME_SIZE	64

typedef struct {
    int		magic;
    int		size;
    int		nseg;
    /*
     * special metrics come next ... not PDH fodder
     */
    int		physmem;		/* hinv.physmem */
    char	uname[MAX_UNAME_SIZE];	/* kernel.uname.distro */
    char	build[MAX_UNAME_SIZE];	/* kernel.uname.release */
    /*
     * all of the variable stuff
     */
    shm_seg_t	segment[1];
} shm_hdr_t;

/*
 * segments in the shared memory region
 */
#define SEG_METRICS	0
#define SEG_SCRATCH	1
#define SEG_INDOM	2

/*
 * data structures assigned to the shared memory region
 */

/*
 * m_qid PDH query groups, used in shm_metrictab[] in data.c to index
 * querytab[] in init.c, shim.c and fetch.c
 */
#define Q_DISK_ALL	0
#define Q_DISK_DEV	1
#define Q_PERCPU	2
#define Q_KERNEL	3
#define Q_MEMORY	4
#define Q_SQLSERVER	5
#define Q_LDISK		6

/*
 * m_flags bit fields
 */
#define M_NONE		       0
#define M_EXPANDED	       1/* pattern has been expanded */
#define M_REDO		       2/* redo pattern expansion on each fetch */
#define M_NOVALUES	       4/* setup failed, don't bother trying to fetch */
#define M_100NSEC	       8/* units are 100nsec timer ticks */	

#define MAX_M_PATH_LEN 80

typedef struct {
    pmDesc	m_desc;		/* pmid and rest copied from metrictab[] */
    int		m_qid;		/* index into querytab[] */
    int		m_flags;	/* see above */
    				/* pattern passed to PdhExpandCounterPath */
    char	m_pat[MAX_M_PATH_LEN];
} shm_metric_t;

#define MAX_INST_NAME_LEN 124

/*
 * Instance information used to build instance domain descriptions.
 *
 * Since the shm segments are not allowed to shrink, we allocate one
 * more of these stuctures than the number of instances in an indom,
 * and use the i_inst field of the first struct to encode the number
 * of instances _really_ in the indom.  The first character of i_name
 * in this entry is set the 'c' to indicate that the indom has changed.
 */
typedef struct {
    int		i_inst;
    char	i_name[MAX_INST_NAME_LEN];
} shm_inst_t;

/*
 * values returned from prefetch for later picking in the fetch
 * callback
 */
typedef struct {
    pmID	r_pmid;
    int		r_inst;
    pmAtomValue	r_atom;
} shm_result_t;

/*
 * Instance domain numbers ... we expect these values to be used as indexes
 * into indomtab[] in data.c
 */
#define DISK_INDOM	0
#define CPU_INDOM	1
#define NETIF_INDOM	2
#define LDISK_INDOM	3
#define SQL_LOCK_INDOM	4
#define SQL_CACHE_INDOM	5
#define SQL_DB_INDOM	6
