/*
 * Copyright (c) 2013 Ken McDonell, Inc.  All Rights Reserved.
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

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "logcheck.h"

#define IS_UNKNOWN	0
#define IS_INDEX	1
#define IS_META		2
#define IS_LOG		3
/*
 * Pass 0 for all files
 * - should only come here if fname exists
 * - check that file contains a number of complete records
 * - the label record and the records for the data and metadata
 *   have this format:
 *   :----------:----------------------:---------:
 *   | int len  |        stuff         | int len |
 *   | header   |        stuff         | trailer |
 *   :----------:----------------------:---------:
 *   and the len fields are in network byte order.
 *   For these records, check that the header length is equal to
 *   the trailer length
 * - for index files, following the label record there should be
 *   a number of complete records, each of which is a __pmLogTI
 *   record, with the fields converted network byte order
 *
 * TODO - repair
 * - truncate metadata and data files ... unconditional or interactive confirm?
 * - mark index as bad and needing rebuild
 * - move access check into here (if cannot open file for reading we're screwed)
 */

/*
 * Already checked len in header and trailer, so just read label
 * directly.
 * If not first file, check label consistency.
 * Checks here mimic those in __pmLogChkLabel().
 */
static int
checklabel(FILE *f, char *fname)
{
    static char 	*goldenfname = NULL;
    __pmLogLabel	label;
    long		offset = ftell(f);
    int			sts;

    fseek(f, sizeof(int), SEEK_SET);
    if ((sts = fread(&label, 1, sizeof(label), f)) != sizeof(label)) {
	fprintf(stderr, "checklabel(...,%s): botch: read returns %d not %d as expected\n", fname, sts, (int)sizeof(label));
	exit(1);
    }
    sts = STS_OK;
    label.ill_magic = ntohl(label.ill_magic);
    label.ill_pid = ntohl(label.ill_pid);
    label.ill_start.tv_sec = ntohl(label.ill_start.tv_sec);
    label.ill_start.tv_usec = ntohl(label.ill_start.tv_usec);
    label.ill_vol = ntohl(label.ill_vol);
    if ((label.ill_magic & 0xffffff00) != PM_LOG_MAGIC) {
	fprintf(stderr, "%s: bad label magic number: 0x%x not 0x%x as expected\n",
	    fname, label.ill_magic & 0xffffff00, PM_LOG_MAGIC);
	sts = STS_FATAL;
    }
    if ((label.ill_magic & 0xff) != PM_LOG_VERS02) {
	fprintf(stderr, "%s: bad label version: %d not %d as expected\n",
	    fname, label.ill_magic & 0xff, PM_LOG_VERS02);
	sts = STS_FATAL;
    }
    if (log_label.ill_start.tv_sec == 0) {
	if (sts == STS_OK) {
	    /* first good label */
	    goldenfname = strdup(fname);
	    memcpy(&log_label, &label, sizeof(log_label));
	}
    }
    else {
	if ((label.ill_magic & 0xff) != (log_label.ill_magic & 0xff)) {
	    fprintf(stderr, "%s: mismatched label version: %d not %d as expected from %s\n",
			    fname, label.ill_magic & 0xff, log_label.ill_magic & 0xff, goldenfname);
	    sts = STS_FATAL;
	}
#if 0
	if (label.ill_pid != log_label.ill_pid) {
	    fprintf(stderr, "Mismatched PID (%d/%d) between %s and %s\n",
			    label.ill_pid, log_label.ill_pid, file, goldenfname);
	    status = 2;
	}
	if (strncmp(label.ill_hostname, log_label.ill_hostname,
			PM_LOG_MAXHOSTLEN) != 0) {
	    fprintf(stderr, "Mismatched hostname (%s/%s) between %s and %s\n",
		    label.ill_hostname, log_label.ill_hostname, file, goldenfname);
	    status = 2;
	}
	if (strncmp(label.ill_tz, log_label.ill_tz, PM_TZ_MAXLEN) != 0) {
	    fprintf(stderr, "Mismatched timezone (%s/%s) between %s and %s\n",
		    label.ill_tz, log_label.ill_tz, file, goldenfname);
	    status = 2;
	}
#endif
    }
    fseek(f, offset, SEEK_SET);
    return sts; 
}

