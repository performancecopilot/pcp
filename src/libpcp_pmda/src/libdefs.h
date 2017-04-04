/*
 * Copyright (c) 2013 Red Hat.
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
#define HAVE_ANY(interface)	((interface) <= PMDA_INTERFACE_6 && HAVE_V_TWO(interface))

/*
 * Auxilliary structure used to save data from pmdaDSO or pmdaDaemon and
 * make it available to the other methods, also as private per PMDA data
 * when multiple DSO PMDAs are in use
 */
typedef struct {
    pmdaInterface	*dispatch;	/* back pointer to our pmdaInterface */
    pmResult		*res;		/* high-water allocation for */
    int			maxnpmids;	/* pmResult for each PMDA */
    __pmHashCtl		hashpmids;	/* hashed metrictab lookups */
} e_ext_t;

#endif /* LIBDEFS_H */
