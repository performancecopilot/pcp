/*
 * Copyright (c) 2018 Fujitsu.
 * Copyright (c) 2018 Red Hat.
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
 */
#ifndef KVM_PMDA_H
#define KVM_PMDA_H

#include "pmapi.h"
#include "libpcp.h"
#include "impl.h"
#include "pmda.h"

/*
 * PMID cluster numbers
 */
enum {
        CLUSTER_DEBUG=0,         /* kvm debug */
        CLUSTER_TRACE=1,         /* kvm trace */
        NUM_CLUSTERS            /* one more than highest numberedNUM_CLUSTERS cluster */
};

/*
 * InDom serial numbers
 */
enum {
	TRACE_INDOM=0,		/* tracing/events/kvm/ */ 
	NUM_INDOMS              /* one more than highest numbered cluster */
};

#endif /* KVM_PMDA_H */
