/*
 * Copyright (c) 2013,2016,2018,2019 Red Hat.
 * Copyright (c) 1999-2000 Silicon Graphics, Inc.  All Rights Reserved.
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
 */
#ifndef LIBDEFS_H
#define LIBDEFS_H

#define HAVE_V_TWO(interface)	((interface) >= PMDA_INTERFACE_2)
#define HAVE_V_FOUR(interface)	((interface) >= PMDA_INTERFACE_4)
#define HAVE_V_FIVE(interface)	((interface) >= PMDA_INTERFACE_5)
#define HAVE_V_SIX(interface)	((interface) >= PMDA_INTERFACE_6)
#define HAVE_V_SEVEN(interface)	((interface) >= PMDA_INTERFACE_7)
#define HAVE_ANY(interface)	((interface) <= PMDA_INTERFACE_7 && HAVE_V_TWO(interface))

struct dynamic;

/*
 * Auxilliary structure used to save data from pmdaDSO or pmdaDaemon and
 * make it available to the other methods, also as private per PMDA data
 * when multiple DSO PMDAs are in use
 */
typedef struct {
    pmdaInterface	*dispatch;	/* back pointer to our pmdaInterface */
    pmResult		*res;		/* high-water allocation for */
    __pmHashCtl		hashpmids;	/* hashed metrictab lookups */
    int			maxnpmids;	/* pmResult for each PMDA */
    int			ndynamics;	/* number of dynamics entries, below */
    struct dynamic	*dynamics;	/* dynamic metric manipulation table */
    void		*privdata;	/* private (user) data for this PMDA */
} e_ext_t;

/*
 * Local hash function
 */
extern __uint32_t hash(const signed char *, int, __uint32_t);

/*
 * These ones escaped via the exports file, but are only used within
 * the libpcp_pmda library, so pull the definitions back from <pcp/pmda.h>
 */
PMDA_CALL extern __pmnsNode * pmdaNodeLookup(__pmnsNode *, const char *);

#endif /* LIBDEFS_H */
