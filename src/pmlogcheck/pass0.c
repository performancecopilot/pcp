/*
 * Copyright (c) 2017,2021 Red Hat.
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
#include "libpcp.h"
#include "logcheck.h"
#include "../libpcp/src/internal.h"

int		goldenmagic;
char * 		goldenfname;
__pmTimestamp	goldenstart;

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
checklabel(__pmFILE *f, char *fname, int len)
{
    __pmTimestamp	start = {0};
    size_t		bytes;
    long		offset = __pmFtell(f);
    int			magic;
    int			sts = STS_OK;

    /* first read the magic number for sanity and version checking */
    __pmFseek(f, sizeof(int), SEEK_SET);
    if ((bytes = __pmFread(&magic, 1, sizeof(magic), f)) != sizeof(magic)) {
	fprintf(stderr, "checklabel(...,%s): botch: magic read returns %zu not %zu as expected\n", fname, bytes, sizeof(magic));
	sts = STS_FATAL;
    }
    __pmFseek(f, sizeof(int), SEEK_SET);

    magic = ntohl(magic);
    if ((magic & 0xffffff00) != PM_LOG_MAGIC) {
	fprintf(stderr, "%s: bad label magic number: 0x%x not 0x%x as expected\n",
	    fname, magic & 0xffffff00, PM_LOG_MAGIC);
	sts = STS_FATAL;
    }
    if ((magic & 0xff) != PM_LOG_VERS02 &&
        (magic & 0xff) != PM_LOG_VERS03) {
	fprintf(stderr, "%s: bad label version: %d not %d or %d as expected\n",
	    fname, magic & 0xff, PM_LOG_VERS02, PM_LOG_VERS03);
	sts = STS_FATAL;
    }

    /* now check version-specific label information - keep start time */
    if ((magic & 0xff) >= PM_LOG_VERS03) {
	__pmExtLabel_v3	label3;

	if (len <= sizeof(label3)) {
	    fprintf(stderr, "%s: bad label length: %d not >%zu as expected\n",
		    fname, len, sizeof(label3));
	    sts = STS_FATAL;
	} else {
	    /* read just the fixed size part of the label for now */
	    bytes = __pmFread(&label3, 1, sizeof(label3), f);
	    if (bytes != sizeof(label3)) {
		fprintf(stderr, "checklabel(...,%s): botch: read returns %zu not %zu as expected\n", fname, bytes, sizeof(label3));
		sts = STS_FATAL;
	    } else {
		/* TODO: add v3-specific checks */
		start.sec = label3.start_sec;
		start.nsec = label3.start_nsec;
		__ntohpmTimestamp(&start);
	    }
	}
    } else {	/* PM_LOG_VERS02 */
	__pmExtLabel_v2	label2;

	if (len != sizeof(label2)) {
	    fprintf(stderr, "%s: bad label length: %d not %zu as expected\n",
		    fname, len, sizeof(label2));
	    sts = STS_FATAL;
	} else {
	    bytes = __pmFread(&label2, 1, sizeof(label2), f);
	    if (bytes != sizeof(label2)) {
		fprintf(stderr, "checklabel(...,%s): botch: read returns %zu not %zu as expected\n", fname, bytes, sizeof(label2));
		sts = STS_FATAL;
	    } else {
		/* TODO: add v2-specific checks */
		start.sec = ntohl(label2.start_sec);
		start.nsec = ntohl(label2.start_usec) * 1000;
	    }
	}
    }

    if (goldenmagic == 0) {
	if (sts == STS_OK) {
	    /* first good label */
	    goldenfname = strdup(fname);
	    goldenmagic = magic;
	    goldenstart = start;
	}
    } else if ((magic & 0xff) != (goldenmagic & 0xff)) {
	fprintf(stderr, "%s: mismatched label version: %d not %d as expected from %s\n",
			    fname, magic & 0xff, magic & 0xff, goldenfname);
	sts = STS_FATAL;
    }
    __pmFseek(f, offset, SEEK_SET);
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
    __pmFILE	*f = NULL;
    int		label_ok = STS_OK;
    char	logBase[MAXPATHLEN];

    if ((f = __pmFopen(fname, "r")) == NULL) {
	fprintf(stderr, "%s: cannot open file: %s\n", fname, osstrerror());
	sts = STS_FATAL;
	goto done;
    }
    
    strncpy(logBase, fname, sizeof(logBase));
    logBase[sizeof(logBase)-1] = '\0';
    if (__pmLogBaseName(logBase) != NULL) {
	/* A valid archive suffix was found */
	p = logBase + strlen(logBase) + 1;
	if (strcmp(p, "index") == 0)
	    is = IS_INDEX;
	else if (strcmp(p, "meta") == 0)
	    is = IS_META;
	else if (isdigit((int)(*p))) {
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

    if (vflag)
	fprintf(stderr, "%s: start pass0 ... ", fname);

    while ((sts = __pmFread(&len, 1, sizeof(len), f)) == sizeof(len)) {
	len = ntohl(len);
	len -= 2 * sizeof(len);
	/* gobble stuff between header and trailer without looking at it */
	for (i = 0; i < len; i++) {
	    check = __pmFgetc(f);
	    if (check == EOF) {
		if (vflag)
		    fputc('\n', stderr);
		if (nrec == 0)
		    fprintf(stderr, "%s: unexpected EOF in label record body, wanted %d, got %d bytes\n", fname, len, i);
		else
		    fprintf(stderr, "%s[record %d]: unexpected EOF in record body, wanted %d, got %d bytes\n", fname, nrec, len, i);
		sts = STS_FATAL;
		goto done;
	    }
	}
	if ((sts = __pmFread(&check, 1, sizeof(check), f)) != sizeof(check)) {
	    if (vflag)
		fputc('\n', stderr);
	    if (nrec == 0)
		fprintf(stderr, "%s: unexpected EOF in label record trailer, wanted %d, got %d bytes\n", fname, (int)sizeof(check), sts);
	    else
		fprintf(stderr, "%s[record %d]: unexpected EOF in record trailer, wanted %d, got %d bytes\n", fname, nrec, (int)sizeof(check), sts);
	    sts = STS_FATAL;
	    goto done;
	}
	check = ntohl(check);
	len += 2 * sizeof(len);
	if (check != len) {
	    if (vflag)
		fputc('\n', stderr);
	    if (nrec == 0)
		fprintf(stderr, "%s: label record length mismatch: header %d != trailer %d\n", fname, len, check);
	    else
		fprintf(stderr, "%s[record %d]: length mismatch: header %d != trailer %d\n", fname, nrec, len, check);
	    sts = STS_FATAL;
	    goto done;
	}

	if (nrec == 0) {
	    int		xsts;
	    xsts = checklabel(f, fname, len - 2 * sizeof(len));
	    if (label_ok == STS_OK)
		/* just remember first not OK status */
		label_ok = xsts;
	}

	nrec++;
	if (is == IS_INDEX) {
	    /* for index files, done label record, now eat index records */
	    size_t	record_size;
	    void	*buffer;

	    if ((goldenmagic & 0xff) >= PM_LOG_VERS03)
		record_size = 8*sizeof(__pmPDU);
	    else
		record_size = 5*sizeof(__pmPDU);
	    if ((buffer = (void *)malloc(record_size)) == NULL) {
		pmNoMem("pass0: index buffer", record_size, PM_FATAL_ERR);
		/* NOTREACHED */
	    }

	    while ((sts = __pmFread(buffer, 1, record_size, f)) == record_size) { 
		nrec++;
	    }
	    if (sts != 0) {
		if (vflag)
		    fputc('\n', stderr);
		fprintf(stderr, "%s[record %d]: unexpected EOF in index entry, wanted %zd, got %d bytes\n", fname, nrec, record_size, sts);
		index_state = STATE_BAD;
		sts = STS_FATAL;
		goto done;
	    }
	    goto empty_check;
	}
    }
    if (sts != 0) {
	if (vflag)
	    fputc('\n', stderr);
	fprintf(stderr, "%s[record %d]: unexpected EOF in record header, wanted %d, got %d bytes\n", fname, nrec, (int)sizeof(len), sts);
	sts = STS_FATAL;
    }
empty_check:
    if (sts != STS_FATAL && nrec < 2) {
	if (vflag)
	    fputc('\n', stderr);
	fprintf(stderr, "%s: contains no PCP data\n", fname);
	sts = STS_WARNING;
    }
    /*
     * sts == 0 (from __pmFread) => STS_OK
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
	__pmFclose(f);

    if (vflag && nrec > 0 && sts != STS_FATAL)
	fprintf(stderr, "found %d records\n", nrec);

    return sts;
}

