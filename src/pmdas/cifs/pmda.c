/*
 * Common Internet File System (CIFS) PMDA
 *
 * Copyright (c) 2014, 2018, 2025 Red Hat.
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

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "domain.h"

#include "pmdacifs.h"

#include <ctype.h>

static int _isDSO = 1; /* for local contexts */

static char *cifs_procfsdir = "/proc/fs/cifs";
static char *cifs_statspath = "";

pmdaIndom indomtable[] = {
    { .it_indom = CIFS_FS_INDOM },
    { .it_indom = CIFS_DEBUG_SVR_INDOM },
    { .it_indom = CIFS_DEBUG_SESSION_INDOM },
    { .it_indom = CIFS_DEBUG_SHARE_INDOM },
};

/*
 * all metrics supported in this PMDA - one table entry for each
 *
 */
pmdaMetric metrictable[] = {
    /* GLOBAL STATS */
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_SESSION),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_SHARES),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_BUFFER),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_POOL_SIZE),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_SMALL_BUFFER),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_SMALL_POOL_SIZE),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_MID_OPS),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_TOTAL_OPERATIONS),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_TOTAL_RECONNECTS),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_VFS_OPS),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_VFS_OPS_MAX),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_VERSION),
        PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* PER CIFS SHARE STATS */
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CONNECTED),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_SMBS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_OPLOCK_BREAKS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_READ),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_READ_BYTES),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_WRITE),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_WRITE_BYTES),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_FLUSHES),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_LOCKS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_HARD_LINKS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_SYM_LINKS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_OPEN),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CLOSE),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_DELETE),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_POSIX_OPEN),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_POSIX_MKDIR),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_MKDIR),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_RMDIR),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_RENAME),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_T2_RENAME),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_FIND_FIRST),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_FIND_NEXT),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_FIND_CLOSE),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* V2 Stats */
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_READ_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_WRITE_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_FLUSHES_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_LOCKS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_NEGOTIATES),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_NEGOTIATES_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_SESSIONSETUPS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_SESSIONSETUPS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_LOGOFFS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_LOGOFFS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_TREECONS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_TREECONS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_TREEDISCONS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_TREEDISCONS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CREATES),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CREATES_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CLOSE_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_IOCTLS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_IOCTLS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CANCELS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CANCELS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_ECHOS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_ECHOS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_QUERYDIRS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_QUERYDIRS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CHANGENOTIFIES),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CHANGENOTIFIES_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_QUERYINFOS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_QUERYINFOS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_SETINFOS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_SETINFOS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_OPLOCK_BREAKS_FAILS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* CIFS DebugData Stats */    
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_GLOBAL_STATS, DEBUG_MAX_BUFFER_SIZE),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_GLOBAL_STATS, DEBUG_ACTIVE_VFS_REQUESTS),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_CONNECTION_ID),
        PM_TYPE_STRING, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_HOSTNAME),
        PM_TYPE_STRING, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_CLIENT_GUID),
        PM_TYPE_STRING, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_NUMBER_OF_CREDITS),
        PM_TYPE_U64, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_SERVER_CAPABILITIES),
        PM_TYPE_STRING, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_TCP_STATUS),
        PM_TYPE_U32, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_INSTANCE),
        PM_TYPE_U32, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_LOCAL_USERS_TO_SERVER),
        PM_TYPE_U32, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_SECURITY_MODE),
        PM_TYPE_STRING, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_REQUESTS_ON_WIRE),
        PM_TYPE_U32, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_NET_NAMESPACE),
        PM_TYPE_STRING, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_SEND),
        PM_TYPE_U64, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SERVERS_STATS, DEBUG_SVR_MAX_REQUEST_WAIT),
        PM_TYPE_U64, CIFS_DEBUG_SVR_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SESSIONS_STATS, DEBUG_SESSION_ADDR),
        PM_TYPE_STRING, CIFS_DEBUG_SESSION_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SESSIONS_STATS, DEBUG_SESSION_USES),
        PM_TYPE_U32, CIFS_DEBUG_SESSION_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SESSIONS_STATS, DEBUG_SESSION_CAPABILITY),
        PM_TYPE_STRING, CIFS_DEBUG_SESSION_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SESSIONS_STATS, DEBUG_SESSION_STATUS),
        PM_TYPE_U32, CIFS_DEBUG_SESSION_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SESSIONS_STATS, DEBUG_SESSION_SECURITY_TYPE),
        PM_TYPE_STRING, CIFS_DEBUG_SESSION_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SESSIONS_STATS, DEBUG_SESSION_ID),
        PM_TYPE_STRING, CIFS_DEBUG_SESSION_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SESSIONS_STATS, DEBUG_SESSION_USER),
        PM_TYPE_U32, CIFS_DEBUG_SESSION_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SESSIONS_STATS, DEBUG_SESSION_CRED_USER),
        PM_TYPE_U32, CIFS_DEBUG_SESSION_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SHARES_STATS, DEBUG_SHARE_MOUNTS),
        PM_TYPE_U64, CIFS_DEBUG_SHARE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SHARES_STATS, DEBUG_SHARE_DEVINFO),
        PM_TYPE_STRING, CIFS_DEBUG_SHARE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SHARES_STATS, DEBUG_SHARE_ATTRIBUTES),
        PM_TYPE_STRING, CIFS_DEBUG_SHARE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SHARES_STATS, DEBUG_SHARE_STATUS),
        PM_TYPE_U32, CIFS_DEBUG_SHARE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SHARES_STATS, DEBUG_SHARE_TYPE),
        PM_TYPE_STRING, CIFS_DEBUG_SHARE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SHARES_STATS, DEBUG_SHARE_SERIAL_NUMBER),
        PM_TYPE_STRING, CIFS_DEBUG_SHARE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SHARES_STATS, DEBUG_SHARE_TID),
        PM_TYPE_STRING, CIFS_DEBUG_SHARE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DEBUG_SHARES_STATS, DEBUG_SHARE_MAXIMAL_ACCESS),
        PM_TYPE_STRING, CIFS_DEBUG_SHARE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

