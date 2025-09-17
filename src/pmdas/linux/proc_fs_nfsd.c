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

char *stats_path = "";

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

    char	*envpath;
    char buf[MAXPATHLEN];
    char path[MAXPATHLEN];
    FILE *infop = NULL;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* update indom cache based on scan of /proc/fs/nfsd/clients/ */
    if ((envpath = getenv("LINUX_STATSPATH")) != NULL)
	stats_path = envpath;

    pmsprintf(path, sizeof(path), "%s/proc/fs/nfsd/clients/", stats_path);
    
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
        }

        nfs4_svr_client_t *nfs4_svr_client;
        
        nfs4_svr_client = calloc(1, sizeof(nfs4_svr_client_t));
        if (nfs4_svr_client == NULL) {
           nfsd_client_status = PM_ERR_AGAIN;
           break;
        }
            
        while (fgets(buf, sizeof(buf)-1, infop) != NULL) {

            if (strncmp(buf, "clientid:", 9) == 0)
                sscanf(buf, "clientid: %s", nfs4_svr_client->client_id);

            if (strncmp(buf, "address:", 8) == 0) {
                sscanf(buf, "address: \"%s", nfs4_svr_client->client_addr);
                /* Remove trailing " character */
                nfs4_svr_client->client_addr[strlen(nfs4_svr_client->client_addr)-1] = '\0';
            }

            if (strncmp(buf, "status:", 7) == 0)
                sscanf(buf, "status: %s", nfs4_svr_client->status);

            if (strncmp(buf, "name:", 5) == 0) {
                sscanf(buf, "name: %*s %*s %s", nfs4_svr_client->hostname);
                /* Remove trailing " character */
                nfs4_svr_client->hostname[strlen(nfs4_svr_client->hostname)-1] = '\0';
            }

            if (strncmp(buf, "callback state:", 15) == 0)
                sscanf(buf, "callback state: %s", nfs4_svr_client->callback_state);

            if (strncmp(buf, "callback address:", 17) == 0) {
                sscanf(buf, "callback address: \"%s", nfs4_svr_client->callback_addr);
                /* Remove trailing " character */
                nfs4_svr_client->callback_addr[strlen(nfs4_svr_client->callback_addr)-1] = '\0';
            }

            if (strncmp(buf, "admin-revoked states:", 21) == 0)
                sscanf(buf, "admin-revoked states: %llu", &nfs4_svr_client->admin_revoked_states);

            if (strncmp(buf, "session slots:", 14) == 0)
                sscanf(buf, "session slots: %llu", &nfs4_svr_client->session_slots);

            if (strncmp(buf, "session target slots:", 21) == 0)
                sscanf(buf, "session target slots: %llu", &nfs4_svr_client->session_target_slots);
        }
        fclose(infop);

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

    char	*envpath;
    char buf[MAXPATHLEN];
    char buf2[MAXPATHLEN];
    char path[MAXPATHLEN];
    char filename[MAXPATHLEN];
    char inst_name[MAXPATHLEN];
    
    FILE *infop = NULL;
    FILE *statesp = NULL;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* update indom cache based on scan of /proc/fs/nfsd/clients/ */
    if ((envpath = getenv("LINUX_STATSPATH")) != NULL)
	stats_path = envpath;

    pmsprintf(path, sizeof(path), "%s/proc/fs/nfsd/clients/", stats_path);

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

        if (files[i]->d_name[0] == '.') {
            continue;
        }

        pmsprintf(path, sizeof(path), "/proc/fs/nfsd/clients/%s/states", files[i]->d_name);

        if ((statesp = linux_statsfile(path, buf, sizeof(buf))) == NULL) {
	    nfsd_client_status = -oserror();
	    if (pmDebugOptions.libpmda) {
	        if (nfsd_client_status == 0)
		    fprintf(stderr, "Warning: nfsd client open metrics are not available : %s\n",
			    osstrerror());	    
	    }
        }

        while (fgets(buf, sizeof(buf)-1, statesp) != NULL) {

            nfs4_svr_open_t *nfs4_svr_open;

            nfs4_svr_open = calloc(1, sizeof(nfs4_svr_open_t));
            if (nfs4_svr_open == NULL) {
                nfsd_client_status = PM_ERR_AGAIN;
                break;
            }

            /* Allocate stats from /proc/fs/nfsd/clients/<client>/states */
            sscanf(buf, "- %*x: { type: %s access: %s superblock: \"fc:%*d:%llu\", filename: \"%s", 
                nfs4_svr_open->type,
                nfs4_svr_open->access,
                &nfs4_svr_open->inode,
                filename
            );

            /* Remove trailing , character */
            nfs4_svr_open->type[strlen(nfs4_svr_open->type)-1] = '\0';
            nfs4_svr_open->access[strlen(nfs4_svr_open->access)-1] = '\0';

            /* Extract filename (last token of path) */
            char *ptr;
            ptr = strrchr(filename, '/') + 1;
            sscanf(ptr, "%s", nfs4_svr_open->filename);
            nfs4_svr_open->filename[strlen(nfs4_svr_open->filename)-1] = '\0';

            /* Allocate stats from /proc/fs/nfsd/clients/<client>/info */
            pmsprintf(path, sizeof(path), "/proc/fs/nfsd/clients/%s/info", files[i]->d_name);

            if ((infop = linux_statsfile(path, buf2, sizeof(buf2))) == NULL)  {
	        nfsd_client_status = -oserror();
	        if (pmDebugOptions.libpmda) {
	            if (nfsd_client_status == 0)
		        fprintf(stderr, "Warning: nfsd client open metrics are not available : %s\n",
			    osstrerror());	    
	        }
            }

            while (fgets(buf2, sizeof(buf2)-1, infop) != NULL) {
                if (strncmp(buf2, "clientid:", 9) == 0)
                    sscanf(buf2, "clientid: %s", nfs4_svr_open->client_id);

                if (strncmp(buf2, "address:", 8) == 0) {
                    sscanf(buf2, "address: \"%s", nfs4_svr_open->client_addr);
                    /* Remove trailing " character */
                    nfs4_svr_open->client_addr[strlen(nfs4_svr_open->client_addr)-1] = '\0';
                }

                if (strncmp(buf2, "name:", 5) == 0) {
                    sscanf(buf2, "name: %*s %*s %s ", nfs4_svr_open->client_hostname);
                    /* Remove trailing " character */
                    nfs4_svr_open->client_hostname[strlen(nfs4_svr_open->client_hostname)-1] = '\0';
                }
            }
            fclose(infop);

            if (nfs4_svr_open->inode == 0) {
                free(nfs4_svr_open);
                continue; /* dont add an instance if we cannt get an inode number */
            }

            pmsprintf(inst_name, sizeof(inst_name), "%llu:%s",
                nfs4_svr_open->inode,
                nfs4_svr_open->client_id
            );

            pmdaCacheStore(indom, PMDA_CACHE_ADD, inst_name, (void *)nfs4_svr_open);
	}
	fclose(statesp);

    }
    
    for (i = 0; i < count; i++)
        free(files[i]);

    free(files);

    return nfsd_client_status;
}
