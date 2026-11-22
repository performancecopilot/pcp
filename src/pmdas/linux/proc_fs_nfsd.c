/*
 * Linux /proc/fs/nfsd metrics cluster
 *
 * Copyright (c) 2017-2025 Red Hat.
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
#include "linux.h"
#include "proc_fs_nfsd.h"

/*
 * The client name from /proc/fs/nfsd/clients/<client>/info is a quoted
 * value holding NFS implementation details followed by the client
 * supplied hostname, e.g.
 *
 *     name: "Linux NFSv4.2 host.example.com"
 *
 * The hostname is the last whitespace separated token.  Locate it in the
 * complete quoted value (which can be longer than the destination field)
 * and only then copy it out, bounded by the caller supplied size.
 */
static void
nfs4_client_hostname(const char *buf, char *hostname, size_t size)
{
    const char *start, *end, *p, *last = NULL;

    if ((start = strstr(buf, "name: \"")) == NULL)
	return;
    start += 7;
    if ((end = strchr(start, '"')) == NULL)
	return;
    for (p = start; p < end; p++) {
	if (*p == ' ')
	    last = p;
    }
    if (last != NULL)
	start = last + 1;
    pmsprintf(hostname, size, "%.*s", (int)(end - start), start);
}

/*
 * Newer kernels use '-' as a placeholder when printing access modes
 * in /proc/fs/nfsd/clients/<client>/states (e.g. "r-" or "-w"),
 * whereas older kernels simply omitted the unset mode.  Strip the
 * placeholders so the metric value does not change across kernel
 * versions.
 */
static void
nfs4_strip_access_placeholders(char *access)
{
    char *p = access;

    while (*access) {
	if (*access != '-')
	    *p++ = *access;
	access++;
    }
    *p = '\0';
}

/*
 * Find the last occurrence of needle in haystack, or NULL.
 */
static const char *
nfs4_strrstr(const char *haystack, const char *needle)
{
    const char *p, *last = NULL;

    while ((p = strstr(haystack, needle)) != NULL) {
	last = p;
	haystack = p + 1;
    }
    return last;
}

/*
 * Find the closing quote of a filename field.  The kernel does not
 * escape quote characters within the path, so the first quote after the
 * opening one is not necessarily the end of the value.  When an owner
 * field follows it the filename ends at the last matching boundary,
 * otherwise at the last quote on the line.
 */
static const char *
nfs4_filename_end(const char *start)
{
    const char *p;

    if ((p = nfs4_strrstr(start, "\", owner: ")) != NULL)
	return p;
    return strrchr(start, '"');
}

int
refresh_proc_fs_nfsd(proc_fs_nfsd_t *proc_fs_nfsd)
{
    static int err_reported;
    char buf[MAXPATHLEN];
    FILE *statsp = NULL;
    FILE *threadsp = NULL;

    memset(proc_fs_nfsd, 0, sizeof(proc_fs_nfsd_t));

    if ((threadsp = linux_statsfile("/proc/fs/nfsd/pool_threads",
				    buf, sizeof(buf))) == NULL) {
	proc_fs_nfsd->errcode = -oserror();
	if (pmDebugOptions.libpmda) {
	    if (err_reported == 0)
		fprintf(stderr, "Warning: nfsd thread metrics are not available : %s\n",
			osstrerror());
	}
    }
    else {
	proc_fs_nfsd->errcode = 0;
	if (fscanf(threadsp,  "%d", &proc_fs_nfsd->th_cnt) != 1)
	    proc_fs_nfsd->errcode = PM_ERR_VALUE;

	/* pool_stats is only valid if nfsd is running */
	if (proc_fs_nfsd->th_cnt > 0) {
	    if ((statsp = linux_statsfile("/proc/fs/nfsd/pool_stats",
					    buf, sizeof(buf))) == NULL) {
	        proc_fs_nfsd->errcode = -oserror();
	        if (err_reported == 0)
	            fprintf(stderr, "Error: missing pool_stats when thread count != 0 : %s\n",
	  	            osstrerror());
            } else {
	    	unsigned int poolid;
	    	unsigned long arrived, enqueued, woken, timedout;
	    	int ret;

		/* first line is headers, read and discard */
	    	if (fscanf(statsp, "#%*[^\n]\n") != 0)
			fprintf(stderr, "Error: parsing /proc/fs/nfsd/pool_stats headers: %s\n",
				osstrerror());

	    	/* default is one pool, but there might be more.
	     	 * aggregate all the pool stats */
	    	while ((ret=fscanf(statsp,  "%u %lu %lu %lu %lu",
			&poolid, &arrived, &enqueued,
			&woken, &timedout)) == 5) {

		    /* poolid is not important, just count them */
		    proc_fs_nfsd->pool_cnt++;
		    proc_fs_nfsd->pkts_arrived += arrived;
		    proc_fs_nfsd->sock_enqueued += enqueued;
		    proc_fs_nfsd->th_woken += woken;
		    proc_fs_nfsd->th_timedout += timedout;
	    	}
	    	if (proc_fs_nfsd->pool_cnt < 1)
	            proc_fs_nfsd->errcode = PM_ERR_VALUE;
	     }
	}
    }

    if (pmDebugOptions.libpmda) {
	if (proc_fs_nfsd->errcode == 0)
	    fprintf(stderr, "refresh_proc_fs_nfsd: found nfsd thread metrics\n");
	else
	    fprintf(stderr, "refresh_proc_fs_nfsd: botch! missing nfsd thread metrics\n");
    }

    if (threadsp)
	fclose(threadsp);
    if (statsp)
	fclose(statsp);

    if (proc_fs_nfsd->errcode < 0) {
	if (!err_reported)
	    err_reported = 1;
	return -1;
    }

    return 0;
}