int
pass0(char *fname)
{
    int		len;
    int		check;
    int		i;
    int		sts;
    int		nrec = 0;
    int		is = IS_UNKNOWN;
    char	*p;
    FILE	*f = NULL;
    int		label_ok = STS_OK;

    if (vflag)
	fprintf(stderr, "%s: start pass0\n", fname);

    if ((f = fopen(fname, "r")) == NULL) {
	fprintf(stderr, "%s: cannot open file: %s\n", fname, osstrerror());
	sts = STS_FATAL;
	goto done;
    }
    p = strrchr(fname, '.');
    if (p != NULL) {
	if (strcmp(p, ".index") == 0)
	    is = IS_INDEX;
	else if (strcmp(p, ".meta") == 0)
	    is = IS_META;
	else if (isdigit(*++p)) {
	    p++;
	    while (*p && isdigit(*p))
		p++;
	    if (*p == '\0')
		is = IS_LOG;
	}
    }
    if (is == IS_UNKNOWN) {
	/*
	 * should never get here because filter() is supposed to
	 * only include PCP archive file names from scandir()
	 */
	fprintf(stderr, "%s: pass0 botch: bad file name?\n", fname);
	exit(1);
    }

    while ((sts = fread(&len, 1, sizeof(len), f)) == sizeof(len)) {
	len = ntohl(len);
	len -= 2 * sizeof(len);
	/* gobble stuff between header and trailer without looking at it */
	for (i = 0; i < len; i++) {
	    check = fgetc(f);
	    if (check == EOF) {
		if (nrec == 0)
		    fprintf(stderr, "%s: unexpected EOF in label record body\n", fname);
		else
		    fprintf(stderr, "%s[record %d]: unexpected EOF in record body\n", fname, nrec);
		sts = STS_FATAL;
		goto done;
	    }
	}
	if ((sts = fread(&check, 1, sizeof(check), f)) != sizeof(check)) {
	    if (nrec == 0)
		fprintf(stderr, "%s: unexpected EOF in label record trailer\n", fname);
	    else
		fprintf(stderr, "%s[record %d]: unexpected EOF in record trailer\n", fname, nrec);
	    sts = STS_FATAL;
	    goto done;
	}
	check = ntohl(check);
	len += 2 * sizeof(len);
	if (check != len) {
	    if (nrec == 0)
		fprintf(stderr, "%s: label record length mismatch: header %d != trailer %d\n", fname, len, check);
	    else
		fprintf(stderr, "%s[record %d]: length mismatch: header %d != trailer %d\n", fname, nrec, len, check);
	    sts = STS_FATAL;
	    goto done;
	}

	if (nrec == 0) {
	    int		xsts;
	    xsts = checklabel(f, fname);
	    if (label_ok == STS_OK)
		/* just remember first not OK status */
		label_ok = xsts;
	}

	nrec++;
	if (is == IS_INDEX) {
	    /* for index files, done label record, now eat index records */
	    __pmLogTI	tirec;
	    while ((sts = fread(&tirec, 1, sizeof(tirec), f)) == sizeof(tirec)) { 
		nrec++;
	    }
	    if (sts != 0) {
		fprintf(stderr, "%s[record %d]: unexpected EOF in index entry\n", fname, nrec);
		index_state = STATE_BAD;
		sts = STS_FATAL;
		goto done;
	    }
	    goto empty_check;
	}
    }
    if (sts != 0) {
	fprintf(stderr, "%s[record %d]: unexpected EOF in record header\n", fname, nrec);
	sts = STS_FATAL;
    }
empty_check:
    if (sts != STS_FATAL && nrec < 2) {
	fprintf(stderr, "%s: contains no PCP data\n", fname);
	sts = STS_WARNING;
    }
    /*
     * sts == 0 (from fread) => STS_OK
     */
done:
    if (is == IS_INDEX) {
	if (sts == STS_OK)
	    index_state = STATE_OK;
	else
	    index_state = STATE_BAD;
    }
    else if (is == IS_META) {
	if (sts == STS_OK)
	    meta_state = STATE_OK;
	else
	    meta_state = STATE_BAD;
    }
    else {
	if (log_state == STATE_OK && sts != STS_OK)
	    log_state = STATE_BAD;
	else if (log_state == STATE_MISSING) {
	    if (sts == STS_OK)
		log_state = STATE_OK;
	    else
		log_state = STATE_BAD;
	}
    }

    if (sts == STS_OK)
	sts = label_ok;

    if (f != NULL)
	fclose(f);

    return sts;
}