pmInDom
cifs_indom(int serial)
{
    return indomtable[serial].it_indom;
}

char
*strtrim(char *str) {
    // check for empty string
    if(*str == 0)
        return str;

    // trim leading whilte space
    while(isspace((unsigned char)*str)) str++;
    
    return str;
}


int
metrictable_size(void)
{
    return sizeof(metrictable)/sizeof(metrictable[0]);
}

/*
 * Update the CIFS filesystems instance domain. This will change
 * as filesystems are mounted and unmounted (and re-mounted, etc).
 * Using the pmdaCache interfaces simplifies things and provides us
 * with guarantees around consistent instance numbering in all of
 * those interesting corner cases.
 */
static int
cifs_instance_refresh(void)
{
    int sts;
    char buffer[PATH_MAX], fsname[PATH_MAX];
    FILE *fp;
    pmInDom indom = INDOM(CIFS_FS_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* update indom cache based off of /proc/fs/cifs/Stats file */
    pmsprintf(buffer, sizeof(buffer), "%s%s/Stats", cifs_statspath, cifs_procfsdir);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL)
        return -oserror();

    while (fgets(buffer, sizeof(buffer) - 1, fp)) {
        if (strstr(buffer, "\\\\") == 0)
            continue;

        sscanf(buffer, "%*d%*s %s %*s", fsname);

        /* at this point fsname contains our cifs mount name: \\<server>\<share>
           this will be used to map stats to file-system instances */

        struct cifs_fs *fs;

	sts = pmdaCacheLookupName(indom, fsname, NULL, (void **)&fs);
	if (sts == PM_ERR_INST || (sts >= 0 && fs == NULL)){
	    fs = calloc(1, sizeof(struct cifs_fs));
            if (fs == NULL) {
                fclose(fp);
                return PM_ERR_AGAIN;
            }
            strcpy(fs->name, fsname);
        }
	else if (sts < 0)
	    continue;

	/* (re)activate this entry for the current query */
	pmdaCacheStore(indom, PMDA_CACHE_ADD, fsname, (void *)fs);
    }
    fclose(fp);
    return 0;
}