int
nfs4_svr_client_fetch(int item, nfs4_svr_client_t *nfs4_svr_client, pmAtomValue *atom)
{
    switch (item) {
        case NFS4_CLIENT_CLIENT_ID:
            atom->cp = nfs4_svr_client->client_id;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_ADDR:
            atom->cp = nfs4_svr_client->client_addr;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_STATUS:
            atom->cp = nfs4_svr_client->status;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_HOSTNAME:
            atom->cp = nfs4_svr_client->hostname;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_CALLBACK_STATE:
            atom->cp = nfs4_svr_client->callback_state;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_CALLBACK_ADDR:
            atom->cp = nfs4_svr_client->callback_addr;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_ADMIN_REVOKED_STATES:
            atom->ull = nfs4_svr_client->admin_revoked_states;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_SESSION_SLOTS:
            atom->ull = nfs4_svr_client->session_slots;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_SESSION_TARGET_SLOTS:
            atom->ull = nfs4_svr_client->session_target_slots;
            return PMDA_FETCH_STATIC;

        default:
            return PM_ERR_PMID;
    }
}

int
nfs4_svr_client_opens_fetch(int item, nfs4_svr_open_t *nfs4_svr_open, pmAtomValue *atom)
{
    switch (item) {
        case NFS4_CLIENT_OPENS_INODE:
            atom->ull = nfs4_svr_open->inode;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_OPENS_TYPE:
            atom->cp = nfs4_svr_open->type;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_OPENS_ACCESS:
            atom->cp = nfs4_svr_open->access;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_OPENS_FILENAME:
            atom->cp = nfs4_svr_open->filename;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_OPENS_CLIENT_ID:
            atom->cp = nfs4_svr_open->client_id;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_OPENS_CLIENT_ADDR:
            atom->cp = nfs4_svr_open->client_addr;
            return PMDA_FETCH_STATIC;

        case NFS4_CLIENT_OPENS_CLIENT_HOSTNAME:
            atom->cp = nfs4_svr_open->client_hostname;
            return PMDA_FETCH_STATIC;

        default:
            return PM_ERR_PMID;
    }
}

