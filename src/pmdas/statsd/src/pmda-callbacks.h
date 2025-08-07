/*
 * Copyright (c) 2020 Red Hat.
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
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
#ifndef PCP_CALLBACKS_
#define PCP_CALLBACKS_

#include <pcp/pmapi.h>
#include <pcp/pmda.h>

/**
 * Wrapper around pmdaDesc, called before control is passed to pmdaDesc
 * @arg pm_id - Instance domain
 * @arg desc - Performance Metric Descriptor
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
extern int
statsd_desc(pmID pm_id, pmDesc* desc, pmdaExt* pmda);

/**
 * Wrapper around pmdaText, called before control is passed to pmdaText
 * @arg ident -
 * @arg type - Base data type
 * @arg buffer - 
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
extern int
statsd_text(int ident, int type, char** buffer, pmdaExt* pmda);

/**
 * Wrapper around pmdaInstance, called before control is passed to pmdaInstance
 * @arg in_dom - Instance domain description
 * @arg inst - Instance domain num
 * @arg name - Instance domain name
 * @arg result - Result to populate
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
extern int
statsd_instance(pmInDom in_dom, int inst, char* name, pmInResult** result, pmdaExt* pmda);

/**
 * Wrapper around pmdaFetch, called before control is passed to pmdaFetch
 * @arg num_pm_id - Metric id
 * @arg pm_id_list - Collection of instance domains
 * @arg resp - Result to populate
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
extern int
statsd_fetch(int num_pm_id, pmID pm_id_list[], pmdaResult** resp, pmdaExt* pmda);

/**
 * Wrapper around pmdaTreePMID, called before control is passed to pmdaTreePMID
 * @arg name -
 * @arg pm_id - Instance domain
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
extern int
statsd_pmid(const char* name, pmID* pm_id, pmdaExt* pmda);

/**
 * Wrapper around pmdaTreeName, called before control is passed to pmdaTreeName
 * @arg pm_id - Instance domain
 * @arg nameset -
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
extern int
statsd_name(pmID pm_id, char*** nameset, pmdaExt* pmda);

/**
 * Wrapper around pmdaTreeChildren, called before control is passed to pmdaTreeChildren
 * @arg name - 
 * @arg traverse -
 * @arg children - 
 * @arg status -
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
extern int
statsd_children(const char* name, int traverse, char*** children, int** status, pmdaExt* pmda);

/**
 * Wrapper around pmdaLabel, called before control is passed to pmdaLabel
 * @arg ident - 
 * @arg type - 
 * @arg lp - Provides name and value indexes in JSON string
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_label(int ident, int type, pmLabelSet** lp, pmdaExt* pmda);

/**
 * NOT IMPLEMENTED
 * @arg in_dom - Instance domain description
 * @arg inst -
 * @arg lp - Provides name and value indexes in JSON string
 */
extern int
statsd_label_callback(pmInDom in_dom, unsigned int inst, pmLabelSet** lp);

/**
 * This callback deals with one request unit which may be part of larger request of PDU_FETCH
 * @arg pmdaMetric - requested metric, along with user data, in out case PMDA extension structure (contains agent-specific private data)
 * @arg inst - requested metric instance
 * @arg atom - atom that should be populated with request response
 * @return value less then 0 signalizes error, equal to 0 means that metric is not available, greater then 0 is success
 */
extern int
statsd_fetch_callback(pmdaMetric* mdesc, unsigned int inst, pmAtomValue* atom);

#endif
