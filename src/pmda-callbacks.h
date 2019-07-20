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
int
statsd_desc(pmID pm_id, pmDesc* desc, pmdaExt* pmda);

/**
 * Wrapper around pmdaText, called before control is passed to pmdaText
 * @arg ident -
 * @arg type - Base data type
 * @arg buffer - 
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_text(int ident, int type, char** buffer, pmdaExt* pmda);

/**
 * Wrapper around pmdaInstance, called before control is passed to pmdaInstance
 * @arg in_dom - Instance domain description
 * @arg inst - Instance domain num
 * @arg name - Instance domain name
 * @arg result - Result to populate
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_instance(pmInDom in_dom, int inst, char* name, pmInResult** result, pmdaExt* pmda);

/**
 * Wrapper around pmdaFetch, called before control is passed to pmdaFetch
 * @arg num_pm_id - Metric id
 * @arg pm_id_list - Collection of instance domains
 * @arg resp - Result to populate
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_fetch(int num_pm_id, pmID pm_id_list[], pmResult** resp, pmdaExt* pmda);

/**
 * Wrapper around pmdaStore, called before control is passed to pmdaStore
 * @arg result - Result to me populated
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_store(pmResult* result, pmdaExt* pmda);

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
int
statsd_label_callback(pmInDom in_dom, unsigned int inst, pmLabelSet** lp);

/**
 * This callback deals with one request unit which may be part of larger request of PDU_FETCH
 * @arg pmdaMetric - requested metric, along with user data, in out case PMDA extension structure (contains agent-specific private data)
 * @arg inst - requested metric instance
 * @arg atom - atom that should be populated with request response
 * @return value less then 0 signalizes error, equal to 0 means that metric is not available, greater then 0 is success
 */
int
statsd_fetch_callback(pmdaMetric* mdesc, unsigned int inst, pmAtomValue* atom);

#endif