int
refresh_nfs4_svr_client(pmInDom indom)
{
    int i, count, nfsd_client_status;
    struct dirent **files = {0};
    char buf[MAXPATHLEN];
    char path[MAXPATHLEN];
    FILE *infop = NULL;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* update indom cache based on scan of /proc/fs/nfsd/clients/ */
    pmsprintf(path, sizeof(path), "%s/proc/fs/nfsd/clients/", linux_statspath);

    count = scandir(path, &files, NULL, NULL);
    if (count < 0) {
        if (oserror() == EPERM)
            nfsd_client_status = PM_ERR_PERMISSION;
        else if (oserror() == ENOENT)
            nfsd_client_status = PM_ERR_AGAIN; /* no current clients */
        else
            nfsd_client_status = PM_ERR_APPVERSION;
    } else {
        nfsd_client_status = 0; /* there many be some stats*/
    }

    for (i = 0; i < count; i++) {

        if (files[i]->d_name[0] == '.')
            continue;

        pmsprintf(path, sizeof(path), "/proc/fs/nfsd/clients/%s/info", files[i]->d_name);
        if ((infop = linux_statsfile(path, buf, sizeof(buf))) == NULL) {
	    nfsd_client_status = -oserror();
	    if (pmDebugOptions.libpmda) {
	        if (nfsd_client_status == 0)
		    fprintf(stderr, "Warning: nfsd client info metrics are not available : %s\n",
			    osstrerror());
	    }
	    continue;
        }

        nfs4_svr_client_t *nfs4_svr_client;
        nfs4_svr_client = calloc(1, sizeof(nfs4_svr_client_t));
        if (nfs4_svr_client == NULL) {
            nfsd_client_status = PM_ERR_AGAIN;
            fclose(infop);
            infop = NULL;
            break;
        }

        while (fgets(buf, sizeof(buf), infop) != NULL) {

            if (strncmp(buf, "clientid:", 9) == 0)
                sscanf(buf, "clientid: %18s", nfs4_svr_client->client_id);

            if (strncmp(buf, "address:", 8) == 0)
                sscanf(buf, "address: \"%51[^\"]", nfs4_svr_client->client_addr);

            if (strncmp(buf, "status:", 7) == 0)
                sscanf(buf, "status: %15s", nfs4_svr_client->status);

            if (strncmp(buf, "name:", 5) == 0)
                nfs4_client_hostname(buf, nfs4_svr_client->hostname,
                                     sizeof(nfs4_svr_client->hostname));

            if (strncmp(buf, "callback state:", 15) == 0)
                sscanf(buf, "callback state: %15s", nfs4_svr_client->callback_state);

            if (strncmp(buf, "callback address:", 17) == 0)
                sscanf(buf, "callback address: \"%51[^\"]", nfs4_svr_client->callback_addr);

            if (strncmp(buf, "admin-revoked states:", 21) == 0)
                sscanf(buf, "admin-revoked states: %llu", &nfs4_svr_client->admin_revoked_states);

            if (strncmp(buf, "session slots:", 14) == 0)
                sscanf(buf, "session slots: %llu", &nfs4_svr_client->session_slots);

            if (strncmp(buf, "session target slots:", 21) == 0)
                sscanf(buf, "session target slots: %llu", &nfs4_svr_client->session_target_slots);
        }
        fclose(infop);
        infop = NULL;

        const char *name = NULL;
        if (nfs4_svr_client->client_id[0] != '\0') {
            name = nfs4_svr_client->client_id;
        } else {
           free(nfs4_svr_client);
           continue; /* haven't got a client id for instance name, move onto to next */
        }
        pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)nfs4_svr_client);
    }

    for (i = 0; i < count; i++)
        free(files[i]);
    free(files);

    return nfsd_client_status;
}