static int
cifs_debug_instance_refresh(void)
{
    int sts;
    char buffer[PATH_MAX], inst_name[PATH_MAX];
    FILE *fp;

    pmInDom debug_svr_indom = INDOM(CIFS_DEBUG_SVR_INDOM);
    pmInDom debug_session_indom = INDOM(CIFS_DEBUG_SESSION_INDOM);
    pmInDom debug_share_indom = INDOM(CIFS_DEBUG_SHARE_INDOM);

    pmdaCacheOp(debug_svr_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(debug_session_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(debug_share_indom, PMDA_CACHE_INACTIVE);

    /* update indom cache based off of /proc/fs/cifs/DebugData file */
    pmsprintf(buffer, sizeof(buffer), "%s%s/DebugData", cifs_statspath, cifs_procfsdir);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL)
        return -oserror();

    while (fgets(buffer, sizeof(buffer) - 1, fp)) {

        if (strstr(buffer, "ConnectionId:") != NULL) {
            sscanf(buffer, "%*d) %*s %*s %*s %s",
                inst_name);

            struct debug_server_stats *server_stats;

	    sts = pmdaCacheLookupName(debug_svr_indom, inst_name, NULL, (void **)&server_stats);
	    if (sts == PM_ERR_INST || (sts >= 0 && server_stats == NULL)){
	        server_stats = calloc(1, sizeof(struct debug_server_stats));
                if (server_stats == NULL) {
                    fclose(fp);
                    return PM_ERR_AGAIN;
                }
            }
	    else if (sts < 0)
	        continue;

	    /* (re)activate this entry for the current query */
	    pmdaCacheStore(debug_svr_indom, PMDA_CACHE_ADD, inst_name, (void *)server_stats);
        }

        if (strstr(buffer, ") Address:") != NULL) {
            int session_number;
            char address[PATH_MAX];

            sscanf(strtrim(buffer), "%d) %*s %s",
                &session_number,
                address);

            pmsprintf(inst_name, sizeof(inst_name), "%s::%d", address, session_number);

            struct debug_session_stats *session_stats;

	    sts = pmdaCacheLookupName(debug_session_indom, inst_name, NULL, (void **)&session_stats);
	    if (sts == PM_ERR_INST || (sts >= 0 && session_stats == NULL)){
	        session_stats = calloc(1, sizeof(struct debug_session_stats));
                if (session_stats == NULL) {
                    fclose(fp);
                    return PM_ERR_AGAIN;
                }
            }
	    else if (sts < 0)
	        continue;

	    /* (re)activate this entry for the current query */
	    pmdaCacheStore(debug_session_indom, PMDA_CACHE_ADD, inst_name, (void *)session_stats);
	}

        if (strstr(buffer, "DevInfo:") != NULL) {
            if (strstr(buffer, "IPC") != NULL) {
                sscanf(strtrim(buffer), "%*d) IPC: %s %*s %*d %*s %*s %*s %*s",
                    inst_name);
            } else {
                sscanf(strtrim(buffer), "%*d) %s %*s %*d %*s %*s %*s %*s",
                    inst_name);
            }

            struct debug_share_stats *share_stats;

	    sts = pmdaCacheLookupName(debug_share_indom, inst_name, NULL, (void **)&share_stats);
	    if (sts == PM_ERR_INST || (sts >= 0 && share_stats == NULL)){
	        share_stats = calloc(1, sizeof(struct debug_share_stats));
                if (share_stats == NULL) {
                    fclose(fp);
                    return PM_ERR_AGAIN;
                }
            }
	    else if (sts < 0)
	        continue;

	    /* (re)activate this entry for the current query */
	    pmdaCacheStore(debug_share_indom, PMDA_CACHE_ADD, inst_name, (void *)share_stats);      
        }
    }
    fclose(fp);
    return 0;
}

static int
cifs_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    cifs_instance_refresh();
    cifs_debug_instance_refresh();
    
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
cifs_fetch_refresh(pmdaExt *pmda, int *need_refresh)
{
    pmInDom indom = INDOM(CIFS_FS_INDOM);
    pmInDom debug_svr_indom = INDOM(CIFS_DEBUG_SVR_INDOM);
    pmInDom debug_session_indom = INDOM(CIFS_DEBUG_SESSION_INDOM);
    pmInDom debug_share_indom = INDOM(CIFS_DEBUG_SHARE_INDOM);
    struct cifs_fs *fs;
    struct debug_server_stats *server_stats;
    struct debug_session_stats *session_stats;
    struct debug_share_stats *share_stats;
    char *name;
    int i, sts;

    if ((sts = cifs_instance_refresh()) < 0)
	return sts;

    if ((sts = cifs_debug_instance_refresh()) < 0)
	return sts;

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, i, &name, (void **)&fs) || !fs)
	    continue;

        if (need_refresh[CLUSTER_FS_STATS])
            cifs_refresh_fs_stats(cifs_statspath, cifs_procfsdir, name, &fs->fs_stats);
    }
    if (need_refresh[CLUSTER_GLOBAL_STATS])
        cifs_refresh_global_stats(cifs_statspath, cifs_procfsdir, name);

    if (need_refresh[CLUSTER_DEBUG_GLOBAL_STATS])
        cifs_refresh_debug_stats(cifs_statspath, cifs_procfsdir);

    for (pmdaCacheOp(debug_svr_indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(debug_svr_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(debug_svr_indom, i, &name, (void **)&server_stats) || !server_stats)
	    continue;

        if (need_refresh[CLUSTER_DEBUG_SERVERS_STATS])
            cifs_refresh_debug_server_stats(cifs_statspath, cifs_procfsdir, name, server_stats);
    }

    for (pmdaCacheOp(debug_session_indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(debug_session_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(debug_session_indom, i, &name, (void **)&session_stats) || !session_stats)
	    continue;

        if (need_refresh[CLUSTER_DEBUG_SESSIONS_STATS])
            cifs_refresh_debug_session_stats(cifs_statspath, cifs_procfsdir, name, session_stats);
    }

    for (pmdaCacheOp(debug_share_indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(debug_share_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(debug_share_indom, i, &name, (void **)&share_stats) || !share_stats)
	    continue;

        if (need_refresh[CLUSTER_DEBUG_SHARES_STATS])
            cifs_refresh_debug_share_stats(cifs_statspath, cifs_procfsdir, name, share_stats);
    }

    return sts;
}

static int
cifs_fetch(int numpmid, pmID pmidlist[], pmdaResult **resp, pmdaExt *pmda)
{
    int i, sts, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	if (pmID_cluster(pmidlist[i]) < NUM_CLUSTERS)
	    need_refresh[pmID_cluster(pmidlist[i])]++;
    }

    if ((sts = cifs_fetch_refresh(pmda, need_refresh)) < 0)
	return sts;

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * callback provided to pmdaFetch
 */
static int
cifs_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    struct cifs_fs *fs;
    struct debug_server_stats *server_stats;
    struct debug_session_stats *session_stats;
    struct debug_share_stats *share_stats;
    int sts;

    switch (pmID_cluster(mdesc->m_desc.pmid)) {
    case CLUSTER_GLOBAL_STATS:
        return cifs_global_stats_fetch(pmID_item(mdesc->m_desc.pmid), atom);

    case CLUSTER_FS_STATS:
	sts = pmdaCacheLookup(INDOM(CIFS_FS_INDOM), inst, NULL, (void **)&fs);
	if (sts < 0)
	    return sts;
	return cifs_fs_stats_fetch(pmID_item(mdesc->m_desc.pmid), &fs->fs_stats, atom);

    case CLUSTER_DEBUG_GLOBAL_STATS:
        return cifs_debug_stats_fetch(pmID_item(mdesc->m_desc.pmid), atom);

    case CLUSTER_DEBUG_SERVERS_STATS:
        sts = pmdaCacheLookup(INDOM(CIFS_DEBUG_SVR_INDOM), inst, NULL, (void **)&server_stats);
	if (sts < 0)
	    return sts;
	return cifs_debug_server_stats_fetch(pmID_item(mdesc->m_desc.pmid), server_stats, atom);

    case CLUSTER_DEBUG_SESSIONS_STATS:
        sts = pmdaCacheLookup(INDOM(CIFS_DEBUG_SESSION_INDOM), inst, NULL, (void **)&session_stats);
	if (sts < 0)
	    return sts;
	return cifs_debug_session_stats_fetch(pmID_item(mdesc->m_desc.pmid), session_stats, atom);

    case CLUSTER_DEBUG_SHARES_STATS:	
        sts = pmdaCacheLookup(INDOM(CIFS_DEBUG_SHARE_INDOM), inst, NULL, (void **)&share_stats);
	if (sts < 0)
	    return sts;
	return cifs_debug_share_stats_fetch(pmID_item(mdesc->m_desc.pmid), share_stats, atom);

    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    /* NOTREACHED */
    return PMDA_FETCH_NOVALUES;
}

static int
cifs_text(int ident, int type, char **buf, pmdaExt *pmda)
{
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	int sts = pmdaDynamicLookupText(ident, type, buf, pmda);
	if (sts != -ENOENT)
	    return sts;
    }
    return pmdaText(ident, type, buf, pmda);
}

static int
cifs_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreePMID(tree, name, pmid);
}

