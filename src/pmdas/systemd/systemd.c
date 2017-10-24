/*
 * systemd support for the systemd PMDA
 *
 * Copyright (c) 2012-2014 Red Hat.
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
 * Structure based upon the logger pmda.
 */

#define _POSIX_C_SOURCE 200112L  /* for strtoull */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"

#include <systemd/sd-journal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <grp.h>

#define DEFAULT_MAXMEM  (2 * 1024 * 1024)       /* 2 megabytes */
long maxmem;
int maxfd;
fd_set fds;
static int interval_expired;
static struct timeval interval = { 60, 0 };
static sd_journal *journald_context; /* Used for monitoring only. */
static sd_journal *journald_context_seeky; /* Used for event detail extraction,
                                              involving seeks. */
static int queue_entries = -1;
static char *username = "adm";


/* Track per-context PCP_ATTR_USERID | _GROUPID, so we
   can filter event records for that context. */
static int uid_gid_filter_p = 1;
struct uid_gid_tuple {
    char wildcard_p; /* do not filter for this context. */
    char uid_p; char gid_p; /* uid/gid received flags. */
    int uid; int gid; }; /* uid/gid received from PCP_ATTR_* */
static struct uid_gid_tuple *ctxtab = NULL;
int ctxtab_size = 0;


static pmdaMetric metrictab[] = {
/* numclients */
#define METRICTAB_NUMCLIENTS_PMID metrictab[0].m_desc.pmid
    { NULL,
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* maxmem */
#define METRICTAB_MAXMEM_PMID metrictab[1].m_desc.pmid
    { NULL,
      { PMDA_PMID(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* journal.field.cursor */
#define METRICTAB_JOURNAL_CURSOR_PMID metrictab[2].m_desc.pmid
    { NULL,
      { PMDA_PMID(1,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* journal.field.string */
#define METRICTAB_JOURNAL_STRING_PMID metrictab[3].m_desc.pmid
    { NULL,
      { PMDA_PMID(1,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* journal.field.blob */
#define METRICTAB_JOURNAL_BLOB_PMID metrictab[4].m_desc.pmid
    { NULL,
      { PMDA_PMID(1,2), PM_TYPE_AGGREGATE, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* journal.records */
#define METRICTAB_JOURNAL_RECORDS_PMID metrictab[5].m_desc.pmid
    { NULL,
      { PMDA_PMID(2,0), PM_TYPE_EVENT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* journal.records_raw */
#define METRICTAB_JOURNAL_RECORDS_RAW_PMID metrictab[6].m_desc.pmid
    { NULL,
      { PMDA_PMID(2,1), PM_TYPE_EVENT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* journal.count */
#define METRICTAB_JOURNAL_COUNT_PMID metrictab[7].m_desc.pmid
    { NULL,
      { PMDA_PMID(2,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* journal.bytes */
#define METRICTAB_JOURNAL_BYTES_PMID metrictab[8].m_desc.pmid
    { NULL,
      { PMDA_PMID(2,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
};


void systemd_shutdown(void)
{
    if (journald_context != 0)
        sd_journal_close (journald_context);

    if (journald_context_seeky != 0)
        sd_journal_close (journald_context_seeky);

    /* XXX: pmdaEvent zap queues? */
}


/* Return a strndup (or NULL) of a field of the current journal entry,
   since sd_journal_get_data returns data that is not
   \0-terminated. */
char *
my_sd_journal_get_data(sd_journal *j, const char *field)
{
    int rc;
    const char* str;
    size_t str_len;

    assert (j != NULL);
    assert (field != NULL);

    rc = sd_journal_get_data(j, field,
                             (const void**) & str, & str_len);
    if (rc < 0)
        return NULL;

    return strndup (str, str_len);
}


void systemd_refresh(void)
{
    /* Absorb any changes such as inotify() messages. */
    (void) sd_journal_process(journald_context);
    (void) sd_journal_process(journald_context_seeky);

    while (1) {
        char *cursor = NULL;
        char *timestamp_str = NULL;
        struct timeval timestamp;

        int rc = sd_journal_next(journald_context);

        if (rc == 0) /* No recent entries. */
            break;

        if (rc < 0) {
            __pmNotifyErr(LOG_ERR, "sd_journal_next failure: %s", strerror(-rc));
            break;
        }

        /* NB: we enqueue the journal cursor string, rather than the
           actual journal records. */
        rc = sd_journal_get_cursor(journald_context, &cursor);
        if (rc < 0) {
            __pmNotifyErr(LOG_ERR, "sd_journal_get_cursor failure: %s",
                          strerror(-rc));
            break;
        }

        /* Extract a timestamp from the journald event fields. */
        timestamp_str = my_sd_journal_get_data(journald_context,
                                               "_SOURCE_REALTIME_TIMESTAMP");
        if (timestamp_str == NULL)
            timestamp_str = my_sd_journal_get_data(journald_context,
                                                   "__REALTIME_TIMESTAMP");
        if (timestamp_str == NULL)
            rc = -ENOMEM;
        else {
            const char* curse;
            unsigned long long epoch_us;
            /* defined in systemd.journal-fields(7) as
               FIELD_NAME=NNNN, where NNNN is decimal us since epoch. */
            curse = strchr (timestamp_str, '=');
            if (curse == NULL)
                rc = -EINVAL;
            else {
                curse ++;
                epoch_us = strtoull (curse, NULL, 10);
                timestamp.tv_sec  = epoch_us / 1000000;
                timestamp.tv_usec = epoch_us % 1000000;
            }
            free (timestamp_str);
        }
        /* Improvise. */
        if (rc < 0)
            gettimeofday (& timestamp, NULL);

        /* Enqueue it to fresh visitors. */
        rc = pmdaEventQueueAppend(queue_entries,
                                  cursor, strlen(cursor)+1 /* \0 */, &timestamp);
        free(cursor); /* Already copied. */
        if (rc < 0) {
            __pmNotifyErr(LOG_ERR, "pmdaEventQueueAppend failure: %s", pmErrStr(rc));
            break;
        }
    }
}



enum journald_field_encoding {
    JFE_STRING_BLOB_AUTO,
    JFE_BLOB_ONLY
};



int
systemd_journal_event_filter (void *rp, void *data, size_t size)
{
    int rc;
    struct uid_gid_tuple* ugt = rp;

    assert (ugt == & ctxtab[pmdaGetContext()]);
    if (pmDebugOptions.appl0)
        __pmNotifyErr(LOG_DEBUG, "filter (%d) uid=%d gid=%d data=%p bytes=%u\n",
                      pmdaGetContext(), ugt->uid, ugt->gid, data, (unsigned)size);

    /* The data/size pair gives the object in the event queue, i.e.,
       the systemd journal cursor string.  It has not yet been turned
       into a PM_TYPE_EVENT tuple yet, and if we have our way, it won't
       be (for non-participating clients). */

    /* The general filtering idea is to only feed journal records to clients
       if their uid matches the _UID=NNN field -or- gid matches _GID=MMM
       -or- the client is highly authenticated (wildcard_p) -or- per-uid filtering
       was turned off at the pmda level. */

    /* Reminder: function rc == 0 passes the filter. */

    /* Unfiltered?  Everyone gets egg soup! */
    if (! uid_gid_filter_p)
        return 0;

    if (pmDebugOptions.appl0)
        __pmNotifyErr(LOG_DEBUG, "filter (%d) uid%s%d gid%s%d wildcard=%d\n",
                      pmdaGetContext(),
                      ugt->uid_p?"=":"?", ugt->uid,
                      ugt->gid_p?"=":"?", ugt->gid,
                      ugt->wildcard_p);

    /* Superuser?  May we offer some goulash? */
    if (ugt->wildcard_p)
        return 0;

    /* Unauthenticated context?  No soup for you! */
    if (! ugt->uid_p && ! ugt->gid_p)
        return 1;

    /* OK, we need to take a look at the journal record in question. */

    if (pmDebugOptions.appl0)
        __pmNotifyErr(LOG_DEBUG, "filter cursor=%s\n", (const char*) data);

    (void) size; /* already known \0-terminated */
    rc = sd_journal_seek_cursor(journald_context_seeky, (char*) data);
    if (rc < 0) {
        __pmNotifyErr(LOG_ERR, "filter cannot seek to cursor=%s\n",
                      (const char*) data);
        return 1; /* No point trying again in systemd_journal_decoder. */
    }

    rc = sd_journal_next(journald_context_seeky);
    if (rc < 0) {
        __pmNotifyErr(LOG_ERR, "filter cannot advance to next\n");
        return 1; /* No point trying again in systemd_journal_decoder. */
    }

    if (ugt->uid_p) {
        char *uid_str = my_sd_journal_get_data(journald_context_seeky, "_UID");
        if (uid_str) {
            int uid = atoi (& uid_str[5]); /* skip over _UID= */
            free (uid_str);
            if (uid == ugt->uid)
                return 0; /* You're a somebody.  Here's a bowl of stew. */
        }
    }

    if (ugt->gid_p) {
        char *gid_str = my_sd_journal_get_data(journald_context_seeky, "_GID");
        if (gid_str) {
            int gid = atoi (& gid_str[5]); /* skip over _GID= */
            free (gid_str);
            if (gid == ugt->gid)
                return 0; /* You're with pals.  Here's a bowl of miso. */
        }
    }

    /* No soup for you! */
    return 1;
}


void
systemd_journal_event_filter_release (void *rp)
{
    /* NB: We have nothing to release, as we don't do memory allocation
       for the filter per se - we clean up during end-context time.
       We can't send a NULL to pmdaEventSetFilter for release purposes
       (since it'll blindly call it), so need this dummy function. */

    (void) rp;
}


int
systemd_journal_decoder(int eventarray, void *buffer, size_t size,
                        struct timeval *timestamp, void *data)
{
    int sts;
    pmAtomValue atom;
    enum journald_field_encoding jfe = * (enum journald_field_encoding *) data;

    sts = pmdaEventAddRecord(eventarray, timestamp, PM_EVENT_FLAG_POINT);
    if (sts < 0)
        return sts;

    /* Go to the cursor point enqueued for this client.  The buffer is already
       \0-terminated. */
    sts = sd_journal_seek_cursor(journald_context_seeky, (char*) buffer);
    if (sts < 0) {
        /* But see RHBZ #876654. */
        return /* sts */ 0;
    }

    sts = sd_journal_next(journald_context_seeky);
    if (sts < 0)
        return sts;
    if (sts == 0)
        return -ENODATA; /* event got lost between cursor-recording and now */

    /* Add the _CURSOR implicit journal field. */
    atom.cp = buffer;
    sts = pmdaEventAddParam(eventarray, METRICTAB_JOURNAL_CURSOR_PMID,
                            PM_TYPE_STRING, &atom);

    /* Add all the explicit journal fields. */
    while (1) {
        const void *data;
        size_t data_len;

        if (sts < 0)
            break;
        sts = sd_journal_enumerate_data(journald_context_seeky, &data, &data_len);
        if (sts <= 0)
            break;

        /* Infer string upon absence of embedded \0's. */
        if (jfe == JFE_STRING_BLOB_AUTO && (memchr (data, '\0', data_len) == NULL)) {
            /* Unfortunately, data may not be \0-terminated, so we can't simply pass
               it to atom.cp.  We need to copy the bad boy first. */
            atom.cp = strndup(data, data_len);
            if (atom.cp == NULL)
                sts = -ENOMEM;
            else {
                sts = pmdaEventAddParam(eventarray, METRICTAB_JOURNAL_STRING_PMID,
                                        PM_TYPE_STRING, &atom);
                free (atom.cp);
            }
            /* NB: we assume libpcp_pmda will not free() the field. */
        } else {
            pmValueBlock *aggr = (pmValueBlock *)malloc(PM_VAL_HDR_SIZE + data_len);
            if (aggr == NULL)
                sts = -ENOMEM;
            else {
                aggr->vtype = PM_TYPE_AGGREGATE;
                if (PM_VAL_HDR_SIZE + data_len >= 1<<24)
                    aggr->vlen = (1U<<24) - 1; /* vlen is a :24 bit field */
                else
                    aggr->vlen = PM_VAL_HDR_SIZE + data_len;
                memcpy (aggr->vbuf, data, data_len);
                atom.vbp = aggr;
                sts = pmdaEventAddParam(eventarray, METRICTAB_JOURNAL_BLOB_PMID,
                                        PM_TYPE_AGGREGATE, &atom);
                /* NB: we assume libpcp_pmda will free() aggr. */
            }
        }
    }

   return sts < 0 ? sts : 1;    /* added one event array */
}


void enlarge_ctxtab(int context)
{
    /* Grow the context table if necessary. */
    if (ctxtab_size /* cardinal */ <= context /* ordinal */) {
        size_t need = (context + 1) * sizeof(struct uid_gid_tuple);
        ctxtab = realloc (ctxtab, need);
        if (ctxtab == NULL)
            __pmNoMem("systemd ctx table", need, PM_FATAL_ERR);
        /* Blank out new entries. */
        while (ctxtab_size <= context)
            memset (& ctxtab[ctxtab_size++], 0, sizeof(struct uid_gid_tuple));
    }
}


static int
systemd_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int sts;
    (void) pmdaEventNewClient(pmda->e_context);
    enlarge_ctxtab(pmda->e_context);
    sts = pmdaEventSetFilter(pmda->e_context, queue_entries,
                             & ctxtab[pmda->e_context], /* any non-NULL value */
                             systemd_journal_event_filter,
                             systemd_journal_event_filter_release /* NULL */);
    if (sts < 0)
        return sts;
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}


static int
systemd_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    pmID id = mdesc->m_desc.pmid;
    int sts;

    if (id == METRICTAB_NUMCLIENTS_PMID) {
        sts = pmdaEventClients(atom);
    } else if (id == METRICTAB_MAXMEM_PMID) {
        atom->ul = (unsigned long)maxmem;
        sts = PMDA_FETCH_STATIC;
    } else if (id == METRICTAB_JOURNAL_CURSOR_PMID) {
        sts = PMDA_FETCH_NOVALUES;
    } else if (id == METRICTAB_JOURNAL_STRING_PMID) {
        sts = PMDA_FETCH_NOVALUES;
    } else if (id == METRICTAB_JOURNAL_BLOB_PMID) {
        sts = PMDA_FETCH_NOVALUES;
    } else if (id == METRICTAB_JOURNAL_COUNT_PMID) {
	sts = pmdaEventQueueCounter(queue_entries, atom);
    } else if (id == METRICTAB_JOURNAL_BYTES_PMID) {
	sts = pmdaEventQueueBytes(queue_entries, atom);
    } else if (id == METRICTAB_JOURNAL_RECORDS_PMID) {
        enum journald_field_encoding jfe = JFE_STRING_BLOB_AUTO;
        sts = pmdaEventSetAccess(pmdaGetContext(), queue_entries, 1);
        if (sts == 0)
            sts = pmdaEventQueueRecords(queue_entries, atom, pmdaGetContext(),
                                        systemd_journal_decoder, & jfe);
    } else if (id == METRICTAB_JOURNAL_RECORDS_RAW_PMID) {
        enum journald_field_encoding jfe = JFE_BLOB_ONLY;
        sts = pmdaEventSetAccess(pmdaGetContext(), queue_entries, 1);
        if (sts == 0)
            sts = pmdaEventQueueRecords(queue_entries, atom, pmdaGetContext(),
                                        systemd_journal_decoder, & jfe);
    } else {
        sts = PM_ERR_PMID;
    }
    return sts;
}


static int
systemd_contextAttributeCallBack(int context,
                                 int attr, const char *value, int length, pmdaExt *pmda)
{
    static int rootlike_gids_found = 0;
    static int adm_gid = -1;
    static int wheel_gid = -1;
    static int systemd_journal_gid = -1;
    int id;

    /* Look up root-like gids if needed.  A later PCP client that
       matches any of these group-id's is treated as if root/adm,
       i.e., journal records are not filtered for them (wildcard_p).
       XXX: we could  examine group-membership lists and check against
       uid to also set wildcard_p. */
    if (! rootlike_gids_found) {
        struct group *grp;
        grp = getgrnam("adm");
        if (grp) adm_gid = grp->gr_gid;
        grp = getgrnam("wheel");
        if (grp) wheel_gid = grp->gr_gid;
        grp = getgrnam("systemd-journal");
        if (grp) systemd_journal_gid = grp->gr_gid;
        rootlike_gids_found = 1;
    }

    enlarge_ctxtab(context);
    assert (ctxtab != NULL && context < ctxtab_size);

    /* NB: we maintain separate uid_p and gid_p for filtering
       purposes; it's possible that a pcp client might send only
       PCP_ATTR_USERID, leaving gid=0, possibly leading us to
       misinterpret that as GROUPID=0 (root) and sending back _GID=0
       records. */
    switch (attr) {
    case PCP_ATTR_USERID:
        ctxtab[context].uid_p = 1;
        id = atoi(value);
        ctxtab[context].uid = id;
        if (id == 0) /* root */
            ctxtab[context].wildcard_p = 1;
        break;

    case PCP_ATTR_GROUPID:
        ctxtab[context].gid_p = 1;
        id = atoi(value);
        ctxtab[context].gid = id;
        if (id == adm_gid ||
            id == wheel_gid ||
            id == systemd_journal_gid)
            ctxtab[context].wildcard_p = 1;
        break;
    }

    if (pmDebugOptions.appl0)
        __pmNotifyErr(LOG_DEBUG, "attrib (%d) uid%s%d gid%s%d wildcard=%d\n",
                      context,
                      ctxtab[context].uid_p?"=":"?", ctxtab[context].uid,
                      ctxtab[context].gid_p?"=":"?", ctxtab[context].gid,
                      ctxtab[context].wildcard_p);

    return 0;
}


static void
systemd_end_contextCallBack(int context)
{
    pmdaEventEndClient(context);

    /* assert (ctxtab != NULL && context < ctxtab_size); */

    /* NB: don't do that; this callback may be hit without any fetch
       calls having been performed, this ctxtab not stretching all the
       way to [context]. */

    if (context < ctxtab_size)
        memset (& ctxtab[context], 0, sizeof(struct uid_gid_tuple));
}


static int
systemd_desc(pmID pmid, pmDesc *desc, pmdaExt *pmda)
{
    return pmdaDesc(pmid, desc, pmda);
}


static int
systemd_text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    return pmdaText(ident, type, buffer, pmda);
}


void
systemd_init(pmdaInterface *dp)
{
    int sts;
    int journal_fd;

    dp->version.six.desc = systemd_desc;
    dp->version.six.fetch = systemd_fetch;
    dp->version.six.text = systemd_text;
    dp->version.six.attribute = systemd_contextAttributeCallBack;
    pmdaSetFetchCallBack(dp, systemd_fetchCallBack);
    pmdaSetEndContextCallBack(dp, systemd_end_contextCallBack);
    pmdaInit(dp, NULL, 0, metrictab, sizeof(metrictab)/sizeof(metrictab[0]));

    /* Initialize the systemd side.  This is failure-tolerant.  */
    /* XXX: SD_JOURNAL_{LOCAL|RUNTIME|SYSTEM}_ONLY */
    sts = sd_journal_open(& journald_context, 0);
    if (sts < 0) {
        __pmNotifyErr(LOG_ERR, "sd_journal_open failure: %s",
                      strerror(-sts));
        dp->status = sts;
        return;
    }

    sts = sd_journal_open(& journald_context_seeky, 0);
    if (sts < 0) {
        __pmNotifyErr(LOG_ERR, "sd_journal_open #2 failure: %s",
                      strerror(-sts));
        dp->status = sts;
        return;
    }

    sts = sd_journal_seek_tail(journald_context);
    if (sts < 0) {
        __pmNotifyErr(LOG_ERR, "sd_journal_seek_tail failure: %s",
                      strerror(-sts));
    }

    /* Work around RHBZ979487. */
    sts = sd_journal_previous_skip(journald_context, 1);
    if (sts < 0) {
        __pmNotifyErr(LOG_ERR, "sd_journal_previous_skip failure: %s",
                      strerror(-sts));
    }

    /* Arrange to wake up for journal events. */
    journal_fd = sd_journal_get_fd(journald_context);
    if (journal_fd < 0) {
        __pmNotifyErr(LOG_ERR, "sd_journal_get_fd failure: %s",
                      strerror(-journal_fd));
        /* NB: not a fatal error; the select() loop will still time out and
           periodically poll.  This makes it ok for sd_journal_reliable_fd()
           to be 0. */
    } else  {
        FD_SET(journal_fd, &fds);
        if (journal_fd > maxfd) maxfd = journal_fd;
    }

    /* NB: One queue is used for both .records and .records_raw; they
       just use different decoder callbacks. */
    queue_entries = pmdaEventNewQueue("systemd", maxmem);
    if (queue_entries < 0)
        __pmNotifyErr(LOG_ERR, "pmdaEventNewQueue failure: %s",
                      pmErrStr(queue_entries));
}


void
systemdMain(pmdaInterface *dispatch)
{
    int pmcdfd;

    pmcdfd = __pmdaInFd(dispatch);
    if (pmcdfd > maxfd)
        maxfd = pmcdfd;

    FD_SET(pmcdfd, &fds);

    for (;;) {
        fd_set readyfds;
        int nready;
        struct timeval select_timeout = interval;

        memcpy(&readyfds, &fds, sizeof(readyfds));
        nready = select(maxfd+1, &readyfds, NULL, NULL, & select_timeout);
        if (pmDebugOptions.appl2)
            __pmNotifyErr(LOG_DEBUG, "select: nready=%d interval=%d",
                          nready, interval_expired);
        if (nready < 0) {
            if (neterror() != EINTR) {
                __pmNotifyErr(LOG_ERR, "select failure: %s", netstrerror());
                exit(1);
            } else if (!interval_expired) {
                continue;
            }
        }

        if (nready > 0 && FD_ISSET(pmcdfd, &readyfds)) {
            if (pmDebugOptions.appl0)
                __pmNotifyErr(LOG_DEBUG, "processing pmcd PDU [fd=%d]", pmcdfd);
            if (__pmdaMainPDU(dispatch) < 0) {
                exit(1);        /* fatal if we lose pmcd */
            }
            if (pmDebugOptions.appl0)
                __pmNotifyErr(LOG_DEBUG, "completed pmcd PDU [fd=%d]", pmcdfd);
        }
        systemd_refresh();
    }
}


static void
convertUnits(char **endnum, long *maxmem)
{
    switch ((int) **endnum) {
        case 'b':
        case 'B':
                break;
        case 'k':
        case 'K':
                *maxmem *= 1024;
                break;
        case 'm':
        case 'M':
                *maxmem *= 1024 * 1024;
                break;
        case 'g':
        case 'G':
                *maxmem *= 1024 * 1024 * 1024;
                break;
    }
    (*endnum)++;
}


static void
usage(void)
{
    fprintf(stderr,
            "Usage: %s [options]\n\n"
            "Options:\n"
            "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
            "  -l logfile   write log into logfile rather than using default log name\n"
            "  -m memory    maximum memory used per queue (default %ld bytes)\n"
            "  -s interval  default delay between iterations (default %d sec)\n"
            "  -U username  user account to run under (default \"adm\")\n"
            "  -f           disable per-uid/gid record filtering (default on)\n",
            pmProgname, maxmem, (int)interval.tv_sec);
    exit(1);
}


int
main(int argc, char **argv)
{
    static char         helppath[MAXPATHLEN];
    char                *endnum;
    pmdaInterface       desc;
    long                minmem;
    int                 c, err = 0, sep = __pmPathSeparator();

    minmem = getpagesize();
    maxmem = (minmem > DEFAULT_MAXMEM) ? minmem : DEFAULT_MAXMEM;
    __pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "systemd" "%c" "help",
                pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_6, pmProgname, SYSTEMD,
                "systemd.log", helppath);

    while ((c = pmdaGetOpt(argc, argv, "D:d:l:m:s:U:f?", &desc, &err)) != EOF) {
        switch (c) {
            case 'm':
                maxmem = strtol(optarg, &endnum, 10);
                if (*endnum != '\0')
                    convertUnits(&endnum, &maxmem);
                if (*endnum != '\0' || maxmem < minmem) {
                    fprintf(stderr, "%s: invalid max memory '%s' (min=%ld)\n",
                            pmProgname, optarg, minmem);
                    err++;
                }
                break;

            case 's':
                if (pmParseInterval(optarg, &interval, &endnum) < 0) {
                    fprintf(stderr, "%s: -s requires a time interval: %s\n",
                            pmProgname, endnum);
                    free(endnum);
                    err++;
                }
                break;

            case 'U':
                username = optarg;
                break;

            case 'f':
                uid_gid_filter_p = 0;
                break;

            default:
                err++;
                break;
        }
    }

    if (err)
        usage();

    FD_ZERO (&fds);
    pmdaOpenLog(&desc);

    /* The systemwide journal may be accessed by the adm user (group);
       root access is not necessary. */
    __pmSetProcessIdentity(username);
    desc.comm.flags |= PDU_FLAG_AUTH;
    pmdaConnect(&desc);
    // After this point, systemd_init is allowed to take some extra time.
    systemd_init(&desc); // sets some fds
    systemdMain(&desc); // sets some more fds
    systemd_shutdown();
    exit(0);
}

/*
  Local Variables:
  c-basic-offset: 4
  End:
*/