int
refresh_nfs4_svr_client_opens(pmInDom indom)
{
    int i, count, nfsd_client_status;
    struct dirent **files = {0};
    char buf[MAXPATHLEN];
    char buf2[MAXPATHLEN];
    char path[MAXPATHLEN];
    FILE *infop = NULL;
    FILE *statesp = NULL;
    char *p;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* update indom cache based on scan of /proc/fs/nfsd/clients/ */
    pmsprintf(path, sizeof(path), "%s/proc/fs/nfsd/clients/", linux_statspath);

    count = scandir(path, &files, NULL, NULL);
    if (count < 0) {
        if (oserror() == EPERM)
            nfsd_client_status = PM_ERR_PERMISSION;
        else if (oserror() == ENOENT)
            nfsd_client_status = PM_ERR_AGAIN; /* no current clients */
        else
            nfsd_client_status = PM_ERR_APPVERSION;
    } else {
        nfsd_client_status = 0; /* there many be some stats*/
    }

    for (i = 0; i < count; i++) {

        if (files[i]->d_name[0] == '.')
            continue;

        pmsprintf(path, sizeof(path), "/proc/fs/nfsd/clients/%s/states", files[i]->d_name);

        if ((statesp = linux_statsfile(path, buf, sizeof(buf))) == NULL) {
	    nfsd_client_status = -oserror();
	    if (pmDebugOptions.libpmda) {
	        if (nfsd_client_status == 0)
		    fprintf(stderr, "Warning: nfsd client open metrics are not available : %s\n",
			    osstrerror());
	    }
	    continue;	/* client may have gone away */
        }

        while (fgets(buf, sizeof(buf), statesp) != NULL) {

            nfs4_svr_open_t *nfs4_svr_open;
            char *filenamefield;

            /* Only state description lines are of interest. */
            if ((p = strstr(buf, "{ type: ")) == NULL)
                continue;

            nfs4_svr_open = calloc(1, sizeof(nfs4_svr_open_t));
            if (nfs4_svr_open == NULL) {
                nfsd_client_status = PM_ERR_AGAIN;
                break;
            }

            /*
             * Allocate stats from /proc/fs/nfsd/clients/<client>/states.
             * The kernel output has changed over time (e.g. a "deny:"
             * field was added, and the device major/minor are printed
             * in hex), so decode each field separately rather than
             * depending on the exact layout of the whole line.
             */
            if (sscanf(p + 8, "%6[^,}]", nfs4_svr_open->type) != 1) {
                free(nfs4_svr_open);
                continue;
            }

            filenamefield = strstr(buf, "filename: \"");

            /*
             * Lock states carry no access field, so only accept one
             * found before the filename (whose value may itself contain
             * the text "access: ").
             */
            if ((p = strstr(buf, "access: ")) != NULL &&
                (filenamefield == NULL || p < filenamefield)) {
                sscanf(p + 8, "%15[^,}]", nfs4_svr_open->access);
                nfs4_strip_access_placeholders(nfs4_svr_open->access);
            }

            /* superblock: "<major>:<minor>:<inode>" */
            if ((p = strstr(buf, "superblock: \"")) != NULL)
                sscanf(p + 13, "%*x:%*x:%llu", &nfs4_svr_open->inode);

            /* filename: "<path>" - report only the last path component */
            if (filenamefield != NULL) {
                const char *end, *base, *q;

                p = filenamefield + 11;
                if ((end = nfs4_filename_end(p)) != NULL) {
                    base = p;
                    for (q = p; q < end; q++) {
                        if (*q == '/')
                            base = q + 1;
                    }
                    pmsprintf(nfs4_svr_open->filename,
                              sizeof(nfs4_svr_open->filename),
                              "%.*s", (int)(end - base), base);
                }
            }

            if (nfs4_svr_open->inode == 0) {
                free(nfs4_svr_open);
                continue; /* dont add an instance if we cannt get an inode number */
            }

            /* Allocate stats from /proc/fs/nfsd/clients/<client>/info */
            pmsprintf(path, sizeof(path), "/proc/fs/nfsd/clients/%s/info", files[i]->d_name);

            if ((infop = linux_statsfile(path, buf2, sizeof(buf2))) == NULL)  {
	        nfsd_client_status = -oserror();
	        if (pmDebugOptions.libpmda) {
	            if (nfsd_client_status == 0)
		        fprintf(stderr, "Warning: nfsd client open metrics are not available : %s\n",
			    osstrerror());
	        }
                free(nfs4_svr_open);
                continue;
            }

            while (fgets(buf2, sizeof(buf2), infop) != NULL) {
                if (strncmp(buf2, "clientid:", 9) == 0)
                    sscanf(buf2, "clientid: %18s", nfs4_svr_open->client_id);

                if (strncmp(buf2, "address:", 8) == 0)
                    sscanf(buf2, "address: \"%51[^\"]", nfs4_svr_open->client_addr);

                if (strncmp(buf2, "name:", 5) == 0)
                    nfs4_client_hostname(buf2, nfs4_svr_open->client_hostname,
                                         sizeof(nfs4_svr_open->client_hostname));
            }
            fclose(infop);
            infop = NULL;

            pmsprintf(path, sizeof(path), "%llu:%s",
                nfs4_svr_open->inode,
                nfs4_svr_open->client_id
            );

            pmdaCacheStore(indom, PMDA_CACHE_ADD, path, (void *)nfs4_svr_open);
	}
	fclose(statesp);
	statesp = NULL;
    }

    for (i = 0; i < count; i++)
        free(files[i]);
    free(files);

    return nfsd_client_status;
}
