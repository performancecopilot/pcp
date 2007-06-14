/*
 * only the essential Windows headers
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
#define WIN32_LEAN_AND_MEAN 1

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <WinPerf.h>
#include <Tchar.h>
#include <Pdh.h>
#include <PdhMsg.h>

/*
 * Customized PCP headers from build environment
 */
#include "./shim_pcp.h"

/*
 * shared memory region layout
 */
#include "./shm.h"

/*
 * q_flags bit fields
 */
#define Q_NONE		0
#define Q_COLLECTED	1	/* if PdhCollectQueryData has been called */
#define Q_ERR_SEEN	2	/* if PdhCollectQueryData error reported */

typedef struct {
    HQUERY	q_hdl;		/* from PdhOpenQuery */
    int		q_flags;	/* see above */
} shim_query_t;

extern shim_query_t	*querytab;
extern int		querytab_sz;

typedef struct {
    HCOUNTER	c_hdl;		/* from PdhAddCounter */
    int		c_inst;		/* PM_IN_NULL or instance identifier */
} shim_ctr_t;

typedef struct {
    int		m_ctype;	/* PDH counter type */
    int		m_num_ctrs;	/* one or more counters */
    shim_ctr_t	*m_ctrs;
} shim_metric_t;

/*
 * two metric tables (corresponding entries are for the same metric)
 * ... shm_metrictab[] is in the shared memory segment and is set up
 *     by the PMDA
 * ... shim_metrictab[] is private to shim.exe and is created at
 *     intialization in shim_init()
 */
extern shm_metric_t	*shm_metrictab;
extern shim_metric_t	*shim_metrictab;
extern int		metrictab_sz;

/*
 * shared memory control
 */
extern shm_hdr_t	*shm;
extern shm_hdr_t	*new_hdr;
extern HANDLE		shm_hfile;
extern HANDLE		shm_hmap;
extern int		hdr_size;
extern int		shm_oldsize;

extern int shim_init(void);
extern char *pdherrstr(int);
extern char *decode_ctype(DWORD);
extern int check_instance(char *, shm_metric_t *, int *);
extern int help(int, int, char **);
extern int prefetch(int);
extern void shm_dump_hdr(FILE *, char *, shm_hdr_t *);
extern void shm_remap(int);
extern void shm_reshape(shm_hdr_t *);