static int
cifs_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupPMID(pmda, pmid);
    return pmdaTreeName(tree, pmid, nameset);
}

static int
cifs_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreeChildren(tree, name, flag, kids, sts);
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void
__PMDA_INIT_CALL
cifs_init(pmdaInterface *dp)
{
    char *envpath;
    char buffer[PATH_MAX];
    FILE *fp;

    if ((envpath = getenv("CIFS_STATSPATH")) != NULL)
	cifs_statspath = envpath;

    int	nindoms = sizeof(indomtable)/sizeof(indomtable[0]);
    int	nmetrics = sizeof(metrictable)/sizeof(metrictable[0]);

    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = pmPathSeparator();
	pmsprintf(helppath, sizeof(helppath), "%s%c" "cifs" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_4, "CIFS DSO", helppath);
    }

    if (dp->status != 0)
	return;

    memset(buffer, 0, sizeof(buffer));
    pmsprintf(buffer, sizeof(buffer), "%s%s/DebugData", cifs_statspath, cifs_procfsdir);
    buffer[sizeof(buffer)-1] = '\0';
    if ((fp = fopen(buffer, "r")) != NULL ) {
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	    /* global cifs stats */
	    if (strncmp(buffer, "CIFS Version", 12) == 0)
		sscanf(buffer, "CIFS Version %u.%u",
                       &global_version_major, &global_version_minor);
	}
	fclose(fp);
    }
    else {
	global_version_major = 1;
        global_version_minor = 0;
    }

    dp->version.four.instance = cifs_instance;
    dp->version.four.fetch = cifs_fetch;
    dp->version.four.text = cifs_text;
    dp->version.four.pmid = cifs_pmid;
    dp->version.four.name = cifs_name;
    dp->version.four.children = cifs_children;
    pmdaSetFetchCallBack(dp, cifs_fetchCallBack);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtable, nindoms, metrictable, nmetrics);
}

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:l:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int	sep = pmPathSeparator();
    pmdaInterface dispatch;
    char helppath[MAXPATHLEN];

    _isDSO = 0;

    pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "cifs" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmGetProgname(), CIFS, "cifs.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    cifs_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
