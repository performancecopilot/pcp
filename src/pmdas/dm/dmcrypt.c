/*
 * Device Mapper PMDA - Crypt (dm-crypt) Stats
 *
 * Copyright (c) 2025 Red Hat.
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

#include "indom.h"
#include "dmcrypt.h"

#include <inttypes.h>
#include <ctype.h>

static char *dm_setup_dmsetup;
static char *dm_setup_cryptsetup;

/* cryptsetup command output is padded with a leading tab layout,
 * trim this out from the output to make comparisons easier.
 */
char 
*strtrim(char* str) {
    // Check for empty String
    if(*str == 0)  // All spaces
        return str;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    return str;
}

int
dm_crypt_fetch(int item, struct crypt_stats *crypt_stats, pmAtomValue *atom)
{
    if (item < 0 || item >= NUM_CRYPT_STATS)
	return PM_ERR_PMID;

    switch(item) {
        case CRYPT_ACTIVE:
            atom->ul = crypt_stats->active;
            break;

        case CRYPT_TYPE:
            atom->cp = crypt_stats->type;
            break;

        case CRYPT_CIPHER:
            atom->cp = crypt_stats->cipher;
            break;

        case CRYPT_KEYSIZE:
            atom->ul = crypt_stats->keysize;
            break;

        case CRYPT_KEY_LOCATION:
            atom->cp = crypt_stats->key_location;
            break;

        case CRYPT_DEVICE:
            atom->cp = crypt_stats->device;
            break;

        case CRYPT_SECTOR_SIZE:
            atom->ul = crypt_stats->sector_size;
            break;

        case CRYPT_OFFSET:
            atom->ull = crypt_stats->offset;
            break;

        case CRYPT_OFFSET_BYTES:
            atom->ull = crypt_stats->offset_bytes;
            break;

        case CRYPT_SIZE:
            atom->ull = crypt_stats->size;
            break;

        case CRYPT_SIZE_BYTES:
            atom->ull = crypt_stats->size_bytes;
            break;

        case CRYPT_MODE:
            atom->cp = crypt_stats->mode;
            break;

        case CRYPT_FLAGS:
            atom->cp = crypt_stats->flags;
            break;
    }
    return PMDA_FETCH_STATIC;
}

/*
 * Grab output from cryptsetup status (or read in from cat when under QA),
 * Match the data to the crypt vol which we wish to update the metrics and
 * assign the values to crypt_stats.
 */
int
dm_refresh_crypt(const char *name, struct crypt_stats *crypt_stats)
{
    char buffer[BUFSIZ];
    FILE *fp;
    int sts;
    __pmExecCtl_t *argp = NULL;

    pmsprintf(buffer, sizeof(buffer), "%s %s", dm_setup_cryptsetup, name);

    if ((sts = __pmProcessUnpickArgs(&argp, buffer)) < 0)
	return sts;
    if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_NONE, &fp)) < 0)
	return sts;

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (strstr(buffer, name)) {
            if (strstr(buffer, "active") != NULL) {
                crypt_stats->active = 1;
            } else {
                crypt_stats->active = 0;
            }
        }
        
        char *trimmed = strtrim(buffer);

        if (strncmp(trimmed, "type:", 5) == 0)
            sscanf(buffer, "%*s %s", crypt_stats->type);

        if (strncmp(trimmed, "cipher:", 7) == 0)
            sscanf(buffer, "%*s %s", crypt_stats->cipher);

        if (strncmp(trimmed, "keysize:", 8) == 0)
            sscanf(buffer, "%*s %"SCNu32"", &crypt_stats->keysize);

        if (strncmp(trimmed, "key location:", 13) == 0)
            sscanf(buffer, "%*s %*s %s", crypt_stats->key_location);

        if (strncmp(trimmed, "device:", 7) == 0)
            sscanf(buffer, "%*s %s", crypt_stats->device);

        if (strncmp(trimmed, "sector size:", 12) == 0)
            sscanf(buffer, "%*s %*s %"SCNu32"", &crypt_stats->sector_size);

        if (strncmp(trimmed, "offset:", 7) == 0)
            sscanf(buffer, "%*s %"SCNu64"", &crypt_stats->offset);

        if (strncmp(trimmed, "size:", 5) == 0)
            sscanf(buffer, "%*s %"SCNu64"", &crypt_stats->size);

        if (strncmp(trimmed, "mode:", 5) == 0)
            sscanf(buffer, "%*s %s", crypt_stats->mode);
    
        if (strncmp(trimmed, "flags:", 6) == 0)
            sscanf(buffer, "%*s %s", crypt_stats->flags);
            
        if ((crypt_stats->size != 0) && (crypt_stats->sector_size !=0))
            crypt_stats->size_bytes = crypt_stats->size * crypt_stats->sector_size;

        if ((crypt_stats->offset != 0) && (crypt_stats->sector_size !=0))
            crypt_stats->offset_bytes = crypt_stats->offset * crypt_stats->sector_size;

    }

    sts = __pmProcessPipeClose(fp);
    if (sts <= 0)
        return sts;
    if (sts == 2000)
	pmNotifyErr(LOG_ERR, "dm_refresh_crypt: pipe (%s %s) terminated with unknown error\n", dm_setup_cryptsetup, name);
    else if (sts > 1000)
	pmNotifyErr(LOG_ERR, "dm_refresh_crypt: pipe (%s %s) terminated with signal %d\n", dm_setup_cryptsetup, name, sts - 1000);
    else
	pmNotifyErr(LOG_ERR, "dm_refresh_crypt: pipe (%s %s) terminated with exit status %d\n", dm_setup_cryptsetup, name, sts);

    return PM_ERR_GENERIC;
}

