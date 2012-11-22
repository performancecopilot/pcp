/*
 * systemd support for the systemd PMDA
 *
 * Copyright (c) 2012 Red Hat Inc.
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



int _isDSO = 1;

#define DEFAULT_MAXMEM  (2 * 1024 * 1024)       /* 2 megabytes */
long maxmem;
int maxfd;
fd_set fds;
static int interval_expired;
static struct timeval interval = { 2, 0 };

static sd_journal* journald_context = NULL; /* Used for monitoring only. */
static sd_journal* journald_context_seeky = NULL; /* Used for event detail extraction,
                                                     involving seeks. */
static int queue_entries = -1;

static __pmnsTree *pmns;

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
        PMDA_PMUNITS(0,0,1,0,0,0) }, },
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
/* systemd.records_raw */
#define METRICTAB_JOURNAL_RECORDS_RAW_PMID metrictab[6].m_desc.pmid
    { NULL,
      { PMDA_PMID(2,1), PM_TYPE_EVENT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
};






void systemd_shutdown(void)
{
    if (journald_context >= 0)
        sd_journal_close (journald_context);

    if (journald_context_seeky >= 0)
        sd_journal_close (journald_context_seeky);

    /* XXX: pmdaEvent zap queues? */
}


void systemd_refresh(void)
{
    while (1) {
        char *cursor = NULL;
        char *timestamp_str = NULL;
        size_t timestamp_len = 0;
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
        rc = sd_journal_get_data(journald_context, "_SOURCE_REALTIME_TIMESTAMP",
                                 (const void**) & timestamp_str, & timestamp_len);
        if (rc < 0)
            rc = sd_journal_get_data(journald_context, "__REALTIME_TIMESTAMP",
                                     (const void**) & timestamp_str, & timestamp_len);
        if (rc == 0) {
            unsigned long long epoch_us;
            assert (timestamp_str != NULL);
            /* defined in systemd.journal-fields(7) as
               FIELD_NAME=NNNN, where NNNN is decimal us since epoch. */
            timestamp_str = strchr (timestamp_str, '=');
            if (timestamp_str == NULL)
                rc = -1;
            else {
                assert (timestamp_str != NULL);
                timestamp_str ++;
                epoch_us = strtoull (timestamp_str, NULL, 10);
                timestamp.tv_sec  = epoch_us / 1000000;
                timestamp.tv_usec = epoch_us % 1000000;
            }
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



static int
systemd_profile(__pmProfile *prof, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return 0;
}

static int
systemd_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    if (_isDSO) { /* poll, in lieu of systemdMain(). */
        systemd_refresh();
    }

    pmdaEventNewClient(pmda->e_context);
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
        atom->ull = (unsigned long long)maxmem;
        sts = PMDA_FETCH_STATIC;
    } else if (id == METRICTAB_JOURNAL_CURSOR_PMID) {
        sts = PMDA_FETCH_NOVALUES;
    } else if (id == METRICTAB_JOURNAL_STRING_PMID) {
        sts = PMDA_FETCH_NOVALUES;
    } else if (id == METRICTAB_JOURNAL_BLOB_PMID) {
        sts = PMDA_FETCH_NOVALUES;
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
systemd_store(pmResult *result, pmdaExt *pmda)
{
    int i;

    pmdaEventNewClient(pmda->e_context); /* since there is no storeCallback */

    for (i = 0; i < result->numpmid; i++) {
        pmValueSet *vsp = result->vset[i];
        pmID id = vsp->pmid;
        int sts;

        (void) id;
        /* NB: nothing writeable at the moment. */
        sts = PM_ERR_PERMISSION;
        if (sts < 0 )
            return sts;
    }
    return 0;
}

static void
systemd_end_contextCallBack(int context)
{
    pmdaEventEndClient(context);
}

static int
systemd_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaTreePMID(pmns, name, pmid);
}

static int
systemd_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaTreeName(pmns, pmid, nameset);
}

static int
systemd_children(const char *name, int traverse, char ***kids, int **sts,
                pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaTreeChildren(pmns, name, traverse, kids, sts);
}

static int
systemd_text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaText(ident, type, buffer, pmda);
}



void
systemd_init(pmdaInterface *dp)
{
    int sts;
    int journal_fd;

    if (_isDSO) {
        char helppath[MAXPATHLEN];
        int sep = __pmPathSeparator();
        snprintf(helppath, sizeof(helppath), "%s%c" "systemd" "%c" "help",
                 pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
        pmdaDSO(dp, PMDA_INTERFACE_5, "systemd DSO", helppath);
        /* A user's own journal may be accessed without process
           identity changes. */
    } else {
        /* The systemwide journal may be accessed by the adm user (group);
           root access is not necessary. */
        __pmSetProcessIdentity("adm");
    }

    dp->version.four.fetch = systemd_fetch;
    dp->version.four.store = systemd_store;
    dp->version.four.profile = systemd_profile;
    dp->version.four.pmid = systemd_pmid;
    dp->version.four.name = systemd_name;
    dp->version.four.children = systemd_children;
    dp->version.four.text = systemd_text;

    pmdaSetFetchCallBack(dp, systemd_fetchCallBack);
    pmdaSetEndContextCallBack(dp, systemd_end_contextCallBack);

    pmdaInit(dp, NULL, 0, metrictab, sizeof(metrictab)/sizeof(metrictab[0]));

    /* Create the dynamic PMNS tree and populate it. */
    if ((sts = __pmNewPMNS(&pmns)) < 0) {
        __pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
                        pmProgname, pmErrStr(sts));
        pmns = NULL;
        return;
    }

    /* Initialize the systemd side.  This is failure-tolerant.  */
    /* XXX: SD_JOURNAL_{LOCAL|RUNTIME|SYSTEM}_ONLY */
    int rc = sd_journal_open(& journald_context, 0);
    if (rc < 0) {
        __pmNotifyErr(LOG_ERR, "sd_journal_open failure: %s",
                      strerror(-rc));
        return;
    }

    rc = sd_journal_open(& journald_context_seeky, 0);
    if (rc < 0) {
        __pmNotifyErr(LOG_ERR, "sd_journal_open #2 failure: %s",
                      strerror(-rc));
        return;
    }

    rc = sd_journal_seek_tail(journald_context);
    if (rc < 0) {
        __pmNotifyErr(LOG_ERR, "sd_journal_seek_tail failure: %s",
                      strerror(-rc));
    }

    /* Arrange to wake up for journal events. */
    journal_fd = sd_journal_get_fd(journald_context);
    if (journal_fd < 0) {
        __pmNotifyErr(LOG_ERR, "sd_journal_get_fd failure: %s",
                      strerror(-journal_fd));
        /* NB: not a fatal error; the select() loop will stil time out and
           periodically poll.  This makes it ok for sd_journal_reliable_fd()
           to be 0. */
    } else  {
        FD_SET(journal_fd, &fds);
        if (journal_fd > maxfd) maxfd = journal_fd;
    }

    /* NB: One queue is used for both .entries and .entries_raw; they
       just use different decoder callbacks. */
    queue_entries = pmdaEventNewQueue("systemd.journal.entries", maxmem);
    if (queue_entries < 0)
        __pmNotifyErr(LOG_ERR, "pmdaEventNewQueue failure: %s",
                      pmErrStr(queue_entries));
}


void
systemdMain(pmdaInterface *dispatch)
{
    fd_set              readyfds;
    int                 nready, pmcdfd;

    pmcdfd = __pmdaInFd(dispatch);
    if (pmcdfd > maxfd)
        maxfd = pmcdfd;

    FD_ZERO(&fds);
    FD_SET(pmcdfd, &fds);

    for (;;) {
        struct timeval select_timeout = interval;
        memcpy(&readyfds, &fds, sizeof(readyfds));
        nready = select(maxfd+1, &readyfds, NULL, NULL, & select_timeout);
        if (pmDebug & DBG_TRACE_APPL2)
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
            if (pmDebug & DBG_TRACE_APPL0)
                __pmNotifyErr(LOG_DEBUG, "processing pmcd PDU [fd=%d]", pmcdfd);
            if (__pmdaMainPDU(dispatch) < 0) {
                exit(1);        /* fatal if we lose pmcd */
            }
            if (pmDebug & DBG_TRACE_APPL0)
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
            "  -m memory    maximum memory used per logfile (default %ld bytes)\n"
            "  -s interval  default delay between iterations (default %d sec)\n",
            pmProgname, maxmem, (int)interval.tv_sec);
    exit(1);
}


/* For use by the DSO invocation. */


int
main(int argc, char **argv)
{
    static char         helppath[MAXPATHLEN];
    char                *endnum;
    pmdaInterface       desc;
    long                minmem;
    int                 c, err = 0, sep = __pmPathSeparator();

    _isDSO = 0;

    minmem = getpagesize();
    maxmem = (minmem > DEFAULT_MAXMEM) ? minmem : DEFAULT_MAXMEM;
    __pmSetProgname(argv[0]);
    snprintf(helppath, sizeof(helppath), "%s%c" "systemd" "%c" "help",
                pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_5, pmProgname, SYSTEMD,
                "systemd.log", helppath);

    while ((c = pmdaGetOpt(argc, argv, "D:d:l:m:s:?", &desc, &err)) != EOF) {
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

            default:
                err++;
                break;
        }
    }

    if (err)
        usage();

    pmdaOpenLog(&desc);
    systemd_init(&desc);
    pmdaConnect(&desc);
    systemdMain(&desc);
    systemd_shutdown();
    exit(0);
}



/*
  Local Variables:
  c-basic-offset: 4
  End:
*/