/*
 * Update the dm crypt instance domain. This will change as volumes are created, 
 * activated and removed.
 *
 * Using the pmdaCache interfaces simplifies things and provides us
 * with guarantees around consistent instance numbering in all of
 * those interesting corner cases.
 */
int
dm_crypt_instance_refresh(void)
{
    int sts;
    FILE *fp;
    char buffer[BUFSIZ];
    struct crypt_stats *crypt;
    pmInDom indom = dm_indom(DM_CRYPT_INDOM);
    __pmExecCtl_t *argp = NULL;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /*
     * update indom cache based off of thin pools listed by dmsetup
     */
    if ((sts = __pmProcessUnpickArgs(&argp, dm_setup_dmsetup)) < 0)
	return sts;
    if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_NONE, &fp)) < 0)
	return sts;

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (strstr(buffer, "crypt")) {
            strtok(buffer, ":");

            /*
             * at this point buffer contains our crypt device lvm names
             * this will be used to map stats to file-system instances
             */
	    sts = pmdaCacheLookupName(indom, buffer, NULL, (void **)&crypt);
	    if (sts == PM_ERR_INST || (sts >= 0 && crypt == NULL)){
	        crypt = calloc(1, sizeof(*crypt));
                if (crypt == NULL) {
		    __pmProcessPipeClose(fp);
                    return PM_ERR_AGAIN;
                }
            }
	    else if (sts < 0)
	        continue;

	    /* (re)activate this entry for the current query */
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, (void *)crypt);
        }
    }

    sts = __pmProcessPipeClose(fp);
    if (sts <= 0)
        return sts;
    if (sts == 2000)
	pmNotifyErr(LOG_ERR, "dm_crypt_instance_refresh: pipe (%s) terminated with unknown error\n", dm_setup_dmsetup);
    else if (sts > 1000)
	pmNotifyErr(LOG_ERR, "dm_crypt_instance_refresh: pipe (%s) terminated with signal %d\n", dm_setup_dmsetup, sts - 1000);
    else
	pmNotifyErr(LOG_ERR, "dm_crypt_instance_refresh: pipe (%s) terminated with exit status %d\n", dm_setup_dmsetup, sts);

    return PM_ERR_GENERIC;            
}

void
dm_crypt_setup(void)
{
    static char dmsetup_command[] = "dmsetup status --target crypt";
    static char cryptsetup_command[] = "cryptsetup status ";
    char *env_command;

    /* allow override at startup for QA testing */
    if ((env_command = getenv("DM_SETUP_CRYPT")) != NULL)
        dm_setup_dmsetup = env_command;
    else
        dm_setup_dmsetup = dmsetup_command;

    if ((env_command = getenv("DM_SETUP_CRYPTSETUP")) != NULL)
        dm_setup_cryptsetup = env_command;
    else
        dm_setup_cryptsetup = cryptsetup_command;
}
